#!/usr/bin/env sh
# Requires Espressif's Xtensa ESP32-S3 GCC cross-compiler, esp-15.2.0_20251204.
set -eu
cd "$(dirname "$0")"
tool_prefix="${XTENSA_PREFIX:-xtensa-esp32s3-elf-}"
mkdir -p build
# source/text_pad.S must stay LAST: it rounds .text up to a multiple of 4 bytes.
"${tool_prefix}gcc" -std=c99 -Os -Wall -Wextra -Werror -mlongcalls \
    -ffreestanding -fno-builtin -fno-stack-protector -fno-jump-tables -fno-tree-loop-distribute-patterns \
    -nostartfiles -nostdlib -fPIC -shared -e app_main \
    -fdata-sections -ffunction-sections -Wl,--gc-sections \
    -fvisibility=hidden -Wl,--strip-all -Wl,--strip-debug \
    -Wl,--strip-discarded -Wl,-u,invader_text_pad \
    source/tengu_knight.c source/text_pad.S \
    -o build/tengu_knight.unstripped.elf
"${tool_prefix}strip" --strip-unneeded \
    --remove-section=.comment --remove-section=.dynamic \
    --remove-section=.got.loc --remove-section=.xt.lit \
    --remove-section=.xt.prop --remove-section=.xtensa.info \
    -o tengu_knight.elf build/tengu_knight.unstripped.elf
text_size=$("${tool_prefix}size" -A tengu_knight.elf | awk '$1==".text"{print $2}')
if [ $((text_size % 4)) -ne 0 ]; then
    rm -f tengu_knight.elf
    echo "ERROR: .text is $text_size bytes, not a multiple of 4. Do not run this ELF." >&2
    exit 1
fi
printf 'Built tengu_knight.elf (.text = %s bytes, word-aligned)\n' "$text_size"
