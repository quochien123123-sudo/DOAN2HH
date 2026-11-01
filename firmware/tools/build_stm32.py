#!/usr/bin/env python3
"""Build Master and three addressed slaves; needs Python3 and GNU Arm Embedded GCC."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

BASE=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target', choices=['all','Master','Slave1','Slave2','Slave3'],default='all')
p.add_argument('--jobs',type=int,default=min(4,os.cpu_count() or 1))
p.add_argument('--gcc-bin',help='Folder containing arm-none-eabi-gcc (also ARM_GCC_BIN env)')
a=p.parse_args()
if a.jobs<1: p.error('--jobs must be positive')
toolbin=a.gcc_bin or os.environ.get('ARM_GCC_BIN')
if not toolbin:
    found=shutil.which('arm-none-eabi-gcc')
    if found: toolbin=str(Path(found).parent)
    elif Path('/workspace/doan2hh-env/toolchain/bin/arm-none-eabi-gcc').exists():
        toolbin='/workspace/doan2hh-env/toolchain/bin'
    else: p.error('Install GNU Arm Embedded GCC and add its bin to PATH or --gcc-bin')
ext='.exe' if os.name=='nt' else ''
def tool(suffix): return str(Path(toolbin)/('arm-none-eabi-'+suffix+ext))
def run(argv,**kw): subprocess.run([str(x) for x in argv],check=True,**kw)
run([tool('gcc'),'--version'])
# The projects also contain local copies so STM32CubeIDE imports are self-contained.
for project in ['Master','relay32']:
    for filename,folder in [('protocol.h','Inc'),('boot_counter.h','Inc'),('boot_counter.c','Src')]:
        if (BASE/'common'/filename).read_bytes()!=(BASE/project/'Core'/folder/filename).read_bytes():
            raise RuntimeError(f'{project}/{filename} differs from common; synchronize intentionally before building')
metadata={}
for target in (['Master','Slave1','Slave2','Slave3'] if a.target=='all' else [a.target]):
    source=BASE/('Master' if target=='Master' else 'relay32')
    out=BASE/'build'/target; out.mkdir(parents=True,exist_ok=True)
    definitions=['-DUSE_HAL_DRIVER','-DSTM32F103xB']
    if target!='Master': definitions+=['-DNODE_ID='+target[-1]]
    cpu=['-mcpu=cortex-m3','-mthumb','-mfloat-abi=soft']
    incs=['Core/Inc','Drivers/CMSIS/Device/ST/STM32F1xx/Include','Drivers/CMSIS/Include',
          'Drivers/STM32F1xx_HAL_Driver/Inc','Drivers/STM32F1xx_HAL_Driver/Inc/Legacy']
    includes=['-I'+str(source/i) for i in incs]
    files=sorted((source/'Core/Src').glob('*.c'))+sorted((source/'Drivers/STM32F1xx_HAL_Driver/Src').glob('*.c'))
    files+=sorted((source/'Core/Startup').glob('*.s'))
    commands=[]; objects=[]
    for f in files:
        obj=out/f.relative_to(source).with_suffix('.o'); obj.parent.mkdir(parents=True,exist_ok=True)
        flags=cpu+['-g3','-Os','-ffunction-sections','-fdata-sections','--specs=nano.specs']
        if f.suffix=='.c':
            flags+=['-std=gnu11','-Wall','-Wextra','-fstack-usage','-MMD','-MP']+definitions+includes
            if f.is_relative_to(source/'Core/Src'):
                flags+=['-Werror']
                if f.name=='syscalls.c': flags+=['-Wno-unused-parameter']
        else: flags+=['-x','assembler-with-cpp']
        command=[tool('gcc'),*flags,'-c',str(f),'-o',str(obj)]
        commands.append({'directory':str(out),'file':str(f),'arguments':command}); objects.append(obj)
    print(f'{target}: full build {len(files)} translation units, jobs={a.jobs}',flush=True)
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        list(pool.map(lambda c:run(c['arguments']),commands))
    (out/'compile_commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    elf=out/(target+'.elf')
    run([tool('gcc'),*cpu,'-o',elf,*objects,'-T'+str(source/'STM32F103C8TX_FLASH.ld'),
         '--specs=nosys.specs','--specs=nano.specs','-static','-Wl,--gc-sections',
         '-Wl,-Map='+str(out/(target+'.map')),'-Wl,--print-memory-usage',
         '-Wl,--start-group','-lc','-lm','-Wl,--end-group'])
    for fmt,suffix in [('binary','.bin'),('ihex','.hex')]:
        run([tool('objcopy'),'-O',fmt,elf,out/(target+suffix)])
    run([tool('size'),elf])
    artifacts={}
    for suffix in ['.elf','.bin','.hex']:
        f=out/(target+suffix)
        artifacts[f.name]={'size':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
    metadata[target]={'node_id':None if target=='Master' else int(target[-1]),'artifacts':artifacts}
(BASE/'build/manifest.json').write_text(json.dumps(metadata,indent=2)+'\n')
print('Build complete. STM32 flash base 0x08000000; last 2KiB reserved for boot counter.')
