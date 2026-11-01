#!/usr/bin/env python3
"""Run portable logic/driver tests; these do not emulate STM32/ESP32 hardware."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def run(command): subprocess.run([str(x) for x in command],check=True,cwd=ROOT)
with tempfile.TemporaryDirectory(prefix='pump-tests-') as directory:
    dest=Path(directory)
    for source,includes in [('test_protocol.c',[ROOT/'common']),
                             ('test_slave_state.c',[ROOT/'common',ROOT/'relay32/Core/Inc'])]:
        executable=dest/Path(source).stem
        run(['gcc','-std=c11','-Wall','-Wextra','-Werror',*[f'-I{x}' for x in includes],ROOT/'tests'/source,'-o',executable])
        run([executable])
    executable=dest/'wifi_button'
    run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(ROOT/'ESP32_ThingsBoard'),
         ROOT/'tests/test_wifi_button.cpp','-o',executable])
    run([executable])
    if sys.platform.startswith('linux'):
        executable=dest/'boot_counter'
        run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/boot'),
             ROOT/'tests/test_boot_counter.c',ROOT/'common/boot_counter.c','-o',executable])
        result=subprocess.run([str(executable)],check=False)
        if result.returncode not in [0,77]: raise SystemExit(result.returncode)
    for test in ['test_master_logic.py','test_radio.py','test_gateway.py']:
        run([sys.executable,ROOT/'tests'/test])
print('All selected host tests completed. Physical hardware/RF/cloud tests are separate.')
