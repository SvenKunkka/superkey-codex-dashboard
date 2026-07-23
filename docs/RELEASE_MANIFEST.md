# Release manifest

Every `codex-dashboard-*` release must contain these five matching files:

| File | Address | Required |
| --- | ---: | --- |
| `ftab.bin` | `0x12000000` | yes |
| `bootloader.bin` | `0x12208000` | yes |
| `ER_IROM1.bin` | `0x12218000` | yes |
| `ER_IROM3.bin` | `0x12460000` | yes |
| `ER_IROM2.bin` | `0x12660000` | yes |

The target is `SF32LB52`, memory mode `NOR`, at 500000 baud for the proven
release process. All five files come from the same SCons build directory and
must be written with `sftool write_flash --verify`.

This is intentionally not compatible with the older `SF32LB52 + NAND`
addresses (`0x620xxxxx`) sometimes found in other macro-pad examples.
