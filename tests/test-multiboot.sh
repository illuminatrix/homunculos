#!/bin/bash
# Test: Multiboot header compliance
# The .multiboot section is merged into .text by the linker;
# the header is at the start of .text.
source "$(dirname "$0")/helpers.sh"

check_deps objdump xxd || exit 1

echo "=== Multiboot Compliance Test ==="

if [ ! -f "$KERNEL_BIN" ]; then
	fail "kernel.bin not found at $KERNEL_BIN"
	exit 1
fi

# Get file offset of .text section (where .multiboot is merged)
off=$(objdump -h "$KERNEL_BIN" 2>/dev/null | awk '/\.text/ {print $6; exit}')
if [ -z "$off" ]; then
	# Try hex without leading zeros
	off=$(objdump -h "$KERNEL_BIN" 2>/dev/null | awk '/\.text/ {print $6; exit}')
fi

if [ -z "$off" ]; then
	fail "Could not find .text section offset"
	exit 1
fi

# Read the first 12 bytes of .text (magic, flags, checksum)
header=$(xxd -s "0x$off" -l 12 "$KERNEL_BIN" 2>/dev/null) || {
	fail "Failed to read kernel.bin at offset $off"
	exit 1
}

# Extract hex bytes from xxd output
# xxd output format: "00001000: 02b0 ad1b 0300 0000 fb4f 52e4  ..."
# awk splits by whitespace: $2=$3=$4... are hex groups
hex_groups=($(echo "$header" | awk '{print $2, $3, $4, $5, $6, $7}'))
magic_bytes="${hex_groups[0]}${hex_groups[1]}"     # 02b0ad1b  (4 bytes)
flags_bytes="${hex_groups[2]}${hex_groups[3]}"     # 03000000  (4 bytes)
checksum_bytes="${hex_groups[4]}${hex_groups[5]}"   # fb4f52e4  (4 bytes)

echo "  Header at file offset 0x$off"
echo "  Raw bytes: $magic_bytes $flags_bytes $checksum_bytes"

# Convert little-endian hex string to value
le2val() {
	echo "$((16#${1:6:2}${1:4:2}${1:2:2}${1:0:2}))"
}

magic=$(le2val "$magic_bytes")
flags=$(le2val "$flags_bytes")
checksum=$(le2val "$checksum_bytes")

# Validate magic
expected_magic=$((16#1BADB002))
if [ "$magic" -ne "$expected_magic" ]; then
	fail "Multiboot magic mismatch: got 0x$(printf '%08X' $magic), expected 0x1BADB002"
	exit 1
fi

# Validate flags (bit 0 = align, bit 1 = meminfo)
if [ "$flags" -ne 3 ]; then
	fail "Multiboot flags mismatch: got $flags, expected 3 (ALIGN | MEMINFO)"
	exit 1
fi

# Validate checksum: magic + flags + checksum == 0 (mod 2^32)
sum=$(( (magic + flags + checksum) & 0xFFFFFFFF ))
if [ "$sum" -ne 0 ]; then
	fail "Multiboot checksum mismatch: magic+flags+checksum = 0x$(printf '%08X' $sum), expected 0"
	exit 1
fi

pass "Multiboot header valid (magic=0x1BADB002, flags=$flags)"
