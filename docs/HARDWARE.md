# Known hardware specification and evidence status

This document describes the unit validated with this firmware. It deliberately
distinguishes **confirmed** facts from board-family assumptions. Do not use an
assumption as a reason to flash a different device.

## Confirmed for the validated unit

| Area | Known value | Evidence / status |
| --- | --- | --- |
| MCU/module | SiFli `SF32LB52-MOD-1-N16R8`; SF32LB52 family | Module marking visible on the PCB; firmware target is `SF32LB52` |
| External flash | 16 MiB NOR, mapped from `0x12000000` | `ptab.json` and built `sftool_param.json` agree on the NOR partition map |
| External RAM | 8 MiB PSRAM region | `N16R8` module designation and board partition configuration |
| Displays | Three independent 128 × 128 colour TFT modules | Physical unit and `gc9107_Multi_screen` driver |
| Display controller | GC9107-family driver | Driver and working physical rendering; panel glass/FPC revision may vary |
| Display transport | Shared serial display bus, three independent chip-selects | Board driver and verified three-panel operation |
| Input | Three momentary keys and one push/rotary encoder | Physical unit and input firmware |
| USB | USB-C runtime connection with HID and CDC functions | Firmware composite USB implementation and running companion link |
| Download bridge | CH340-class USB-to-UART interface on the development/programming path | Physical PCB marking and observed `cu.usbserial` enumeration |
| Storage | microSD socket present on the PCB | Physical PCB observation; not required by this dashboard |

## NOR flash layout used by this release

All values are byte addresses in the MCU's NOR mapping. The release writer
uses exactly these files and addresses:

| Region | Start | Size / maximum region | Release file |
| --- | ---: | ---: | --- |
| Flash table | `0x12000000` | `0x00008000` | `ftab.bin` |
| Bootloader | `0x12208000` | `0x00010000` | `bootloader.bin` |
| HCPU code | `0x12218000` | `0x00240000` | `ER_IROM1.bin` |
| Asset/EZIP data | `0x12460000` | `0x00200000` | `ER_IROM3.bin` |
| Font data | `0x12660000` | `0x00500000` | `ER_IROM2.bin` |
| File system | `0x12B60000` | `0x00400000` | preserved / not written by the release helper |

The writer intentionally does not overwrite the file-system region. Existing
persistent custom-key configuration normally survives a release flash, but
users should still export/record any important key mappings first.

## Runtime behavior

* The USB HID interface emits keyboard/consumer controls.
* The USB CDC command parser accepts dashboard and `custom_key` values from the
  companion service.
* Firmware requests a first dashboard update with `REQ:codex` after runtime
  USB startup.
* The encoder is configured for consumer-volume up/down in this firmware.
* Buttons are stored in the SuperKey custom-key file system. The companion's
  defaults are Ctrl, Enter, and Delete from left to right.

## Items intentionally not claimed as verified

* There is no labelled physical BOOT key on the photographed PCB. The exact
  download-mode entry method depends on the board revision and host tooling.
* Individual MCU GPIO assignments should be taken from the checked-in board
  code, not inferred from this table. A complete schematic/netlist has not
  been independently published with this release.
* Panel FPC markings alone do not prove every electrical property of a display
  revision. This firmware is validated against the three-panel unit described
  here, not every GC9107 module sold online.
* The microSD, RGB LED, and environmental-sensor capabilities belong to the
  SuperKey board family but are not required for the Codex dashboard workflow.

## Compatibility gate

Use this image only when all of these are true:

1. The module is SF32LB52/SuperKey-family hardware matching the photographs.
2. The board has the same three 128 × 128 panels, three switches, and encoder.
3. The release manifest reports **NOR** and the five `0x12xxxxxx` addresses.
4. The programmer reports a clean verification for every image.

If any condition fails, stop and ask for a board-specific build rather than
forcing this image.
