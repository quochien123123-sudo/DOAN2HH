#!/usr/bin/env python3
"""Build ESP32 on Linux x86_64 using verified official GitHub packages.

Normal user builds: `pio run -e devkitv1` from firmware/. This fallback avoids
PlatformIO's registry when it is unavailable; it does not bypass network policy.
Requires Python 3.12+, curl, and network permission for PyPI/GitHub release hosts.
Never connects to ThingsBoard or uploads to physical hardware.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cache', type=Path, default=ROOT / '.tools' / 'esp32-cloud')
parser.add_argument('--jobs', type=int, default=4)
args = parser.parse_args()
if sys.version_info < (3, 12) or platform.system() != 'Linux' or platform.machine() != 'x86_64':
    parser.error('Fallback tool downloads require Linux x86_64 and Python 3.12+')
if args.jobs < 1:
    parser.error('--jobs must be positive')
BASE = args.cache.resolve()
BASE.mkdir(parents=True, exist_ok=True)
DOWNLOADS = BASE / 'downloads'
DOWNLOADS.mkdir(exist_ok=True)
ENV = os.environ.copy()
ENV.update(PLATFORMIO_CORE_DIR=str(BASE / 'pio-core'),
           PLATFORMIO_SETTING_ENABLE_TELEMETRY='no', UV_CACHE_DIR=str(BASE / 'uv-cache'))

def run(command, **kwargs):
    subprocess.run([str(c) for c in command], check=True, env=ENV, **kwargs)

def save_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')

venv = BASE / 'venv'
python = venv / 'bin/python'
if not python.exists():
    if shutil.which('uv'):
        run(['uv', 'venv', venv])
    else:
        run([sys.executable, '-m', 'venv', venv])
requirements = ROOT / 'tools/requirements-esp32-cloud.txt'
marker = BASE / 'python-requirements.sha256'
reqhash = hashlib.sha256(requirements.read_bytes()).hexdigest()
if not marker.exists() or marker.read_text().strip() != reqhash:
    if shutil.which('uv'):
        run(['uv', 'pip', 'install', '--python', python, '-r', requirements])
    else:
        run([python, '-m', 'pip', 'install', '-r', requirements])
    marker.write_text(reqhash + '\n')

manifest = json.loads((ROOT / 'tools/esp32-downloads.json').read_text())
for artifact in manifest.values():
    archive = DOWNLOADS / artifact['filename']
    if not archive.exists():
        temporary = archive.with_suffix(archive.suffix + '.partial')
        run(['curl', '--fail', '--location', '--retry', '2', '--connect-timeout', '15',
             '--max-time', '300', '--output', temporary, artifact['url']])
        temporary.rename(archive)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != artifact['sha256']:
        raise RuntimeError(f'Checksum mismatch: {archive}; preserve verification and redownload trusted artifact')
    print(f"Verified SHA256: {artifact['filename']}", flush=True)

EXTRACT = BASE / 'extract'
EXTRACT.mkdir(exist_ok=True)
layouts = {'core': 'framework/esp32-2.0.17', 'toolchain': 'xtensa/xtensa-esp32-elf',
           'esptool': 'esptool/esptool', 'platform': 'platform/platform-espressif32-6.10.0',
           'pubsub': 'pubsub/pubsubclient-2.8', 'arduinojson': 'arduinojson/ArduinoJson-7.3.1'}
# Per-package extraction marker prevents mistaking a partial extraction for ready.
for key, artifact in manifest.items():
    folder = EXTRACT / layouts[key].split('/')[0]
    mark = folder / '.complete'
    if mark.exists() and mark.read_text().strip() == artifact['sha256']:
        continue
    if folder.exists():
        raise RuntimeError(f'Incomplete or changed tool package {folder}; preserve it and use a fresh --cache directory')
    temporary = Path(tempfile.mkdtemp(prefix='.extract-', dir=EXTRACT))
    try:
        archive = DOWNLOADS / artifact['filename']
        if archive.suffix == '.zip':
            with zipfile.ZipFile(archive) as z:
                for item in z.infolist():
                    rel = PurePosixPath(item.filename)
                    if rel.is_absolute() or '..' in rel.parts:
                        raise RuntimeError('Unsafe ZIP path')
                z.extractall(temporary)
        else:
            with tarfile.open(archive) as t:
                t.extractall(temporary, filter='data')
        (temporary / '.complete').write_text(artifact['sha256'] + '\n')
        temporary.rename(folder)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
paths = {key: EXTRACT / layout for key, layout in layouts.items()}
# Package manager metadata wraps the same checked artifacts; no compiler/core
# source or checksum is patched. Official framework keeps its own package.json.
save_json(paths['toolchain'] / 'package.json', {'name': 'toolchain-xtensa-esp32', 'version': '8.4.0+2021r2-patch5'})
save_json(paths['esptool'] / 'package.json', {'name': 'tool-esptoolpy', 'version': '1.40501.0'})
scons = BASE / 'scons'
scons.mkdir(exist_ok=True)
save_json(scons / 'package.json', {'name': 'tool-scons', 'version': '4.40801.0'})
(scons / 'scons.py').write_text('from SCons.Script import main\nmain()\n')
pio = venv / 'bin/pio'
scons_meta = BASE / 'pio-core/packages/tool-scons/.piopm'
if not scons_meta.exists():
    run([pio, 'pkg', 'install', '--global', '--tool', scons.as_uri(), '--skip-dependencies'])
# PlatformIO core matches its SCons adapter by owner. The actual SCons 4.8.1
# executable comes from pinned PyPI requirements and is imported by this wrapper.
metadata = json.loads(scons_meta.read_text())
metadata['spec'].update(owner='platformio', name='tool-scons')
save_json(scons_meta, metadata)
project = BASE / 'project'
project.mkdir(exist_ok=True)
(project / 'platformio.ini').write_text(f'''[platformio]
src_dir = {ROOT / 'ESP32_ThingsBoard'}
lib_dir = {project / 'lib'}
[env:devkitv1]
platform = {paths['platform'].as_uri()}
board = esp32doit-devkit-v1
framework = arduino
board_build.partitions = default.csv
board_upload.maximum_size = 1310720
board_build.arduino.upstream_packages = no
platform_packages =
  toolchain-xtensa-esp32 @ {paths['toolchain'].as_uri()}
  framework-arduinoespressif32 @ {paths['core'].as_uri()}
  tool-esptoolpy @ {paths['esptool'].as_uri()}
build_flags = -Wall -Wextra
''')
libs = project / 'lib'
libs.mkdir(exist_ok=True)
for key, name in [('pubsub', 'PubSubClient'), ('arduinojson', 'ArduinoJson')]:
    dest = libs / name
    if not dest.exists():
        dest.symlink_to(paths[key], target_is_directory=True)
release = ROOT / 'release/ESP32_ThingsBoard'
release.mkdir(parents=True, exist_ok=True)
logfile = release / 'build.log'
with logfile.open('w') as log:
    result = subprocess.run([str(pio), 'run', '--project-dir', str(project), '-j', str(args.jobs)],
                            env=ENV, stdout=log, stderr=subprocess.STDOUT)
print(logfile.read_text())
if result.returncode:
    raise SystemExit(result.returncode)
build = project / '.pio/build/devkitv1'
for filename in ['firmware.bin', 'firmware.elf', 'bootloader.bin', 'partitions.bin']:
    shutil.copy2(build / filename, release / filename)
shutil.copy2(paths['core'] / 'tools/partitions/boot_app0.bin', release / 'boot_app0.bin')
run([python, '-m', 'esptool', '--chip', 'esp32', 'merge_bin', '-o', release / 'factory.bin',
     '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', '4MB',
     '0x1000', release / 'bootloader.bin', '0x8000', release / 'partitions.bin',
     '0xe000', release / 'boot_app0.bin', '0x10000', release / 'firmware.bin'])
source_hashes = {}
for pattern in ['*.cpp', '*.h', '*.ino']:
    for path in sorted((ROOT / 'ESP32_ThingsBoard').glob(pattern)):
        # Record public sources only; never publish private credential content or hashes.
        if path.name == 'secrets.h':
            continue
        source_hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
notes = {'board':'esp32doit-devkit-v1 (DOIT ESP32 DevKit V1, 4MB, DIO40MHz)',
         'environment':'devkitv1', 'partition_layout':'default.csv',
         'physical_flash_bytes':4194304, 'application_partition_bytes':1310720,
         'arduino_esp32':'2.0.17','platformio':'6.1.18','platform':'6.10.0',
         'compiler':'xtensa-esp32-elf GCC8.4.0 esp-2021r2-patch5',
         'libraries':{'PubSubClient':'2.8','ArduinoJson':'7.3.1'},
         'private_config_present':(ROOT/'ESP32_ThingsBoard/secrets.h').is_file(),
         'factory_flash_address':'0x0','parts':{'bootloader.bin':'0x1000','partitions.bin':'0x8000',
         'boot_app0.bin':'0xe000','firmware.bin':'0x10000'},'source_sha256':source_hashes,
         'artifact_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted(release.glob('*.bin'))}}
save_json(release / 'build-manifest.json', notes)
print(f'ESP32 build and merged factory image ready: {release}')
