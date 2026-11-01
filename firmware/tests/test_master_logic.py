#!/usr/bin/env python3
"""Compile selected original Master functions with host radio/time stubs.
This exercises transaction logic, not STM32 peripherals or RF timing.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Master/Core/Src/main.c').read_text()

def function(name):
    match = re.search(r'(?m)^static [^\n]*\b' + re.escape(name) + r'\(', source)
    if not match:
        raise RuntimeError('Function not found: ' + name)
    start = match.start()
    opening = source.index('{', match.end())
    depth = 0
    state = 'code'
    i = opening
    while i < len(source):
        ch = source[i]
        nxt = source[i:i+2]
        if state == 'code':
            if nxt == '/*': state = 'comment'; i += 2; continue
            if nxt == '//': state = 'line'; i += 2; continue
            if ch == '"': state = 'string'
            elif ch == "'": state = 'char'
            elif ch == '{': depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0: return source[start:i+1]
        elif state == 'comment':
            if nxt == '*/': state = 'code'; i += 2; continue
        elif state == 'line':
            if ch == '\n': state = 'code'
        else:
            if ch == '\\': i += 2; continue
            if (state == 'string' and ch == '"') or (state == 'char' and ch == "'"): state = 'code'
        i += 1
    raise RuntimeError('Unclosed function: ' + name)

defines = '\n'.join(line for line in source.splitlines() if line.startswith('#define '))
types = source[source.index('typedef struct {'):source.index('static volatile uint8_t rxRing')]
prefix = '''#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n#include <limits.h>\n#include <assert.h>\n#include "protocol.h"\n'''
prefix += defines + '\n' + '''
static uint32_t fakeNow = 1000U;
static uint32_t masterSession = 42U, nextSequence;
static uint8_t radioReady = 1U;
static uint32_t nextRadioRecovery;
static uint32_t masterRadioErrors, masterCommandOk, masterCommandUnconfirmed, masterProtocolErrors;
static char sent[192];
static uint32_t HAL_GetTick(void) { return fakeNow; }
static void UARTSend(const char *s) { snprintf(sent, sizeof(sent), "%s", s); }
static uint8_t LoRa_IsReady(void) { return 1U; }
static int16_t LoRa_GetLastRSSI(void) { return -80; }
static int8_t LoRa_GetLastSNR(void) { return 12; }
'''
functions = ['Age', 'Online', 'NextSequence', 'SendResult', 'FindResult', 'StoreResult',
             'FinishCommand', 'RequestCommand', 'HandleUARTLine', 'UpdateNode', 'StartTransmission',
             'ClearOwner', 'RadioFailure', 'RetryOrFinish', 'HandleReply']
body = prefix + '\n' + types + '\n\n' + '\n\n'.join(function(n) for n in functions)
body += r'''
static void reset(void) {
    memset(nodes,0,sizeof(nodes)); memset(completed,0,sizeof(completed)); memset(&owner,0,sizeof(owner));
    cacheWrite=0U; acceptedGatewaySession=0U; newestGatewayCommandId=0U;
    radioReady=1U; consecutiveRadioFailures=0U; nextSequence=0U; sent[0]='\0'; fakeNow=1000U; masterProtocolErrors=0U;
    for(unsigned i=0;i<NODE_COUNT;i++) { nodes[i].seen=1U; nodes[i].lastSeen=fakeNow; nodes[i].nodeBoot=12U+i; nodes[i].revision=4U; }
}
static ProtoFrame response(uint8_t result, uint8_t relay) {
    ProtoFrame f={0}; f.type=PROTO_REPLY; f.node=1U; f.masterSession=masterSession;
    f.seq=owner.request.seq; f.nodeBoot=12U; f.revision=5U; f.relay=relay; f.result=result;
    return f;
}
int main(void) {
    reset();
    char oldEpoch[]="U1,C,99,1,1,1,41";
    HandleUARTLine(oldEpoch);
    assert(!nodes[0].command.pending && strstr(sent,"BOOT_CHANGED"));
    assert(acceptedGatewaySession==0U && newestGatewayCommandId==0U);
    char legacy[]="U1,C,99,1,1,1";
    sent[0]='\0'; HandleUARTLine(legacy);
    assert(!nodes[0].command.pending && masterProtocolErrors==1U && !sent[0]);
    char matchingEpoch[]="U1,C,99,1,1,1,42";
    HandleUARTLine(matchingEpoch);
    assert(nodes[0].command.pending && nodes[0].command.gatewaySession==99U && nodes[0].command.id==1U);
    puts("PASS UART receiver epoch: matching accepted, old BOOT_CHANGED without dedup mutation, legacy rejected");

    reset();
    RequestCommand(99U,1U,1U,1U);
    assert(nodes[0].command.pending && !nodes[1].command.pending && !nodes[2].command.pending);
    uint32_t before=nodes[0].command.queuedAt;
    RequestCommand(99U,1U,1U,1U); assert(nodes[0].command.queuedAt==before);
    RequestCommand(99U,1U,2U,1U); assert(strstr(sent,"BAD_REQUEST"));
    /* A later request rejected BUSY must stay rejected on a UART retry. */
    RequestCommand(99U,2U,1U,0U); assert(strstr(sent,"BUSY"));
    FinishCommand(0U,"OK");
    RequestCommand(99U,2U,1U,0U); assert(!nodes[0].command.pending && strstr(sent,"BUSY"));
    puts("PASS UART dedup, request identity, BUSY terminal-result cache, per-node isolation");

    reset(); RequestCommand(99U,1U,1U,1U);
    owner.job=RF_PREFLIGHT; owner.nodeIndex=0U; owner.request.node=1U;
    owner.request.masterSession=masterSession; owner.request.seq=100U;
    ProtoFrame f=response(PROTO_STATUS,0U); f.revision=27U;
    HandleReply(&f);
    assert(owner.job==RF_SET && owner.request.type==PROTO_SET && owner.request.nodeBoot==12U);
    assert(owner.request.revision==27U && owner.request.relay==1U && owner.txQueued);
    uint32_t setSeq=owner.request.seq;
    f=response(PROTO_APPLIED,1U); f.seq=setSeq-1U; HandleReply(&f);
    assert(nodes[0].relay==0U && nodes[0].command.pending);
    f.seq=setSeq; HandleReply(&f);
    assert(!nodes[0].command.pending && nodes[0].relay==1U && strstr(sent,",OK,1,1"));
    puts("PASS fresh preflight CAS fields; unmatched old ACK ignored; matching applied ACK completes");

    reset(); RequestCommand(99U,1U,1U,1U);
    owner.job=RF_SET; owner.nodeIndex=0U; owner.request.node=1U;
    owner.request.masterSession=masterSession; owner.request.seq=9U; owner.request.nodeBoot=12U;
    f=response(PROTO_DUPLICATE,0U); HandleReply(&f);
    assert(!nodes[0].command.pending && strstr(sent,"OVERRIDDEN") && nodes[0].relay==0U);
    puts("PASS duplicate reply with intervening local OFF becomes OVERRIDDEN, never re-applies ON");

    reset(); RequestCommand(99U,1U,1U,1U);
    owner.job=RF_SET; owner.nodeIndex=0U; owner.attempts=SET_ATTEMPTS;
    nodes[0].relay=1U;
    RetryOrFinish("ACK_TIMEOUT");
    assert(nodes[0].relay==1U && !nodes[0].command.pending && strstr(sent,"ACK_TIMEOUT"));
    assert(!Online(&nodes[0],fakeNow+NODE_OFFLINE_MS+1U));
    assert(nodes[0].relay==1U && nodes[0].seen);
    puts("PASS timeout/offline preserves last known ON, no OFF command generated");

    reset(); RequestCommand(0U,1U,1U,1U);
    owner.job=RF_PREFLIGHT; owner.nodeIndex=0U; owner.request.node=1U; owner.request.seq=1U;
    f=response(PROTO_STATUS,1U); f.revision=90U;
    HandleReply(&f);
    assert(owner.request.relay==0U && owner.request.revision==90U);
    puts("PASS physical toggle uses freshly polled state, with revision precondition");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='master-logic-') as temp:
    path = Path(temp)
    (path/'test.c').write_text(body)
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function',
                    '-Wno-unused-variable','-I'+str(root/'common'),str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
