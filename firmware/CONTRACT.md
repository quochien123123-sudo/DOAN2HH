# Pump protocol, version 1

These protocols are used together by the supplied Master, Slave and ESP32.
They are incompatible with the original CSV radio and N1/TB/TBRES UART formats.

## Radio

SX1278/RA-02, 433 MHz, SF7, BW125 kHz, coding rate4/5, explicit header,
payload CRC, preamble8, sync word0x12. All integers are explicitly encoded
little-endian; the C struct is never transmitted with native padding.

| Offset | Size | Field |
|---:|---:|---|
|0|2|Magic ASCII LP|
|2|1|Version1|
|3|1|Type: POLL1, SET2, REPLY3|
|4|1|Slave ID1..3|
|5|1|Relay0/1|
|6|1|Sensor-valid mask: SHT bit0, soil bit1|
|7|1|Result|
|8|4|Master boot session|
|12|4|RF sequence|
|16|4|Slave boot session|
|20|4|State revision|
|24|2|Signed temperature×10|
|26|1|Humidity%|
|27|1|Soil%|
|28|4|Age since sensor sampling cycle, ms|
|32|2|CRC16-CCITT-FALSE over bytes0..31|

Exactly34 bytes. A frame takes approximately77.056ms on air at this configuration.
Result values: STATUS0, APPLIED1, DUPLICATE2, STALE3, CONFLICT4, BOOT_CHANGED5.

Master owns the half-duplex schedule. Slave replies only to addressed poll/set,
not unsolicited periodic telemetry. Poll replies echo the Master session and
sequence and include current Slave boot/revision/output/sensors. A different
Master session is bound during poll, establishing a new operational session.

Before SET, Master polls the Slave and uses that boot ID and revision as
preconditions. SET with an old boot, wrong revision or old sequence cannot
change the output. Duplicate same-session/sequence/payload does not reapply;
it returns the current output and revision. Local button events increment the
revision. Therefore an ACK can report OVERRIDDEN at the gateway if the prior
SET was applied but a local action has since changed the output.

Session counters survive ordinary MCU resets through the final2KiB Flash
journal. Full erase/reprovisioning resets this history and requires resetting
and resynchronizing the system. This is operational duplicate handling, not
cryptographic sender authentication. There is one Master in this design.

Driver TX is asynchronous with a500ms deadline. Master waits250ms for poll
reply and350ms for SET reply after TxDone, with at most3 SET transmissions.
Commands expire after6s in Master; link loss never sends OFF. Retry deadlines
are software defaults requiring hardware timing validation. Radio reception
and long network operations do not share an ESP32 task.

## UART

115200 8N1, ASCII newline-terminated. CR is ignored. Frames are shorter than192
bytes. Numeric fields require complete decimal parsing with checked ranges;
empty/extra fields and overflow are rejected. An overlong/error line is dropped
through newline, not interpreted by its tail.

ESP32 -> Master,7 tokens:

```text
U1,C,gatewaySession,commandId,node,relay,expectedMasterSession
```

The expected Master session is learned from heartbeat and captured with the
request. Master rejects another boot epoch before command admission. ESP32
cancels active/queued old-epoch commands on a changed heartbeat; retry retains
the exact same identity/epoch. This also protects retries arriving before the
new heartbeat after an ordinary Master reset.

Master -> ESP32 result,8 tokens:

```text
U1,A,gatewaySession,commandId,node,status,relay,known
```

Wire statuses: OK, BUSY, OFFLINE, RADIO_ERROR, ACK_TIMEOUT, CONFLICT,
BOOT_CHANGED, BAD_REQUEST, OVERRIDDEN. ESP32 may additionally produce local
MASTER_TIMEOUT/BAD_RESULT and configuration/admission errors. A timeout
means unconfirmed outcome, not proof the pump did not execute.

Master -> ESP32 node snapshot,14 tokens, each node approximately once/second:

```text
U1,N,node,online,known,relay,validMask,temp10,humidity,soil,sampleAgeMs,lastSeenAgeMs,rssi,snrQuarter
```

Master -> ESP32 heartbeat,4 tokens, approximately once/second:

```text
U1,H,masterSession,radioReady
```

No UART telemetry ACK is needed. Unknown/offline output is last-known data;
freshness flags and ages must accompany it in the dashboard. SNR is signed
quarter-dB on UART and converted to dB for ThingsBoard.

Master caches12 completed UART results, including rejected requests; duplicate
requests do not become new commands. ESP32 caches16 cloud requests/results in
RAM, keeps failed publishes for reconnect, and retries UART with the same ID.
Those caches have explicit limits and are not persistent cloud delivery logs.
PubSubClient outbound publish is QoS0; retain=false is not a QoS selector.
