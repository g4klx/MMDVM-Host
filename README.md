These are the source files for building the MMDVM Host, the program that
interfaces to the MMDVM or DVMega on the one side, and a suitable network (via a
gateway) on the other. It supports D-Star, DMR, P25 Phase 1, NXDN, System Fusion,
POCSAG, and FM on the MMDVM, and D-Star, DMR, and System Fusion on the DVMega.

On the D-Star side the MMDVM Host interfaces with the D-Star Gateway, on DMR it
connects to the DMR Gateway to allow for connection to multiple DMR networks.
On System Fusion it connects to the DG-Id or YSF Gateway to allow
access to the FCS and YSF networks. On P25 it connects to the P25 Gateway. On
NXDN it connects to the NXDN Gateway which provides access to the NXDN and
NXCore talk groups. It uses the DAPNET Gateway to access DAPNET to receive
paging messages for transmission using POCSAG.

It builds on 32-bit and 64-bit Linux as well as on Windows using Visual Studio
2022 on x86 and x64.

This software is licenced under the GPL v2 and is primarily intended for amateur and
educational use.

## Duplex DMR control calls

The optional `[DMR]` setting `DuplexControlTG` selects one RF talkgroup whose
group voice calls may be forwarded to the gateway while the same slot is
receiving a network voice call. It defaults to `0` (disabled); valid talkgroup
IDs are 1 through 16777215. For a BrandMeister unlink command, use:

```ini
[DMR]
DuplexControlTG=4000
```

This applies only in duplex mode and retains the normal source-ID and per-slot
talkgroup access checks. Private calls, other talkgroups and network data calls
do not get the exception. Both ordinary headers and embedded-LC late entry are
supported. A control call admitted during a downlink is forwarded to the network
only, through its RF terminator; it is not repeated over RF if the downlink ends
first. Calls begun on an idle slot retain normal behavior. Gateway rewrite rules
still apply.

The setting permits delivery of the command; interrupting an existing network
route and generating an unlink announcement are decisions made by the gateway
or master. In particular, delivery of TG4000 to a BrandMeister master does not
by itself guarantee immediate termination of the current downlink.

A Linux regression test exercises the slot decoder and call lifecycle without
opening a modem or gateway socket:

```sh
make test-dmr-duplex-control
```
