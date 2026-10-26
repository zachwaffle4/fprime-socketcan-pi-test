# fprime-socketcan Pi test bench

Tests [fprime-socketcan](https://github.com/TeraLink-1/fprime-socketcan) (`SocketCan.LinuxCanDriver`)
between two Raspberry Pi 5s, each with a Waveshare 2-CH CAN HAT+ (2x MCP2515).

The same `PiCanTest` deployment runs on both Pis. A `CanTester` component sits on top of the driver
and can ping, echo, and burst frames. You drive each Pi from its own GDS.

```
 Pi A                                      Pi B
 +--------------------+                    +--------------------+
 | PiCanTest          |                    | PiCanTest          |
 |  CanTester         |                    |  CanTester         |
 |  LinuxCanDriver    |                    |  LinuxCanDriver    |
 |  can0 (MCP2515)    |                    |  can0 (MCP2515)    |
 +----CAN_H---CAN_L---+                    +----CAN_H---CAN_L---+
        |      |_______________________________|      |
        |______________________________________________|     (plus GND)
```

## Layout

| Path | What |
|------|------|
| `PiCanTest/Components/CanTester` | Test component: ping/echo, bursts with sequence checking, counters |
| `PiCanTest/Top` | Topology: driver, `CanTester`, rate groups, GDS link (`Drv.TcpServer`) |
| `lib/fprime` | F´ v4.4.0 (submodule) |
| `lib/fprime-socketcan` | The library under test (submodule) |
| `scripts/` | Pi setup, builds, deploy, GDS launcher, raw CAN checks |
| `container/` | arm64 Debian build image for building on the Mac |

## 1. Hardware

- Set the HAT's **VIO jumper to 3.3 V** for a Pi.
- Wire channel to channel between the two HATs: `CAN_H` to `CAN_H`, `CAN_L` to `CAN_L`, and the
  `G` terminals together. Never swap H and L.
- **Termination:** a two-node bus needs 120 Ω at both ends, so enable the HAT's onboard 120 Ω
  jumper on both. Waveshare's wiki doesn't say which jumper position enables it, so check the
  silkscreen or schematic. Measure about 60 Ω between H and L with the bus unpowered.
- Use the standoff that comes with the HAT so the terminals don't short against the HDMI ports.

## 2. Configure each Pi (once)

On each Pi, with this repo cloned (`git clone --recurse-submodules` is not needed, the build script
fetches what it needs):

```bash
scripts/pi-bootstrap.sh          # can-utils, iproute2, build tools
scripts/enable-can-overlay.sh    # adds the MCP2515 overlays to config.txt, keeps a backup
sudo reboot
```

After the reboot, check the controllers came up:

```bash
dmesg | grep -i mcp251
ip -br link show type can        # expect can0 and can1
```

`enable-can-overlay.sh` uses the lines from [Waveshare's 2-CH CAN HAT+ wiki](https://www.waveshare.com/wiki/2-CH_CAN_HAT+):
`spi1-3cs` plus one `mcp2515` overlay per channel (`spi1-1` and `spi1-2`), 16 MHz oscillators,
interrupts on GPIO22 (channel 0) and GPIO13 (channel 1). Those are the HAT's default interrupt
pins; if you move them in hardware, override `INT_CAN0`/`INT_CAN1`. **Treat the overlay as
unverified on your hardware** until `dmesg` shows both controllers. The `can0`/`can1` names are
assigned in probe order, so confirm which physical channel is which with `can-sanity.sh`.
`OSCILLATOR` can be overridden the same way.

## 3. Check the bus without F´

Bring the interface up on both Pis. Every node must use the same bitrate (default 500 kbit/s,
set `BITRATE=1000000` for 1 Mbit/s):

```bash
scripts/setup-can.sh can0
```

Save the `ip -details -statistics link show can0` output it prints: it records the real bit timing.

Then:

```bash
# Pi A                                  # Pi B
scripts/can-sanity.sh dump can0         scripts/can-sanity.sh send can0
```

Pi A should print `123  [4]  DE AD BE EF`. Swap roles to check the other direction. With both
channels of one HAT wired together, `scripts/can-sanity.sh loopback can0 can1` checks a single Pi.

## 4. Build

Two options. Both produce a Linux aarch64 binary at `build-artifacts/Linux/PiCanTest/bin/PiCanTest`.

**On each Pi** (simplest; slow on the first build):

```bash
scripts/build-on-pi.sh
```

**On the Mac, in a container** (uses Apple's [`container`](https://github.com/apple/container) CLI,
which starts its apiserver if needed; native arm64 on Apple Silicon, so no cross toolchain.
Override `CPUS` and `MEMORY` if the defaults of 4 and 4g are wrong for your Mac):

```bash
git submodule update --init --depth 1 lib/fprime lib/fprime-socketcan
scripts/container-build.sh
scripts/deploy.sh pi@pi-a.local
scripts/deploy.sh pi@pi-b.local
```

`deploy.sh` copies the binary and `setup-can.sh` to `~/picantest` on the Pi.

A plain `fprime-util generate && fprime-util build` on the Mac works too, but macOS gets the
**stub driver** (nothing opens), so it only checks that the project compiles and produces the
dictionary for the GDS.

## 5. Run

On each Pi, bring up the interface, then start the deployment. Start Pi B with `-e` so it echoes pings:

```bash
scripts/setup-can.sh can0
./PiCanTest -i can0 -e        # Pi B: answers pings
./PiCanTest -i can0           # Pi A
```

Options: `-i` interface (default `can0`), `-e` echo from startup, `-a`/`-p` address and port the GDS
connects to (default `0.0.0.0:50000`). The deployment keeps running if the interface can't be opened
(it prints why and disables CAN), so a missing or down interface shows up as an
`InterfaceOpenFailed` / `InterfaceDown` event.

On the Mac, with the fprime venv active (`. fprime-venv/bin/activate`), one GDS per Pi:

```bash
scripts/run-gds.sh pi-a.local 5000     # http://localhost:5000
scripts/run-gds.sh pi-b.local 5001     # http://localhost:5001
```

## 6. Tests

Frame IDs: ping `0x100`, pong `0x101`, burst `0x200`. Send commands from Pi A's GDS unless noted.
Reset both sides before each test with `canTester.RESET_COUNTERS` so the sequence tracking starts clean.

| Test | Do | Expect |
|------|----|--------|
| Open | Start the deployment | `can.InterfaceOpened` event on both |
| One frame | `canTester.SEND_FRAME` id `0x123`, len 4, payload `0xDEADBEEF00000000` | Pi B `FrameReceived`, `OtherRx` +1; `can.FramesTx` +1 on A, `FramesRx` +1 on B |
| 29-bit ID | `SEND_FRAME` with `extended` true, id `0x1ABCDE01` | Same, with `ext true` in the event |
| Ping | B: `SET_ECHO true` (or `-e`). A: `START_PING 20` | 20 `PongReceived` events; `PongsReceived` = 20; `MinRttUs`/`MaxRttUs` filled |
| Burst | A: `SEND_BURST 10000 false` | A `BurstDone` with 10000 sent; B `BurstRx` = 10000, `BurstLost` = 0 |
| Burst, 29-bit | A: `SEND_BURST 10000 true` | Same counts; `candump` shows 8-digit IDs |
| Backpressure | On both Pis `BITRATE=125000 scripts/setup-can.sh can0`, and on A `TXQUEUELEN=10` as well; burst 10000 | `BurstDone` shows `TX queue full` > 0, `can.TxBusy` > 0, no `SendError` |
| No peer | Stop Pi B's deployment, then `START_PING 5` on A | `can.NoAck` events, `BusState` to `ERROR_PASSIVE`; TEC rising if the kernel reports it |
| Peer returns | Start B again | `BusState` should return to `ERROR_ACTIVE` and pings are answered |
| Bus-off | Short H to L briefly while A sends | `BusOff`, then `BusRestarted` after `restart-ms` (100 ms) |
| Restart | Restart A's deployment during a burst | A reopens cleanly; B's burst counters show the gap |
| Raw load | `scripts/can-sanity.sh flood can0 100000` on one Pi | Other Pi's `can.SocketRxDrops`, `can.IfRxOverErrors` stay 0 |

Notes on reading results:

- `canTester.BurstLost` counts gaps in sequence numbers, so it only sees loss between frames that
  arrived. Frames lost at the very end of a burst show up as a shortfall in `BurstRx` instead.
- A sequence number lower than expected is treated as the sender restarting, not as loss.
- `LastRttUs` is measured on one clock (the sender's), so it needs no time sync between the Pis.
  It includes the echo node's queueing delay. Pings go out on the 1 Hz rate-group tick, but the
  timestamp is taken when each ping is sent, so the tick doesn't inflate the round trip.
- A burst takes the whole 1 Mbit/s bus if the bitrate is set that high. At 500 kbit/s a worst-case
  frame is about 270 µs, so expect roughly 3,700 frames/s.
- The MCP2515 sits on SPI and has small buffers (2 TX, 2 RX), so it is a harsher test of the
  driver's drop and backpressure handling than a Zynq AXI CAN will be. Don't read Pi limits as
  Q7 limits.

## Status

What has been checked, and what hasn't:

- Verified: the project generates and builds on macOS against the stub driver.
- Not yet verified: the Linux build (needs a Linux host or the container build), the overlay lines on a Pi 5,
  and every test above on real hardware.
