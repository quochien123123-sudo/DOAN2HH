#ifndef PUMP_PROTOCOL_H
#define PUMP_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#define PROTO_VERSION 1U
#define PROTO_RF_SIZE 34U
#define PROTO_NODE_COUNT 3U
#define PROTO_VALID_SHT 1U
#define PROTO_VALID_SOIL 2U
#define PROTO_POLL 1U
#define PROTO_SET 2U
#define PROTO_REPLY 3U
#define PROTO_STATUS 0U
#define PROTO_APPLIED 1U
#define PROTO_DUPLICATE 2U
#define PROTO_STALE 3U
#define PROTO_CONFLICT 4U
#define PROTO_BOOT_CHANGED 5U
typedef struct {
    uint8_t type, node, relay, valid, result;
    uint32_t masterSession, seq, nodeBoot, revision, sampleAgeMs;
    int16_t temp10;
    uint8_t humidity, soil;
} ProtoFrame;
static inline uint16_t Proto_CRC16(const uint8_t *p, size_t len) {
    uint16_t crc = 0xFFFFU;
    while (len--) {
        crc ^= (uint16_t)*p++ << 8;
        for (uint8_t bit = 0U; bit < 8U; ++bit)
            crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
    }
    return crc;
}
static inline void Proto_Put32(uint8_t *p, uint32_t v) {
    for (unsigned i=0U;i<4U;++i) p[i]=(uint8_t)(v>>(8U*i));
}
static inline uint32_t Proto_Get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline int Proto_Valid(const ProtoFrame *f) {
    return f && f->type>=PROTO_POLL && f->type<=PROTO_REPLY && f->node>=1U && f->node<=3U &&
        f->relay<=1U && f->valid<=3U && f->result<=PROTO_BOOT_CHANGED &&
        f->masterSession!=0U && f->seq!=0U &&
        (f->type==PROTO_POLL || f->nodeBoot!=0U) &&
        (!(f->valid & PROTO_VALID_SHT) || (f->temp10>=-400 && f->temp10<=1250 && f->humidity<=100U)) &&
        (!(f->valid & PROTO_VALID_SOIL) || f->soil<=100U);
}
static inline uint8_t Proto_Encode(uint8_t out[PROTO_RF_SIZE], const ProtoFrame *f) {
    if (!out || !Proto_Valid(f)) return 0U;
    out[0]='L'; out[1]='P'; out[2]=PROTO_VERSION; out[3]=f->type;
    out[4]=f->node; out[5]=f->relay; out[6]=f->valid; out[7]=f->result;
    Proto_Put32(out+8,f->masterSession); Proto_Put32(out+12,f->seq);
    Proto_Put32(out+16,f->nodeBoot); Proto_Put32(out+20,f->revision);
    uint16_t t=(uint16_t)f->temp10; out[24]=(uint8_t)t; out[25]=(uint8_t)(t>>8);
    out[26]=f->humidity; out[27]=f->soil; Proto_Put32(out+28,f->sampleAgeMs);
    uint16_t crc=Proto_CRC16(out,32U); out[32]=(uint8_t)crc; out[33]=(uint8_t)(crc>>8);
    return PROTO_RF_SIZE;
}
static inline uint8_t Proto_Decode(ProtoFrame *f, const uint8_t *p, size_t len) {
    if (!f || !p || len!=PROTO_RF_SIZE || p[0]!='L' || p[1]!='P' || p[2]!=PROTO_VERSION) return 0U;
    if (Proto_CRC16(p,32U)!=((uint16_t)p[32]|((uint16_t)p[33]<<8))) return 0U;
    ProtoFrame x;
    x.type=p[3]; x.node=p[4]; x.relay=p[5]; x.valid=p[6]; x.result=p[7];
    x.masterSession=Proto_Get32(p+8); x.seq=Proto_Get32(p+12);
    x.nodeBoot=Proto_Get32(p+16); x.revision=Proto_Get32(p+20);
    uint16_t t=(uint16_t)p[24]|((uint16_t)p[25]<<8);
    x.temp10=(int16_t)((t & 0x8000U) ? (int32_t)t-65536 : (int32_t)t);
    x.humidity=p[26]; x.soil=p[27]; x.sampleAgeMs=Proto_Get32(p+28);
    if (!Proto_Valid(&x)) return 0U;
    *f=x; return 1U;
}
/* Split in-place, preserving no empty fields; zero means invalid/too many tokens. */
static inline size_t Proto_Split(char *line, char **parts, size_t capacity) {
    if (!line || !*line || !parts || capacity==0U) return 0U;
    size_t n=1U; parts[0]=line;
    for (char *p=line;*p;++p) {
        if (*p==',') {
            if (p==parts[n-1U] || p[1]=='\0' || n==capacity) return 0U;
            *p='\0'; parts[n++]=p+1;
        } else if ((unsigned char)*p<0x20U || (unsigned char)*p>0x7EU) return 0U;
    }
    return n;
}
static inline int Proto_U32(const char *s, uint32_t *out) {
    if (!s || !*s || !out) return 0;
    uint32_t v=0U;
    while (*s) {
        if (*s<'0' || *s>'9') return 0;
        uint32_t digit=(uint32_t)(*s++-'0');
        if (v>(UINT32_MAX-digit)/10U) return 0;
        v=v*10U+digit;
    }
    *out=v; return 1;
}
static inline int Proto_I32(const char *s, int32_t *out) {
    if (!s || !out) return 0;
    uint8_t neg=(*s=='-'); if (neg) ++s;
    uint32_t v;
    if (!Proto_U32(s,&v) || v>(neg?2147483648U:2147483647U)) return 0;
    *out=neg ? (int32_t)(-(int64_t)v) : (int32_t)v; return 1;
}
#endif
