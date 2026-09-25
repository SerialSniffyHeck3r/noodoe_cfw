"""장치 없이 graphics profile의 성공/실패 경계를 검증한다.

실측 LCD JSON은 별도의 회귀 입력으로 실행자가 전달할 수 있다. 이 테스트는
의도적으로 GPU fault, 중간 publish, FIFO busy와 stall을 다르게 취급한다.
"""
from __future__ import annotations
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from graphics_checks import (GRAPHICS_FIELDS, GRAPHICS_TEST_FIELDS, graphics_symbol_layout,
                             graphics_snapshot_stable, check_graphics_snapshot, check_graphics_progress)
from bringup import Board, check_snapshot
from probe_graphics import load_plan, case_coverage, check_runtime_progress


def valid_sample() -> dict:
    """기본 계약에 필요한 값만 만든다. 이 값이 실제 하드웨어 증거는 아니다."""
    g = dict.fromkeys(GRAPHICS_FIELDS, 0)
    g.update(magic=0x47524131, version=1, sample_seq=2, stage=4, initialized=1,
             process_count=10, render_count=2, last_process_ms=1000, hardware_sample_ms=950,
             heap_total=65536, heap_free=32768, heap_used=32768, heap_largest=16000,
             heap_peak=33000, heap_fragmentation=51, eve_frames=70, eve_pclk=3,
             eve_cmd_read=100, eve_cmd_write=100, eve_cmd_dl=500, ramg_used=8192)
    t = dict.fromkeys(GRAPHICS_TEST_FIELDS, 0)
    t.update(magic=0x47545354, version=1, initialized=1, case_count=12, state=2,
             auto_advance=1, process_count=10, rendered_frames=2)
    return dict(graphics=g, graphics_test=t, graphics_sample_match=True,
                panel=dict(magic=0x504E4C31, ready=1, stage=7, result=0, power_mode=0x9C),
                backlight=dict(magic=0x424C5431, initialized=1, result=0, timer_hz=84000000),
                tim5=dict(psc=1679, arr=99, ccr4=25, ccmr2=6 << 12, ccer=1 << 12, cr1=1),
                backlight_gpio=dict(pi0_mode=2, pi0_af=2, pi8_level=1, pc8_level=0))


def advanced(sample: dict) -> dict:
    out = copy.deepcopy(sample)
    for field, amount in dict(process_count=100, render_count=10, eve_frames=650,
                              last_process_ms=9000, hardware_sample_ms=9000).items():
        out['graphics'][field] += amount
    out['graphics_test'].update(current_case=1, transition_count=1, rendered_frames=12, process_count=110)
    return out


class GraphicsChecksTests(unittest.TestCase):
    def test_runtime_checks_each_halt_instead_of_accumulated_skew(self):
        # 실측과 같은 1/1/2/1/1 tick drift를 각 halt 구간별로 검사한다.
        first = dict(runtime=dict(heartbeat=100, hal_tick=1000, kernel_tick=990, tim6_interrupts=1000))
        current = copy.deepcopy(first)
        trace = []
        for index, skew in enumerate((1, 1, 2, 1, 1)):
            for field, amount in dict(heartbeat=10, hal_tick=100, kernel_tick=100-skew,
                                      tim6_interrupts=100).items():
                current['runtime'][field] += amount
            trace.append(dict(evidence=f'try{index + 1}', runtime=dict(current['runtime'])))
        current.update(capture_attempts=5, capture_trace=trace)
        report = check_runtime_progress(first, current)
        self.assertEqual(report['hal_kernel_skew'], 6)
        self.assertEqual(report['checked_halt_intervals'], 5)
        self.assertEqual([entry['hal_kernel_skew'] for entry in report['intervals']], [1, 1, 2, 1, 1])
        # 동일 +6이 한 구간에서 발생했다면 기존 기준대로 실패해야 한다.
        with self.assertRaisesRegex(RuntimeError, 'diverged'):
            check_runtime_progress(first, dict(runtime=current['runtime']))
        # 횟수만 늘리고 runtime 증거를 생략하는 것은 허용하지 않는다.
        with self.assertRaisesRegex(RuntimeError, 'Missing runtime trace'):
            check_runtime_progress(first, dict(runtime=current['runtime'], capture_attempts=5))
        bad = copy.deepcopy(current)
        bad['capture_attempts'] = 4
        with self.assertRaisesRegex(RuntimeError, 'mismatch'):
            check_runtime_progress(first, bad)
        # 중간 값만 바꿔 최종 합계가 그대로여도 국소 발산/정지를 잡는다.
        for field, value, error in [('kernel_tick', first['runtime']['kernel_tick'], 'stopped'),
                                    ('heartbeat', first['runtime']['heartbeat'], 'stopped'),
                                    ('hal_tick', trace[0]['runtime']['hal_tick'] + 10, 'diverged')]:
            bad = copy.deepcopy(current)
            bad['capture_trace'][0]['runtime'][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, error):
                check_runtime_progress(first, bad)

    def test_runtime_progress_rejects_stopped_or_diverging_clocks_and_accepts_wrap(self):
        fields = ('heartbeat', 'hal_tick', 'kernel_tick', 'tim6_interrupts')
        first = dict(runtime=dict.fromkeys(fields, 0xFFFFFFF0))
        current = dict(runtime=dict.fromkeys(fields, 0x20))
        self.assertEqual(check_runtime_progress(first, current)['delta']['hal_tick'], 48)
        for field in fields:
            bad = copy.deepcopy(current)
            bad['runtime'][field] = first['runtime'][field]
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, 'stopped'):
                check_runtime_progress(first, bad)
        for kernel in (0x1D, 0x23):
            bad = copy.deepcopy(current)
            bad['runtime']['kernel_tick'] = kernel
            with self.assertRaisesRegex(RuntimeError, 'diverged'):
                check_runtime_progress(first, bad)

    def test_graphics_capture_keeps_each_actual_halt_runtime_and_metadata(self):
        # CLI 없이 retry 정책만 실행한다. 실제 두 번 읽은 runtime이 accepted
        # JSON에도 보존되어, 다음 probe가 횟수만으로 허용치를 늘리지 않게 한다.
        class FakeBoard(Board):
            def snapshot(self, name, symbols, graphics_test=False):
                sample = valid_sample()
                index = int(name.rsplit('try', 1)[1])
                sample['graphics']['sample_seq'] = index
                sample['runtime'] = dict(heartbeat=index, hal_tick=index, kernel_tick=index, tim6_interrupts=index)
                return sample
        with tempfile.TemporaryDirectory() as temporary, patch('bringup.check_snapshot'), patch('bringup.time.sleep'):
            board = FakeBoard('FAKE', Path(temporary))
            sample = board.graphics_snapshot('offline', {}, attempts=2)
            saved = json.loads((Path(temporary) / 'offline-try2.json').read_text(encoding='utf-8'))
            self.assertEqual(saved['capture_attempts'], 2)
            self.assertEqual(saved['capture_trace'], sample['capture_trace'])
            self.assertEqual([item['runtime']['heartbeat'] for item in sample['capture_trace']], [1, 2])

    def test_runtime_control_allows_fpca_without_relaxing_privilege_or_psp(self):
        # FP 계산 전후 CONTROL 2→6은 정상이다. MSP/nPRIV/예약 비트나 IRQ
        # mask까지 허용하는 회귀가 생기지 않도록 실제 공통 검사기를 호출한다.
        sample = dict(
            boot=dict(magic=0x424F4F54, version=1, status=0x20, entry_vtor=0x20000000,
                      runtime_vtor=0x08010000, runtime_primask=1),
            runtime=dict(magic=0x42525550, stage=6, vtor=0x08010000, core_clock=168000000,
                         control=2, basepri=0, primask=0, min_free_heap=53128, stack_free_words=1559),
            fault=dict(magic=0, code=0))
        for control in (2, 6):
            with self.subTest(accepted_control=control):
                sample['runtime']['control'] = control
                check_snapshot(sample)
        for control in (0, 1, 3, 4, 5, 7, 8, 10, 14, 0xFFFFFFFF):
            with self.subTest(rejected_control=control), self.assertRaises(RuntimeError):
                sample['runtime']['control'] = control
                check_snapshot(sample)
        sample['runtime']['control'] = 6
        for field in ('basepri', 'primask'):
            sample['runtime'][field] = 1
            with self.subTest(masked_interrupt=field), self.assertRaises(RuntimeError):
                check_snapshot(sample)
            sample['runtime'][field] = 0

    def test_persistent_case_table_keeps_frames_missed_by_polling(self):
        table = [dict(id=index, state=state, visits=1, rendered_visits=visits, frames=frames,
                      ui_events=0, allocation_failures=0)
                 for index, (state, visits, frames) in enumerate([(1, 2, 50), (3, 0, 10), (2, 1, 1)])]
        sample = dict(graphics_test=dict(case_count=3, current_case=0, state=1), graphics_test_results=table)
        report = case_coverage([sample])
        self.assertTrue(report['all_cases_observed'])
        self.assertTrue(report['persistent_result_table_used'])
        self.assertEqual(report['observed_rendered_cases'], [0, 2])
        self.assertEqual(report['observed_skipped_cases'], [1])
        table[2]['state'] = 4
        with self.assertRaises(RuntimeError): case_coverage([sample])

    def test_full_case_coverage_separates_unsupported_from_rendered(self):
        samples = [{'graphics_test': dict(case_count=3, current_case=case, state=state)}
                   for case, state in [(0, 2), (1, 3), (2, 1)]]
        report = case_coverage(samples)
        self.assertFalse(report['all_cases_observed'])
        samples.append({'graphics_test': dict(case_count=3, current_case=2, state=2)})
        report = case_coverage(samples)
        self.assertTrue(report['all_cases_observed'])
        self.assertEqual(report['observed_rendered_cases'], [0, 2])
        self.assertEqual(report['observed_skipped_cases'], [1])
        self.assertFalse(report['skipped_is_implementation_pass'])
        self.assertEqual(report['observed_case_states']['2'], [1, 2])

    def test_good_sample_and_transition(self):
        first = valid_sample()
        self.assertFalse(check_graphics_snapshot(first)['fifo_pending'])
        delta = check_graphics_progress(first, advanced(first), True)
        self.assertEqual(delta['test']['transition_count'], 1)

    def test_odd_sequence_or_busy_requires_retry(self):
        for field in ('sample_seq', 'render_busy'):
            sample = valid_sample()
            sample['graphics'][field] = 1
            self.assertFalse(graphics_snapshot_stable(sample))
            with self.assertRaises(RuntimeError):
                check_graphics_snapshot(sample)

    def test_nonatomic_double_read_rejected(self):
        sample = valid_sample()
        sample['graphics_sample_match'] = False
        self.assertFalse(graphics_snapshot_stable(sample))

    def test_fifo_busy_valid_but_stalled_fails(self):
        first = valid_sample()
        first['graphics']['eve_cmd_write'] = 200
        self.assertTrue(check_graphics_snapshot(first)['fifo_pending'])
        second = advanced(first)
        second['graphics']['render_count'] = first['graphics']['render_count']
        with self.assertRaisesRegex(RuntimeError, 'stalled'):
            check_graphics_progress(first, second)

    def test_gpu_fault_and_bad_heap_rejected(self):
        for field, value in [('eve_cmd_read', 0xFFF), ('eve_cmd_write', 3),
                             ('eve_cmd_dl', 8196), ('heap_free', 65537),
                             ('heap_largest', 65537), ('eve_pclk', 0), ('spi_failures', 1)]:
            sample = valid_sample()
            sample['graphics'][field] = value
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                check_graphics_snapshot(sample)

    def test_transition_running_is_not_error(self):
        sample = valid_sample()
        sample['graphics_test']['state'] = 1
        check_graphics_snapshot(sample)
        sample['graphics_test']['state'] = 4
        with self.assertRaises(RuntimeError):
            check_graphics_snapshot(sample)

    def test_legacy_blink_zero_not_valid_in_graphics_mode(self):
        sample = valid_sample()
        sample['tim5']['ccr4'] = 0
        with self.assertRaises(RuntimeError):
            check_graphics_snapshot(sample)

    def test_case_advance_optional_static_frame_valid(self):
        first = valid_sample()
        second = advanced(first)
        second['graphics']['render_count'] = first['graphics']['render_count']
        second['graphics_test']['transition_count'] = 0
        check_graphics_progress(first, second)
        with self.assertRaises(RuntimeError):
            check_graphics_progress(first, second, True)

    def test_exact_symbol_contract(self):
        symbols = {'g_graphics': dict(address='0x20000100', size=160),
                   'g_graphics_test': dict(address='0x20000200', size=72)}
        layout = graphics_symbol_layout(symbols)
        self.assertEqual(layout['graphics']['fields'][39]['offset'], 156)
        for name, field, value in [('g_graphics', 'size', 156),
                                  ('g_graphics_test', 'address', '0x2002FFF0'),
                                  ('g_graphics', 'address', '0x20000101')]:
            bad = copy.deepcopy(symbols)
            bad[name][field] = value
            with self.assertRaises(RuntimeError):
                graphics_symbol_layout(bad)

    def test_manifest_plan_has_no_device_io(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'manifest.json'
            path.write_text(json.dumps(dict(schema_version=1, status='valid', flash_address='0x08010000',
                flash_limit_exclusive='0x08080000', binary_size=1024, binary_sha256='0' * 64,
                symbols={'g_graphics': dict(address='0x20000100', size=160),
                         'g_graphics_test': dict(address='0x20000200', size=72)})), encoding='utf-8')
            _, plan = load_plan(path, 3, 9.137, True, False)
            self.assertFalse(plan['hardware_access'])
            self.assertFalse(plan['flash_writes'])
            self.assertEqual(len(plan['graphics_layout']['graphics_test']['fields']), 18)

    def test_board_snapshot_decodes_and_rereads_under_one_halt(self):
        # 실제 CLI 대신 요청한 파일을 0으로 채운다. 이 검사는 주소/크기/
        # read 순서와 decoder만 검증하며 장치의 성공을 흉내 내지 않는다.
        class FakeBoard(Board):
            def command(self, name, *operations, timeout=240):
                self.operations = list(operations)
                index = 0
                while index < len(operations):
                    if operations[index] == '-u':
                        size = int(operations[index + 2], 0)
                        Path(operations[index + 3]).write_bytes(bytes(size))
                        index += 4
                    else:
                        index += 1
                return ''
        sizes = {'g_bsp_boot': 220, 'g_bsp_bringup': 56, 'g_bsp_fault': 40,
                 'g_bsp_eve': 72, 'g_bsp_lcd_panel': 60, 'g_bsp_backlight': 40,
                 'g_graphics': 160, 'g_graphics_test': 72, 'g_graphics_test_results': 41 * 28}
        symbols = {name: dict(address=hex(0x20001000 + index * 256), size=size)
                   for index, (name, size) in enumerate(sizes.items())}
        with tempfile.TemporaryDirectory() as temporary:
            board = FakeBoard('FAKE', Path(temporary))
            out = board.snapshot('offline', symbols, graphics_test=True)
            self.assertEqual(len(out['graphics']), 40)
            self.assertEqual(len(out['graphics_test']), 18)
            self.assertEqual(len(out['graphics_test_results']), 41)
            self.assertEqual(len(out['graphics_test_results'][0]), 7)
            self.assertTrue(out['graphics_sample_match'])
            self.assertEqual(board.operations[0], '-halt')
            self.assertEqual(board.operations[-1], '-run')
            self.assertEqual(board.operations.count('-halt'), 1)
            self.assertEqual(board.operations.count('-run'), 1)
            self.assertFalse(any(op in board.operations for op in ('-w', '-w32', '-rst', '-ob')))


if __name__ == '__main__':
    unittest.main()
