#!/usr/bin/env python3
"""Exercise actual gateway parser functions on host; not an ESP32 emulation."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'ESP32_ThingsBoard/gateway.cpp').read_text()

def extract(marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth = 0
    quote = None
    escape = False
    comment = None
    i = opening
    while i < len(source):
        c = source[i]
        pair = source[i:i+2]
        if comment == '//':
            if c == '\n': comment = None
        elif comment == '/*':
            if pair == '*/': comment = None; i += 1
        elif quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == quote: quote = None
        elif pair in ('//', '/*'):
            comment = pair; i += 1
        elif c in ('"', "'"): quote = c
        elif c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                tail = i + 1
                if source[tail:tail+1] == ';': tail += 1
                return source[start:tail]
        i += 1
    raise RuntimeError('Unterminated source item: ' + marker)

items = ['struct NodeState ', 'struct GatewayState ', 'struct Command ', 'struct Completion ',
         'struct UartPending ', 'static bool ParseU32(', 'static bool ParseI32(',
         'static size_t Split(', 'static bool ValidStatus(', 'static void NoteUartError(',
         'static void NoteMasterFrame(', 'static void ParseMasterLine(']
prelude = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
static const uint8_t NODE_COUNT = 3;
static const int portMAX_DELAY = -1;
static int stateMutex;
static uint32_t mockTime = 1000;
static uint32_t gatewaySession = 1234;
static uint32_t millis() { return mockTime; }
static void xSemaphoreTake(int, int) {}
static void xSemaphoreGive(int) {}
'''
chunks = [prelude]
for marker in items:
    if marker == 'static bool ParseU32(':
        chunks.append('static GatewayState sharedState = {};\nstatic UartPending uartPending[NODE_COUNT] = {};')
    chunks.append(extract(marker))
checks = r'''
static void parse(const char *input) {
    char line[256];
    assert(strlen(input) < sizeof(line));
    strcpy(line, input);
    ParseMasterLine(line);
}
int main() {
    uint32_t u;
    int32_t i;
    assert(ParseU32("4294967295", u) && u == UINT32_MAX);
    assert(!ParseU32("4294967296", u));
    assert(!ParseU32("-1", u) && !ParseU32("+1", u) && !ParseU32("", u));
    assert(ParseI32("-2147483648", i) && i == INT32_MIN);
    assert(!ParseI32("2147483648", i) && !ParseI32("-2147483649", i));
    parse("U1,H,44,1");
    assert(sharedState.masterSession == 44 && sharedState.masterReceivedAt == 1000);
    for (unsigned node = 1; node <= 3; ++node) {
        char line[150];
        snprintf(line, sizeof(line), "U1,N,%u,1,1,1,3,275,48,63,100,10,-80,-5", node);
        parse(line);
        assert(sharedState.node[node-1].seen && sharedState.node[node-1].relay == 1);
        assert(sharedState.node[node-1].temp10 == 275 && sharedState.node[node-1].snrQuarter == -5);
    }
    const uint32_t frames = sharedState.uartFrames;
    const uint32_t errors = sharedState.uartErrors;
    mockTime = 2000;
    parse("U1,N,1,1,1,1,3,275,48,63,100,10,-80,-5,junk");
    parse("U1,N,4,1,1,1,3,275,48,63,100,10,-80,-5");
    parse("U1,N,1,1,1,1,3,275,48,63,100,10,-80,128");
    parse("U1,N,1,,1,1,3,275,48,63,100,10,-80,-5");
    assert(sharedState.uartFrames == frames && sharedState.uartErrors == errors+4);
    assert(sharedState.masterReceivedAt == 1000); // invalid data never freshens link
    UartPending &p = uartPending[1];
    p.active = true; p.command = {77, 2, 1, 44};
    parse("U1,A,999,77,2,OK,1,1");
    assert(!p.terminal); // foreign gateway session
    parse("U1,A,1234,78,2,OK,1,1");
    assert(!p.terminal); // other command
    parse("U1,A,1234,77,1,OK,1,1");
    assert(!p.terminal); // other node
    parse("U1,A,1234,77,2,OK,1,1");
    assert(p.terminal && !strcmp(p.result.status, "OK") && p.result.relay == 1);
    p = {}; p.active = true; p.command = {88, 2, 0, 44};
    parse("U1,A,1234,88,2,OK,1,1");
    assert(p.terminal && !strcmp(p.result.status, "BAD_RESULT"));
    p = {}; p.active = true; p.command = {99, 2, 1, 44};
    sharedState.node[1].relay = 1;
    sharedState.node[1].known = true;
    sharedState.node[1].online = true;
    sharedState.node[1].valid = 3;
    parse("U1,H,45,1");
    assert(p.terminal && !strcmp(p.result.status, "BOOT_CHANGED"));
    assert(p.result.relay == 1 && p.result.known == 1);
    assert(!sharedState.node[1].online && sharedState.node[1].valid == 0);
    assert(sharedState.node[1].relay == 1 && sharedState.node[1].known);
    assert(sharedState.masterSession == 45);
    puts("Gateway parser: nodes1..3, strict framing/ranges, freshness, session/ID/node, Master reboot cancellation+relay retention: PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='gateway-parser-') as directory:
    path = Path(directory)
    cpp = path / 'parser.cpp'
    cpp.write_text('\n'.join(chunks) + checks)
    executable = path / 'parser'
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-O0', '-g', '-fsanitize=address,undefined', str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, check=True)
