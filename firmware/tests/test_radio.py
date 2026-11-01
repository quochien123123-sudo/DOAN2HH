#!/usr/bin/env python3
"""Build the actual radio driver against a register/FIFO HAL fake on the host.

This verifies state-machine and error behavior, not RF electrical properties.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="doan2-radio-") as directory:
    temp = Path(directory)
    shutil.copy2(root / 'Master/Core/Src/lora.c', temp / 'lora.c')
    header = (root / 'Master/Core/Inc/lora.h').read_text()
    (temp / 'lora.h').write_text(header.replace('#include "main.h"', '#include "fake_hal.h"'))
    shutil.copy2(root / 'tests/radio/fake_hal.h', temp / 'fake_hal.h')
    shutil.copy2(root / 'tests/radio/test_radio.c', temp / 'test_radio.c')
    binary = temp / 'test-radio'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', '-g',
                    str(temp / 'lora.c'), str(temp / 'test_radio.c'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
