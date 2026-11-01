#include "protocol.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    ProtoFrame in={0}, out={0}; uint8_t b[PROTO_RF_SIZE];
    in.type=PROTO_REPLY; in.node=3; in.relay=1; in.valid=3; in.result=PROTO_APPLIED;
    in.masterSession=42; in.seq=UINT32_MAX; in.nodeBoot=23; in.revision=8;
    in.temp10=-123; in.humidity=84; in.soil=75; in.sampleAgeMs=400;
    assert(Proto_Encode(b,&in)==PROTO_RF_SIZE); assert(Proto_Decode(&out,b,sizeof b));
    assert(out.seq==UINT32_MAX && out.temp10==-123 && out.node==3 && out.revision==8);
    b[5]^=1; assert(!Proto_Decode(&out,b,sizeof b)); b[5]^=1;
    assert(!Proto_Decode(&out,b,sizeof b-1)); assert(!Proto_Decode(&out,b,sizeof b+1));
    in.node=0; assert(!Proto_Encode(b,&in)); in.node=4; assert(!Proto_Encode(b,&in));
    in.node=1; in.relay=2; assert(!Proto_Encode(b,&in));
    uint32_t u; int32_t i;
    assert(Proto_U32("4294967295",&u) && u==UINT32_MAX);
    assert(!Proto_U32("4294967296",&u)); assert(!Proto_U32("-1",&u));
    assert(!Proto_U32("1junk",&u)); assert(!Proto_U32("",&u)); assert(!Proto_U32(" 1",&u));
    assert(Proto_I32("-2147483648",&i) && i==INT32_MIN);
    assert(Proto_I32("2147483647",&i) && i==INT32_MAX);
    assert(!Proto_I32("2147483648",&i)); assert(!Proto_I32("-2147483649",&i));
    char a[]="U1,C,1,2,3,1"; char *parts[6]; assert(Proto_Split(a,parts,6)==6);
    char empty[]="U1,,C"; assert(!Proto_Split(empty,parts,6));
    char many[]="1,2,3,4,5,6,7"; assert(!Proto_Split(many,parts,6));
    char suffix[]="1,2,"; assert(!Proto_Split(suffix,parts,6));
    puts("protocol: encode/decode CRC, strict framing/ranges, integer limits PASS");
}
