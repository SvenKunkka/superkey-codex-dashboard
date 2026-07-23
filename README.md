# SuperKey Codex Dashboard Firmware

Public, reproducible firmware for the three-screen SuperKey desktop companion.
It targets the photographed **SiFli SF32LB52 SuperKey** hardware: three 128 ×
128 colour LCDs, three programmable buttons, and one rotary encoder.

This repository is for a specific hardware family. It is not a universal
SF32LB52 image. Read [Hardware](docs/HARDWARE.md) and
[AI-assisted flashing](docs/AI_FLASHING.md) before writing a device.

## What is on the device

| Display | Purpose |
| --- | --- |
| Left | City, local time, date, temperature, and weather condition |
| Centre | Codex state, elapsed time, and a short status detail |
| Right | Codex remaining-usage percentage, active window, progress bar, and reset date |

Default physical controls, left to right:

* Button 1: **Ctrl**
* Button 2: **Enter**
* Button 3: **Delete**
* Encoder: **system volume** (clockwise up, counter-clockwise down)

The buttons use the SuperKey persistent custom-key store and can be remapped by
the matching macOS companion without reflashing.

## Fast path: use a release

The [Releases](../../releases) page contains a complete, matching image set.
It must be written as five files at the exact addresses below; do not flash a
single `main.bin` from another SuperKey board.

```text
ftab.bin                 0x12000000
bootloader.bin           0x12208000
ER_IROM1.bin             0x12218000
ER_IROM3.bin             0x12460000
ER_IROM2.bin             0x12660000
```

On macOS, install the SiFli `sftool` binary, put the board in its UART download
mode, then run the checked-in helper with an explicit serial port:

```bash
./scripts/flash-codex-dashboard.sh --port /dev/cu.usbserial-XXXX --yes
```

The helper verifies every write. It refuses to guess a port and never performs
an erase-only operation. Download mode is board-specific; the photographed PCB
has no labelled BOOT push button. See [AI-assisted flashing](docs/AI_FLASHING.md)
for a safe prompt and recovery rules.

## Install the macOS companion

The device becomes useful after the normal runtime USB connection is present:

```bash
git clone https://github.com/cherish6/superkey-codex-companion.git
cd superkey-codex-companion
./scripts/install-service.sh
```

The service pushes time, weather, Codex status/usage, and button mappings. It
does not read auth tokens or upload personal data. Its README documents setup,
configuration, and privacy behavior.

## Hardware status

Confirmed and observed hardware facts are separated from assumptions. The
short version is **SF32LB52 module + 16 MiB NOR mapping + 8 MiB PSRAM mapping +
three 128 × 128 LCDs + three switches + rotary encoder + USB-C + CH340 UART**.
For the evidence status, display routing, flash map, USB modes, and what is not
yet safe to assume, read [docs/HARDWARE.md](docs/HARDWARE.md).

## Build from source

The SiFli SDK is a pinned Git submodule. Initialise it first:

```bash
git submodule update --init --recursive
export SIFLI_SDK="$PWD/SiFli-SDK"
export RTT_CC=gcc
export RTT_EXEC_PATH=/path/to/arm-none-eabi-gcc/bin
export PYTHONPATH="$SIFLI_SDK/tools/build"
export PATH="$RTT_EXEC_PATH:$PATH"
cd app/project
scons --board=sf32lb52-superkey --board_search_path=../boards -j4
```

Build output is deliberately ignored by Git. The signed-off flash layout is
defined in `app/boards/sf32lb52-superkey/ptab.json`; generated output includes
`sftool_param.json` with the authoritative five-file NOR mapping.

## Repository layout

```text
app/
  boards/sf32lb52-superkey/  exact NOR partition table and board target
  gc9107_Multi_screen/       three-panel LCD driver
  src/custom/                Codex dashboard and encoder-volume controls
  src/device/                USB HID + CDC command handling
  font/                      dashboard font assets and generated glyph sources
  project/                   SCons project and build configuration
docs/
  HARDWARE.md                observed hardware facts and uncertainty register
  AI_FLASHING.md             safe prompt and exact release flash procedure
scripts/
  flash-codex-dashboard.sh   release-image writer with verification
SiFli-SDK/                   pinned upstream SDK submodule
```

## Serial/CDC data contract

The runtime companion sends four dashboard values through the SuperKey CDC
command parser:

```text
sys_set codex_time    DATE|HH:MM|
sys_set codex_weather CITY|TEMP|CONDITION
sys_set codex_status  STATE|ELAPSED|DETAIL
sys_set codex_usage   PRIMARY|SECONDARY|LABEL|...|RESET_PRIMARY|RESET_SECONDARY
```

`custom_key` writes one persistent HID mapping per physical key. The complete
implementation is in `app/src/custom/codex_dashboard.c` and
`app/src/fs/custom_key_storage.c`.

## Safety

Flashing rewrites the partition table and bootloader. A successful serial-port
enumeration is **not** proof that a board is in download mode or that this is
the matching hardware. Stop if the tool cannot identify the exact image set,
download port, or verification result. Do not substitute a NAND image, an
unrelated SF32LB52 dev-board image, or guessed addresses.

## License and upstream work

The project is Apache-2.0; see [LICENSE](LICENSE). It builds on the public
SiFli SuperKey codebase and the OpenSiFli SDK submodule. Font assets retain
their upstream licences; see [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
