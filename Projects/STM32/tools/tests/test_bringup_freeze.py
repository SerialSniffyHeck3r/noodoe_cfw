"""Offline checks of real prepare/restore commands and unchanged tick gate.

No CLI subprocess or device is used. Hardware effects require root's rerun.
"""
import ast
import inspect
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bringup


class FakeBoard(bringup.Board):
    def __init__(self, before=0x1800):
        super().__init__('NO_DEVICE', Path('.'))
        self.before = before
        self.calls = []
        self.identity = 'Device ID : 0x419\nNVM size : 512 KBytes\nRDP : 0xAA\n'

    def command(self, name, *operations, **kwargs):
        self.calls.append((name, operations))
        if name == 'target':
            return self.identity + f'0xE0042008 : {self.before:08X}\n'
        return ''


class TestDebugFreeze(unittest.TestCase):
    def test_watchdog_and_tim6_only_preserve_previous_bits(self):
        for before in (0, 0x1800, 0x1810, 0x00200428, 0xFFFFFFFF):
            with self.subTest(before=hex(before)):
                board = FakeBoard(before)
                board.prepare()
                self.assertEqual(board.freeze_before, before)
                self.assertEqual(board.calls, [
                    ('target', ('-r32', '0xE0042008', '4', '-ob', 'displ')),
                    ('watchdog-freeze', ('-w32', '0xE0042008', hex(before | 0x1810))),
                ])
                self.assertEqual((int(board.calls[-1][1][-1], 0) ^ before) & ~0x1810, 0)

    def test_bad_identity_never_changes_debug_state(self):
        for bad in ('Device ID : 0x123\n', 'NVM size : 1024 KBytes\n', 'RDP : 0xBB\n'):
            with self.subTest(bad=bad):
                board = FakeBoard()
                board.identity = bad
                with self.assertRaises(RuntimeError):
                    board.prepare()
                self.assertEqual(len(board.calls), 1)

    def test_success_restore_uses_original_word_not_new_mask(self):
        # Execute the actual, unchanged main restore expression with a fake
        # Board. This catches an accidental restore to a fixed debug default.
        tree = ast.parse(inspect.getsource(bringup.main))
        calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call)
                 and node.args and isinstance(node.args[0], ast.Constant)
                 and node.args[0].value == 'restore-debug-freeze']
        self.assertEqual(len(calls), 1)
        board = FakeBoard(0x00200428)
        board.prepare()
        eval(compile(ast.Expression(calls[0]), '<actual-restore>', 'eval'), {'board': board})
        self.assertEqual(board.calls[-1], ('restore-debug-freeze',
                         ('-w32', '0xE0042008', '0x200428', '-run')))

    def test_actual_hal_kernel_tolerance_stays_two(self):
        tree = ast.parse(inspect.getsource(bringup.main))
        checks = [node.args[0] for node in ast.walk(tree) if isinstance(node, ast.Call)
                  and isinstance(node.func, ast.Name) and node.func.id == 'require'
                  and len(node.args) > 1 and isinstance(node.args[1], ast.JoinedStr)
                  and any(isinstance(x, ast.Constant) and 'HAL/kernel tick progress diverged' in str(x.value)
                          for x in node.args[1].values)]
        self.assertEqual(len(checks), 1)
        predicate = compile(ast.Expression(checks[0]), '<actual-tick-predicate>', 'eval')
        for difference in (-3, -2, 0, 2, 3):
            actual = eval(predicate, {'delta': {'hal_tick': 10000+difference, 'kernel_tick': 10000}})
            self.assertEqual(actual, abs(difference) <= 2)


if __name__ == '__main__':
    unittest.main()
