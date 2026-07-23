# AI-assisted flashing guide

This repository is designed so a beginner can ask an AI coding assistant to
perform a repeatable flash **without giving it permission to guess**.

## What you need

* This exact SuperKey/SF32LB52 three-screen hardware family.
* A USB data cable and a macOS or Linux machine.
* SiFli `sftool` available on `PATH` (or an AI assistant that can install it).
* A release image set from this repository; all five files must have matching
  release tags.

## Safe prompt to give an AI assistant

Copy this prompt exactly, then attach the release archive or work inside a
clone of this repository:

> I have the documented SF32LB52 SuperKey three-screen device. Read
> `README.md`, `docs/HARDWARE.md`, and `docs/RELEASE_MANIFEST.md` first.
> Do not erase or flash anything until you have listed the serial ports and I
> confirm the exact download-mode port. Use only the five release files and
> the addresses in the manifest. Run the release helper with `--verify`/write
> verification enabled. Do not substitute NAND addresses, a one-file image, or
> any image from another SF32LB52 board. After flashing, report every verified
> region and wait for me to photograph the three displays before making UI
> changes.

## Exact release write

After the user confirms the download-mode port, the command is:

```bash
./scripts/flash-codex-dashboard.sh --port /dev/cu.usbserial-XXXX --yes
```

The script requires `sftool`, checks all five release images, and calls:

```bash
sftool -c SF32LB52 -m nor -p /dev/cu.usbserial-XXXX -b 500000 \
  --connect-attempts 5 write_flash --verify \
  bootloader.bin@0x12208000 \
  ER_IROM1.bin@0x12218000 \
  ER_IROM3.bin@0x12460000 \
  ER_IROM2.bin@0x12660000 \
  ftab.bin@0x12000000
```

The script uses named image paths from the release directory; the shortened
command above documents only the address contract.

## Download mode

The physical board photographed for this project has no labelled BOOT button.
It exposes a CH340-class UART path. Entering SiFli download mode is therefore
board-revision/tooling dependent. If `sftool` cannot connect, stop—do not keep
resetting, erase the flash, or flash the runtime USB CDC port. Obtain the
specific board's download-mode procedure first.

## After a successful write

1. Disconnect and reconnect the board normally.
2. Confirm the three panels are on and show dashboard content.
3. Install `superkey-codex-companion` from its public repository.
4. Edit its TOML configuration for city and optional button mappings.
5. Restart the user LaunchAgent and inspect its log.

## Failure boundaries

Stop and seek help if the chip/memory is reported as NAND, a release file is
missing, the serial port is ambiguous, the verification phase fails, the board
does not reboot normally, or the image does not match this exact hardware.
