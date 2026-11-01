#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "node_state.h"

static ProtoFrame frame(uint8_t type, uint32_t session, uint32_t seq,
                        uint32_t boot, uint32_t rev, uint8_t relay)
{
    ProtoFrame q;
    memset(&q, 0, sizeof(q));
    q.type = type; q.node = 1U; q.masterSession = session; q.seq = seq;
    q.nodeBoot = boot; q.revision = rev; q.relay = relay;
    return q;
}

int main(void)
{
    NodeControl s;
    ProtoFrame q;
    NodeControl_Init(&s, 41U);
    assert(s.relay == 0U && s.revision == 0U);
    q = frame(PROTO_SET, 10U, 1U, 41U, 0U, 1U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STALE && s.relay == 0U);
    q = frame(PROTO_POLL, 10U, 1U, 0U, 0U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STATUS);
    q = frame(PROTO_SET, 10U, 2U, 41U, 0U, 1U);
    assert(NodeControl_Handle(&s, &q) == PROTO_APPLIED);
    assert(s.relay == 1U && s.revision == 1U);
    /* Lost ACK, manual OFF, then same command retry: do not turn the pump on. */
    NodeControl_LocalToggle(&s);
    assert(s.relay == 0U && s.revision == 2U);
    assert(NodeControl_Handle(&s, &q) == PROTO_DUPLICATE);
    assert(s.relay == 0U && s.revision == 2U);
    /* A different payload must not reuse an existing ID. */
    q.relay = 0U;
    assert(NodeControl_Handle(&s, &q) == PROTO_CONFLICT);
    q = frame(PROTO_SET, 10U, 3U, 41U, 1U, 1U);
    assert(NodeControl_Handle(&s, &q) == PROTO_CONFLICT && s.relay == 0U);
    q.revision = 2U;
    assert(NodeControl_Handle(&s, &q) == PROTO_CONFLICT && s.relay == 0U);
    q.seq = 4U;
    assert(NodeControl_Handle(&s, &q) == PROTO_APPLIED);
    assert(s.relay == 1U && s.revision == 3U);
    q = frame(PROTO_SET, 10U, 2U, 41U, 0U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STALE && s.relay == 1U);
    q = frame(PROTO_SET, 10U, 5U, 42U, 3U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_BOOT_CHANGED && s.relay == 1U);
    q = frame(PROTO_SET, 11U, 1U, 41U, 3U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STALE);
    q.type = PROTO_POLL;
    assert(NodeControl_Handle(&s, &q) == PROTO_STATUS);
    q.type = PROTO_SET;
    assert(NodeControl_Handle(&s, &q) == PROTO_APPLIED);
    assert(s.relay == 0U && s.revision == 4U);
    /* Half-range sequence ordering handles wrap and rejects a late old frame. */
    NodeControl_Init(&s, 1U);
    q = frame(PROTO_POLL, 2U, 1U, 0U, 0U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STATUS);
    q = frame(PROTO_SET, 2U, UINT32_MAX - 1U, 1U, 0U, 1U);
    assert(NodeControl_Handle(&s, &q) == PROTO_APPLIED);
    q = frame(PROTO_SET, 2U, 1U, 1U, 1U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_APPLIED && s.relay == 0U);
    q = frame(PROTO_SET, 2U, UINT32_MAX, 1U, 1U, 1U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STALE && s.relay == 0U);
    q = frame(PROTO_POLL, 0U, 1U, 0U, 0U, 0U);
    assert(NodeControl_Handle(&s, &q) == PROTO_STALE && s.masterSession == 2U);
    puts("slave policy: startup OFF, session binding, applied/duplicate/stale, local override,");
    puts("CAS conflict, boot mismatch, new-session recovery and sequence wrap all passed.");
    return 0;
}
