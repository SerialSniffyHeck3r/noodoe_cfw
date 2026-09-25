# Generated task stack contract

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/tests/task_stack_contract/test_stack_contract.ps1`.

The fixture exercises the shared production parser and the complete sync script
against a separate small project. It verifies the old generated 1024-word field
becomes 3072 words, every other Core byte remains identical, a second run does not
write the file, IOC is unchanged, BOM/LF survive and ambiguous input is rejected.
Each run keeps its small fixture and `result.json` here for inspection.

No Cube GUI, firmware build or board operation occurs. This synthetic regeneration
test does not claim that an actual CubeMX GUI generation was repeated.
