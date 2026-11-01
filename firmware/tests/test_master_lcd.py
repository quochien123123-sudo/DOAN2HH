#!/usr/bin/env python3
"""Regression-test the actual Master LCD scheduler with host time/LCD stubs.

A frozen pre-fix function reproduces the blank screen when radio initialization
fails. The production function is extracted from main.c each run. No STM32
peripheral or physical display behavior is emulated by these scheduler checks.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'Master/Core/Src/main.c').read_text()
# Historical function frozen below; only its name is changed for side-by-side tests.
BEFORE_FIX = 'static void LCDProcess(void)\n{\n    uint32_t now = HAL_GetTick();\n    if (!Due(now, nextLCD) || !LCD2004_IsReady() || owner.job != RF_NONE || HaveCommands()) return;\n    unsigned i;\n    for (i = 0; i < NODE_COUNT; ++i)\n        if (Due(now + 60U, nodes[i].nextPoll)) return;\n    nextLCD = now + 150U;\n    char line[21];\n    if (lcdRow < NODE_COUNT) {\n        NodeState *n = &nodes[lcdRow];\n        snprintf(line, sizeof(line), "%cN%u %-3s R:%s%s",\n                 selectedNode == lcdRow ? \'>\' : \' \', (unsigned)lcdRow + 1U,\n                 Online(n, now) ? "ONL" : "OFF", n->seen ? (n->relay ? "ON" : "OFF") : "?",\n                 n->command.pending ? " WAIT" : "");\n    } else {\n        snprintf(line, sizeof(line), "SEL:N%u RF:%s", (unsigned)selectedNode + 1U,\n                 radioReady ? "OK" : "ERR");\n    }\n    LCD2004_PrintLine(lcdRow, line);\n    lcdRow = (uint8_t)((lcdRow + 1U) % 4U);\n}'


def extract_function(name):
    match = re.search(r'(?m)^static [^\n]*\b' + re.escape(name) + r'\(', SOURCE)
    if not match:
        raise RuntimeError('Missing production function: ' + name)
    start = match.start()
    opening = SOURCE.index('{', match.end())
    depth = 0
    state = 'code'
    i = opening
    while i < len(SOURCE):
        ch = SOURCE[i]
        pair = SOURCE[i:i + 2]
        if state == 'code':
            if pair == '/*': state = 'comment'; i += 2; continue
            if pair == '//': state = 'line'; i += 2; continue
            if ch == '"': state = 'string'
            elif ch == "'": state = 'char'
            elif ch == '{': depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0: return SOURCE[start:i + 1]
        elif state == 'comment':
            if pair == '*/': state = 'code'; i += 2; continue
        elif state == 'line':
            if ch == '\n': state = 'code'
        else:
            if ch == '\\': i += 2; continue
            if (state == 'string' and ch == '"') or (state == 'char' and ch == "'"):
                state = 'code'
        i += 1
    raise RuntimeError('Unclosed production function: ' + name)


prefix = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define NODE_COUNT 3U
#define NODE_OFFLINE_MS 8000U
#define RF_NONE 0U

typedef struct {
    uint8_t seen, relay;
    uint32_t lastSeen, nextPoll;
    struct { uint8_t pending; } command;
} NodeState;
static NodeState nodes[NODE_COUNT];
static struct { uint8_t job; } owner;
static uint8_t radioReady, selectedNode, lcdRow;
static uint32_t nextLCD;
static uint32_t fakeNow, writeCount, rowsWritten;
static uint8_t fakeLCDReady, fakeRadioReady, fakeCommands;
static char lastLine[21];
static uint32_t HAL_GetTick(void) { return fakeNow; }
static uint8_t LCD2004_IsReady(void) { return fakeLCDReady; }
static uint8_t LoRa_IsReady(void) { return fakeRadioReady; }
static uint8_t HaveCommands(void) { return fakeCommands; }
static void LCD2004_PrintLine(uint8_t row, const char *line) {
    assert(row < 4U);
    ++writeCount;
    rowsWritten |= 1U << row;
    snprintf(lastLine, sizeof(lastLine), "%s", line);
}
'''

cases = r'''
static void reset(void) {
    memset(nodes, 0, sizeof(nodes));
    owner.job = RF_NONE;
    radioReady = 0U; fakeRadioReady = 0U;
    selectedNode = 0U; lcdRow = 3U; nextLCD = 0U;
    fakeNow = 1000U; fakeLCDReady = 1U; fakeCommands = 0U;
    writeCount = rowsWritten = 0U; lastLine[0] = '\0';
}
int main(void) {
    reset();
    LCDProcessBeforeFix();
    assert(writeCount == 0U);
    puts("PASS reproduction: pre-fix no-radio scheduler writes zero rows with overdue polls");

    reset();
    LCDProcess();
    assert(writeCount == 1U && strstr(lastLine, "RF:ERR") != NULL);
    for (unsigned i = 0U; i < 3U; ++i) { fakeNow += 150U; LCDProcess(); }
    assert(writeCount == 4U && rowsWritten == 0x0FU);
    puts("PASS no-radio scheduler paints RF:ERR and all four rows instead of remaining blank");

    reset();
    radioReady = 1U; fakeRadioReady = 1U;
    for (unsigned i = 0U; i < NODE_COUNT; ++i) nodes[i].nextPoll = fakeNow + 50U;
    LCDProcess();
    assert(writeCount == 0U && nextLCD == 0U);
    puts("PASS healthy imminent polls still defer LCD work");

    reset();
    radioReady = 1U; fakeRadioReady = 1U;
    for (unsigned i = 0U; i < NODE_COUNT; ++i) nodes[i].nextPoll = fakeNow + 1000U;
    LCDProcess();
    assert(writeCount == 1U && strstr(lastLine, "RF:OK") != NULL);
    puts("PASS healthy idle radio permits LCD updates");

    reset();
    radioReady = 1U; fakeRadioReady = 0U;
    LCDProcess();
    assert(writeCount == 1U);
    puts("PASS hardware radio not-ready also releases the unusable poll reservation");

    reset(); owner.job = 1U; LCDProcess(); assert(writeCount == 0U);
    reset(); fakeCommands = 1U; LCDProcess(); assert(writeCount == 0U);
    reset(); fakeLCDReady = 0U; LCDProcess(); assert(writeCount == 0U);
    puts("PASS active radio owner, queued commands, and unavailable LCD retain priority guards");
    return 0;
}
'''

production = '\n\n'.join(extract_function(name) for name in ['Due', 'Age', 'Online', 'LCDProcess'])
baseline = BEFORE_FIX.replace('LCDProcess(void)', 'LCDProcessBeforeFix(void)', 1)
with tempfile.TemporaryDirectory(prefix='master-lcd-') as directory:
    temp = Path(directory)
    (temp / 'test.c').write_text(prefix + '\n' + production + '\n' + baseline + '\n' + cases)
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O0',
                    str(temp / 'test.c'), '-o', str(temp / 'test')], check=True)
    subprocess.run([str(temp / 'test')], check=True)

# The address probe must not race PCF8574/LCD power-up, and the delay must not
# accidentally be duplicated during refactoring.
lcd_source = (ROOT / 'Master/Core/Src/lcd2004.c').read_text()
assert lcd_source.count('HAL_Delay(50);') == 1
assert lcd_source.index('HAL_Delay(50);') < lcd_source.index('HAL_I2C_IsDeviceReady(')
print('PASS one 50ms power-up delay precedes the first Master LCD address probe')
