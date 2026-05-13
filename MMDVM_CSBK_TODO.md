# MMDVMHost CSBK Channel Grant - Development Plan
**KE9CNK DMR Repeater Project**
**Date: May 12, 2026**

---

## Background & Context

### Repeater Architecture
- **rxhot** (192.168.2.202): Raspberry Pi Zero 2W + MMDVM_HS_Dual_Hat
  - Receives RF on **439.9875 MHz**, CC1, TS2, TG14260
  - Runs MMDVMHost-20230927 (compiled from git commit `eb79830`)
  - Config: `/etc/mmdvm/MMDVM.ini`, `Duplex=0` (simplex/hotspot mode)
  - Connects to HBLink3 on txhot via HomeBrew Protocol
  - Systemd service: `mmdvmhost.service`

- **txhot** (192.168.2.4): Raspberry Pi Zero 2W + MMDVM_HS_Dual_Hat
  - Transmits RF on **449.9875 MHz**, CC1, TS2, TG14260
  - Runs MMDVMHost-20230927 (compiled from git commit `eb79830`)
  - Config: `/etc/mmdvm/MMDVM.ini`, `Duplex=0`
  - Connects to local HBLink3 (127.0.0.1:62032)
  - Systemd services: `hblink3.service` + `mmdvmhost.service`

- **HBLink3** runs on txhot at `~/hblink3/`
  - Two MASTER instances: RXHOT-MASTER (port 62031) and TXHOT-LOCAL (port 62032)
  - Bridge rules: `~/hblink3/bridge_rules.py` routes TG14260 TS2 between them
  - Systemd service: `hblink3.service`

### MMDVMHost Source Location
- Source: `~/MMDVMHost/` on both boards (git commit `eb79830`, Oct 10 2023)
- Compiled binary: `~/MMDVMHost/MMDVMHost`
- Key source files for this task:
  - `BPTC19696.cpp` / `BPTC19696.h`
  - `DMRSlotType.cpp`
  - `DMRCSBK.cpp` / `DMRCSBK.h`
  - `DMRSlotRX.cpp` (hotspot RX processing)
  - `Modem.cpp` / `Modem.h`

---

## The Problem

The **Baofeng DR1801UV** radio sends a **Downlink Activate CSBK** (Control Signaling Block)
before transmitting voice. This is a DMR channel access request where the radio:

1. Sends a CSBK with opcode `0xB8` (BS_Dwn_Act with C_LAST bit set)
2. **Waits for a Channel Grant response** from the repeater
3. Only transmits voice AFTER receiving the grant

Without the grant, the radio never transmits voice. The repeater stays silent.

### Raw CSBK Packet Observed
```
B8 00 00 00 FF FF FF 31 10 0F 1C 04
```
- `B8` = CSBK opcode 0x38 (BS_Dwn_Act) with C_LAST bit (0x80) set
- `FF FF FF` = destination (broadcast)
- `31 10 0F` = source radio ID 3215375 (DR1801UV's DMR ID, 3 bytes)
- `1C 04` = additional fields

This appears repeatedly (every ~60-80ms) as the radio polls for a grant.

### Why It Happens
- rxhot runs in `Duplex=0` (simplex hotspot mode)
- In simplex mode, MMDVMHost does **not** transmit CSBK responses
- The radio expects a proper Tier II repeater CSBK handshake
- Other radios (OpenGD77) have "PTT State=Always" and don't require this handshake
- The DR1801UV CPS has a bug preventing the PTT State from being changed

---

## Goal

Modify MMDVMHost on **rxhot** to detect the incoming Downlink Activate CSBK and
respond with a **Channel Grant CSBK** on the RX frequency (439.9875 MHz), telling
the radio it is clear to transmit.

**Expected result**: DR1801UV receives the grant, stops sending CSBKs, and
immediately begins transmitting voice which rxhot decodes and routes through
HBLink3 to txhot for retransmission on 449.9875 MHz.

---

## Technical Approach

### CSBK Response Required
When rxhot sees opcode `0xB8` (Downlink Activate), respond with a
**Channel Grant** CSBK or **Aloha** CSBK on the same slot/timeslot.

In DMR Tier II, the appropriate response is:
- **CSBK Aloha** (opcode `0x19`) — general "you may proceed" response, OR
- **BS_Dwn_Act acknowledgement** — direct response to the access request

The response must be:
- Transmitted on **TS2** (same timeslot as the request)
- Addressed to the requesting radio's ID (`31 10 0F` = 3215375)
- Sent promptly (within one DMR frame ~30ms) after receiving the CSBK

### Where to Make Changes

#### Option A — Modify rxhot MMDVMHost (Recommended)
File: `~/MMDVMHost/DMRCSBK.cpp` and/or `DMRSlot.cpp`

In the CSBK receive handler, add detection for opcode `0x38`/`0xB8` and
queue a CSBK response for transmission on the modem board.

Since rxhot is in `Duplex=0`, the modem board CAN still transmit on
439.9875 MHz (same frequency as RX) — it just doesn't do so automatically.
The response only needs to be a short CSBK burst (~60ms), not sustained TX.

#### Option B — Add CSBK handling in DMRSlotRX
File: `~/MMDVMHost/DMRSlotRX.cpp`

In the hotspot RX data path, intercept CSBK frames and inject a response
via the modem's TX queue.

### Key MMDVMHost Functions to Modify

```
CDMRCSBK::decode()          - Decodes incoming CSBK data
CDMRSlot::writeModemSlot2() - Handles slot 2 data from modem (rxhot receives here)
CDMRSlot::writeNetwork()    - Sends data to network (HBLink3)
CModem::writeData()         - Queues data for TX on modem board
```

### CSBK Response Packet Structure
A minimal Aloha/Grant response for DMR Tier II:
```
Byte 0:    0x99  (opcode 0x19 = Aloha, C_LAST=1 → 0x80|0x19=0x99)
Byte 1:    0x00  (reserved)
Byte 2:    0x00  (MS Info)
Byte 3:    0x00  (reserved)
Bytes 4-6: <destination ID - radio's ID: 31 10 0F>
Bytes 7-9: <source ID - repeater ID: from MMDVM.ini>
Byte 10:   0x00
Byte 11:   0x00
```
This is then BPTC(196,96) encoded and sent as a DMR data burst on TS2.

---

## Build Instructions

### On txhot (cross-compile for speed):
```bash
# WSL2 on Windows - already set up
cd /mnt/c/users/pistar/MMDVMHost
# Make changes to source files
make CXX=aarch64-linux-gnu-g++ CC=aarch64-linux-gnu-gcc -j$(nproc)
# Use Docker container for correct glibc:
docker run --rm -v "$(pwd):/build" debian:trixie bash -c \
  "apt-get update -qq && apt-get install -y -qq gcc-aarch64-linux-gnu \
   g++-aarch64-linux-gnu make && cd /build && make clean && \
   make CXX=aarch64-linux-gnu-g++ CC=aarch64-linux-gnu-gcc -j$(nproc)"
```

### Transfer and deploy:
```bash
scp MMDVMHost pi-star@192.168.2.202:~/MMDVMHost/MMDVMHost
scp MMDVMHost pi-star@192.168.2.4:~/MMDVMHost/MMDVMHost
sudo systemctl restart mmdvmhost.service
```

---

## Testing

### Confirm fix works:
1. Key up DR1801UV on 439.9875 MHz, CC1, TS2, TG14260
2. Watch rxhot journal: `sudo journalctl -fu mmdvmhost.service`
3. Should see: `DMR Slot 2, received RF voice header` instead of repeated `Downlink Activate CSBK`
4. txhot should retransmit on 449.9875 MHz
5. OpenGD77 or Baofeng listening on 449.9875 MHz should decode audio

### Confirm no regression:
- OpenGD77 (which already works) should continue to work normally
- HBLink3 bridge should continue routing TG14260 TS2

---

## Files Reference

| Location | File | Purpose |
|----------|------|---------|
| rxhot/txhot `~/MMDVMHost/` | `DMRCSBK.cpp` | CSBK encode/decode |
| rxhot/txhot `~/MMDVMHost/` | `DMRSlot.cpp` | Main DMR slot handler |
| rxhot/txhot `~/MMDVMHost/` | `DMRSlotRX.cpp` | Hotspot RX path |
| rxhot/txhot `~/MMDVMHost/` | `Modem.cpp` | Modem board interface |
| rxhot `~/MMDVMHost/` | `MMDVM.ini` | Config (Duplex=0, RX=439987500) |
| txhot `~/MMDVMHost/` | `MMDVM.ini` | Config (Duplex=0, TX=449987500) |
| txhot `~/hblink3/` | `bridge.py` | HBLink3 bridge |
| txhot `~/hblink3/` | `bridge_rules.py` | TG14260 routing rules |

---

## Notes

- MMDVMHost git commit in use: `eb79830` (Oct 10, 2023)
- Firmware on both boards: `Nano_hotSPOT-v1.6.1 20231002_WPSD`, dual ADF7021
- Both boards use `/dev/ttyAMA0` at 115200 baud
- Serial console and getty on ttyAMA0 have been **disabled** on both boards
- Both boards are in `dialout` group for serial access
- HBLink3 REPORT port disabled (`REPORT: False`) to prevent startup crash
- The DR1801UV radio ID is **3215375** (seen in CSBK source bytes `31 10 0F`)
