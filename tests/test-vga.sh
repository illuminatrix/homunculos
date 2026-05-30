#!/bin/bash
# Test: VGA text output behavior
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== VGA Output Test ==="

qemu_start_with_disk 10 || { fail "QEMU failed to start"; exit 1; }

errors=0

if ! wait_for_boot 15; then
	fail "Boot message not found in VGA"
	qemu_stop
	exit 1
fi

sleep 2

# Test 1: Welcome message in VGA
if vga_contains "Illuminatrix Kernel"; then
	pass "Welcome message present in VGA text buffer"
else
	vga_text | head -c 80 | cat -A
	fail "Welcome message not found in VGA"
	errors=$((errors + 1))
fi

# Test 2: Shell prompt in VGA
if vga_contains ">"; then
	pass "Shell prompt '>' present in VGA text buffer"
else
	dump=$(vga_text | tail -c 160)
	fail "Shell prompt not found. VGA tail: [$dump]"
	errors=$((errors + 1))
fi

# Test 3: VGA buffer is non-empty
raw=$(vga_dump)
if [ -n "$raw" ] && echo "$raw" | grep -v '(qemu)' | grep -v '^$' | head -5 | grep -q '0x'; then
	pass "VGA memory at 0xB8000 contains valid data"
else
	fail "VGA dump returned no data or unexpected format"
	errors=$((errors + 1))
fi

# Test 4: VGA attribute at position 0 is 0x0F (white on black)
attr_check=$(monitor_cmd "xp /2bx 0xB8000" 2>/dev/null | grep -E '^[[:space:]]*[0-9a-f]' | head -1)
attr_byte=$(echo "$attr_check" | grep -oE '0x[0-9a-fA-F]{2}' | tail -1)
attr_byte="${attr_byte#0x}"
if [ "$attr_byte" = "0f" ] || [ "$attr_byte" = "0F" ]; then
	pass "VGA attribute at position 0 is 0x0F (white on black)"
else
	fail "VGA attribute at position 0 is 0x$attr_byte, expected 0x0F"
	errors=$((errors + 1))
fi

qemu_stop
exit $errors
