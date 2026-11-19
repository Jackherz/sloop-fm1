#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# SLOOP JR: rank the firmware's modules by compiled code size, on the host, with no toolchain.
#
#   tools/measure_code.sh
#
# The firmware is one translation unit, so per-file numbers do not fall out of the linker. DWARF does
# record which .c file every instruction came from, so this builds two host binaries that together
# include the whole firmware (the DSP and the UI one way, the storage / editor / OTA path the other),
# reads the line tables, and adds up the address range each file owns.
#
# The absolute bytes are x86-64, not pi32v2: they rank modules, they do not size the image. Use the
# shares to choose what to cut, then confirm with tools/measure_flash.sh on a machine with the
# JieLi toolchain.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUT=build/host
GEN=build/gen
mkdir -p "$OUT" /tmp/sloop_measure/gen

for h in felucca_tables.h felucca_font.h felucca_icons.h felucca_drumkits.h felucca_fm6.h sloop_logo.h felucca_samples.h; do
    [ -f "$GEN/$h" ] || {
        case $h in
            felucca_tables.h)        python3 tools/gen_tables.py "$GEN/$h" ;;
            felucca_font.h)          python3 tools/gen_font.py "$GEN/$h" ;;
            felucca_icons.h)         python3 tools/gen_icons.py "$GEN/$h" ;;
            felucca_drumkits.h)      python3 tools/gen_drumkits.py "$GEN/$h" ;;
            felucca_fm6.h)           python3 tools/gen_fm6_patches.py "$GEN/$h" ;;
            sloop_logo.h)            python3 tools/gen_logo.py "$GEN/$h" ;;
            felucca_samples.h)       python3 tools/gen_samples.py "$GEN/$h" ;;
        esac
    }
done

# the editor harness reads SMP_DATA through a pointer under -fsanitize=bounds (tests/editor_fuzz.sh)
{ echo '#define SMP_DATA (smp_data_v)'
  sed 's/^static const uint8_t SMP_DATA\[/static const uint8_t SMP_DATA_ARR[/' "$GEN/felucca_samples.h"
  echo 'static const uint8_t *volatile smp_data_v = SMP_DATA_ARR;'; } > /tmp/sloop_measure/gen/felucca_samples.h
for h in felucca_tables.h felucca_font.h felucca_icons.h felucca_drumkits.h felucca_fm6.h sloop_logo.h; do
    cp "$GEN/$h" /tmp/sloop_measure/gen/
done

echo "building the two host proxies..." >&2
$CC -O2 -g -w -I"$GEN" -Ifirmware/src -Ifirmware/hal -o "$OUT/code_ui" tests/ui_pages_test.c -lm
$CC -O2 -g -w -I"$GEN" -Ifirmware/src -o "$OUT/code_dsp" tests/hostsim.c -lm
$CC -O1 -g -w -fsanitize=undefined -fno-sanitize=shift-base,signed-integer-overflow \
    -include firmware/hal/fm1_flash_id.h -I/tmp/sloop_measure/gen -I"$GEN" -Ifirmware/src -Ifirmware/hal \
    -o "$OUT/code_fw" tests/editor_fuzz.c -lm

python3 - "$OUT/code_ui" "$OUT/code_dsp" "$OUT/code_fw" <<'EOF'
import subprocess, re, sys, collections
def attr(exe):
    txt = subprocess.run(["objdump", "--dwarf=decodedline", exe], capture_output=True, text=True).stdout
    seq = []
    for ln in txt.splitlines():
        m = re.match(r"^(\S+)\s+(\d+)\s+(0x[0-9a-f]+)\s*$", ln.strip())
        if m:
            seq.append((int(m.group(3), 16), m.group(1)))
        elif ln.strip().endswith("-"):
            seq.append((None, None))
    s, last = collections.Counter(), None
    for a, f in seq:
        if last and last[1] and a and last[0] and a > last[0] and a - last[0] < 4096:
            s[last[1]] += a - last[0]
        last = (a, f)
    return s
harness = {"ui_pages_test.c", "hostsim.c", "editor_fuzz.c", "editor_host.c", "editor_repro.c", "cpu1_host.c", "?"}
tot = collections.Counter()
for exe in sys.argv[1:]:
    for f, v in attr(exe).items():
        if f not in harness:
            tot[f] = max(tot[f], v)
S = sum(tot.values())
print(f"whole firmware, host proxy: {S:,} B of code ({len(tot)} files)\n")
print(f"{'module':22}{'code B':>9}{'share':>8}")
for f, v in sorted(tot.items(), key=lambda x: -x[1]):
    if v >= 500:
        print(f"{f:22}{v:>9,}{100*v/S:>7.1f}%")
print("\n-- the candidate cuts --")
for c in ("ui_studio.c", "ui_vis.c", "punch.c", "arranger_scene.c", "ui_song.c",
          "editor.c", "editor_live.c", "editor_fm6.c", "editor_dsyn.c", "ui_menu.c"):
    print(f"{c:22}{tot.get(c,0):>9,}{100*tot.get(c,0)/S:>7.1f}%")
EOF
