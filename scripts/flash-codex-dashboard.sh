#!/usr/bin/env bash
# Write one matching SuperKey Codex Dashboard release and verify every region.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RELEASE_DIR="${RELEASE_DIR:-$ROOT/release}"
PORT=""
CONFIRM=""

usage() {
  cat <<'EOF'
Usage: scripts/flash-codex-dashboard.sh --port /dev/cu.usbserial-XXXX --yes

Set RELEASE_DIR to a directory containing the five files listed in
docs/RELEASE_MANIFEST.md. The device must already be in the correct SiFli UART
download mode. This command writes the flash table and bootloader.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --port) PORT="${2:-}"; shift 2 ;;
    --yes) CONFIRM="yes"; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

[ -n "$PORT" ] || { echo "Refusing to guess a port." >&2; usage >&2; exit 2; }
[ "$CONFIRM" = yes ] || { echo "Refusing destructive write without --yes." >&2; exit 2; }
[ -e "$PORT" ] || { echo "Port not found: $PORT" >&2; exit 2; }

SFTOOL="${SFTOOL:-$(command -v sftool || true)}"
[ -n "$SFTOOL" ] || { echo "sftool is not on PATH." >&2; exit 127; }

need() { [ -s "$RELEASE_DIR/$1" ] || { echo "Missing release image: $RELEASE_DIR/$1" >&2; exit 1; }; }
need ftab.bin
need bootloader.bin
need ER_IROM1.bin
need ER_IROM3.bin
need ER_IROM2.bin

echo "Target port: $PORT"
echo "Release directory: $RELEASE_DIR"
echo "Chip/memory: SF32LB52 / NOR"

"$SFTOOL" -c SF32LB52 -m nor -p "$PORT" -b 500000 --connect-attempts 5 \
  write_flash --verify \
  "$RELEASE_DIR/bootloader.bin@0x12208000" \
  "$RELEASE_DIR/ER_IROM1.bin@0x12218000" \
  "$RELEASE_DIR/ER_IROM3.bin@0x12460000" \
  "$RELEASE_DIR/ER_IROM2.bin@0x12660000" \
  "$RELEASE_DIR/ftab.bin@0x12000000"

echo "Write verification completed. Disconnect/reconnect the device normally."
