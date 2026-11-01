#ifndef NODE_STATE_H
#define NODE_STATE_H
/* Portable command policy: no GPIO, tick, radio or sensor dependencies. */
#include <stdint.h>
#include <string.h>
#include "protocol.h"

typedef struct {
    uint32_t nodeBoot, masterSession, revision;
    uint32_t lastSeq, lastExpectedRevision, lastExpectedBoot;
    uint8_t relay, haveLast, lastValue, lastResult;
} NodeControl;

static inline void NodeControl_Init(NodeControl *s, uint32_t boot)
{
    memset(s, 0, sizeof(*s));
    s->nodeBoot = boot; /* Reset/power-on policy: OFF. Link loss never calls this. */
}

static inline void NodeControl_LocalToggle(NodeControl *s)
{
    s->relay = (uint8_t)!s->relay;
    ++s->revision;
}

static inline uint8_t NodeControl_Handle(NodeControl *s, const ProtoFrame *q)
{
    uint32_t delta;
    uint8_t result;
    if (q->masterSession == 0U || q->seq == 0U) return PROTO_STALE;
    if (q->type == PROTO_POLL) {
        if (q->masterSession != s->masterSession) {
            s->masterSession = q->masterSession;
            s->haveLast = 0U;
        }
        return PROTO_STATUS;
    }
    if (q->type != PROTO_SET || q->masterSession == 0U || q->seq == 0U ||
        q->masterSession != s->masterSession) return PROTO_STALE;
    if (q->nodeBoot != s->nodeBoot) return PROTO_BOOT_CHANGED;
    if (s->haveLast) {
        if (q->seq == s->lastSeq) {
            /* A different payload cannot borrow an already used command ID. */
            if (q->relay != s->lastValue || q->revision != s->lastExpectedRevision ||
                q->nodeBoot != s->lastExpectedBoot) return PROTO_CONFLICT;
            /* Report CURRENT state; do not undo a subsequent local button action. */
            return (s->lastResult == PROTO_APPLIED) ? PROTO_DUPLICATE : s->lastResult;
        }
        delta = q->seq - s->lastSeq;
        if (delta == 0U || delta >= UINT32_C(0x80000000)) return PROTO_STALE;
    }
    result = PROTO_CONFLICT;
    if (q->relay <= 1U && q->revision == s->revision) {
        if (s->relay != q->relay) {
            s->relay = q->relay;
            ++s->revision;
        }
        result = PROTO_APPLIED;
    }
    s->lastSeq = q->seq;
    s->lastExpectedRevision = q->revision;
    s->lastExpectedBoot = q->nodeBoot;
    s->lastValue = q->relay;
    s->lastResult = result;
    s->haveLast = 1U;
    return result;
}
#endif
