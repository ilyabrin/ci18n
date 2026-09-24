#!/bin/sh
# The tests on 8-bit AVR under QEMU. Needs avr-gcc, avr-libc,
# qemu-system-avr and a host C compiler.
#
#   tests/avr/run.sh
#
# 1. The unit tests on an ATmega2560, in shards, with CI18N_NO_COMPILED.
# 2. tests/avr/compiled.c, catalogues kept in flash, on the ATmega2560 and
#    on an Arduino Uno's ATmega328P, which has 2 KB of RAM.
# 3. A call to ci18n_get() on AVR, which must not compile.
#
# SPDX-License-Identifier: MIT

set -eu

SHARDS=${SHARDS:-12}
OUT=${OUT:-build/avr}
HOSTCC=${HOSTCC:-cc}
FLAGS="-Os -std=c99 -Wall -Wextra -Wpedantic -Werror
       -ffunction-sections -fdata-sections -Wl,--gc-sections
       -DCI18N_NO_FILES -Iinclude ${EXTRA_CFLAGS:-}"

mkdir -p "$OUT"
failed=0

# Runs an ELF on a QEMU board until it prints its summary. The board never
# powers off, so QEMU runs in the background and is stopped afterwards.
run() {
    machine=$1
    elf=$2
    log=$3
    rm -f "$log"
    qemu-system-avr -machine "$machine" -bios "$elf" -display none \
        -serial "file:$log" -monitor none &
    qemu=$!
    i=0
    while [ "$i" -lt 600 ] && ! grep -q "^Failed:" "$log" 2>/dev/null; do
        sleep 1
        i=$((i + 1))
    done
    kill "$qemu" 2>/dev/null || true
    wait "$qemu" 2>/dev/null || true
    if ! grep -q "^Failed:" "$log" 2>/dev/null; then
        echo "$elf: no summary, the program hung or crashed"
        cat "$log" 2>/dev/null || true
        exit 1
    fi
    grep "FAILED" "$log" || true
    failed=$((failed + $(awk '/^Failed:/ {print $2}' "$log")))
}

ram() {
    avr-size "$1" | awk -v what="$2" 'NR==2 {print what ": " $1 " bytes of flash, " $2 + $3 " of RAM before the heap"}'
}

# 1. The unit tests.
total=0
shard=0
while [ "$shard" -lt "$SHARDS" ]; do
    elf="$OUT/tests_$shard.elf"
    # shellcheck disable=SC2086
    avr-gcc -mmcu=atmega2560 $FLAGS -DCI18N_NO_COMPILED \
        -DTEST_SHARDS="$SHARDS" -DTEST_SHARD="$shard" -o "$elf" tests/avr/main.c
    run mega2560 "$elf" "$OUT/tests_$shard.log"
    total=$((total + $(awk '/^Total:/ {print $2}' "$OUT/tests_$shard.log")))
    shard=$((shard + 1))
done
echo "unit tests: $total in $SHARDS shards"

# 2. Catalogues in flash.
"$HOSTCC" -std=c99 -Iinclude -o "$OUT/ci18n_compile" tools/ci18n_compile.c
for l in en ru; do
    "$OUT/ci18n_compile" -o "$OUT/$l.h" "$l" "tests/avr/$l.txt"
done
for board in mega2560:atmega2560 uno:atmega328p; do
    machine=${board%%:*}
    mcu=${board#*:}
    elf="$OUT/compiled_$machine.elf"
    # shellcheck disable=SC2086
    avr-gcc -mmcu="$mcu" $FLAGS -I"$OUT" -o "$elf" tests/avr/compiled.c
    ram "$elf" "compiled catalogues on $mcu"
    run "$machine" "$elf" "$OUT/compiled_$machine.log"
    grep "checks" "$OUT/compiled_$machine.log"
done

# 3. The pointer API is a compile error, with the message that says why.
# shellcheck disable=SC2086
if avr-gcc -mmcu=atmega328p $FLAGS -o "$OUT/pointer_api.elf" tests/avr/pointer_api.c \
    2>"$OUT/pointer_api.err"; then
    echo "tests/avr/pointer_api.c compiled, and must not"
    exit 1
fi
if ! grep -q "ci18n_get_copy" "$OUT/pointer_api.err"; then
    echo "tests/avr/pointer_api.c failed for another reason:"
    cat "$OUT/pointer_api.err"
    exit 1
fi
echo "ci18n_get() on AVR: refused, pointing at ci18n_get_copy()"

echo "AVR: $failed failed"
[ "$failed" -eq 0 ]
