#!/bin/bash
# Test: Kernel boots and prints welcome message to VGA
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== Boot Test ==="

qemu_start || { fail "QEMU failed to start"; exit 1; }

if wait_for_boot 15; then
	pass "Boot message 'HomunculOS Kernel!' found in VGA"
else
	dump_head=$(vga_text | head -c 80)
	fail "Boot message not found. VGA: [$dump_head]"
	qemu_stop
	exit 1
fi

qemu_stop
