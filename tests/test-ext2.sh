#!/bin/bash
# Test: ext2 filesystem mounted at root /, tmpfs at /dev
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== ext2 Filesystem Test ==="
echo "  Subtest 1: Boot welcome message"
echo "  Subtest 2: 'ext2: mounted at /' appears in boot output"
echo "  Subtest 3: Shell prompt '>' appears"
echo "  Subtest 4: 'ls /' shows files on ext2"
echo "  Subtest 5: 'cat /hello.txt' shows file content"
echo "  Subtest 6: 'poweroff' shuts down QEMU (regression)"

errors=0

# --- Create partitioned ext2 disk image with /bin/shell ---
DISK_SIZE=32
PART_START=2048
rm -f "$DISK_IMG"

# Create raw ext2 partition image with /dev, /bin and test files
dd if=/dev/zero of=.part.img bs=1M count=31 2>/dev/null
mkfs.ext2 -F -E revision=0 -b 1024 .part.img 2>/dev/null
debugfs -w .part.img -R "rmdir /lost+found" 2>/dev/null

debugfs -w .part.img -R "mkdir /dev" 2>/dev/null
debugfs -w .part.img -R "mkdir /bin" 2>/dev/null

# Add shell binary
debugfs -w .part.img -R "write ${TESTS_DIR}/../shell/shell /bin/shell" 2>/dev/null

# Create a temp file with known content
TEST_CONTENT=$(mktemp)
printf "Hello from ext2!" > "$TEST_CONTENT"

# Populate partition with test files via debugfs
debugfs -w .part.img -R "mkdir /testdir" 2>/dev/null
debugfs -w .part.img -R "write $TEST_CONTENT /hello.txt" 2>/dev/null
debugfs -w .part.img -R "mkdir /nested" 2>/dev/null
debugfs -w .part.img -R "mkdir /nested/deep" 2>/dev/null
printf "deep file" > "${TEST_CONTENT}2"
debugfs -w .part.img -R "write ${TEST_CONTENT}2 /nested/deep/secret.txt" 2>/dev/null
rm -f "$TEST_CONTENT" "${TEST_CONTENT}2"

# Create MBR-partitioned disk image
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE 2>/dev/null
printf '2048,,L,*\n' | sfdisk "$DISK_IMG" 2>/dev/null
dd if=.part.img of="$DISK_IMG" bs=512 seek=$PART_START conv=notrunc 2>/dev/null
rm -f .part.img

# --- Start QEMU with disk ---
qemu_start_with_disk || { fail "QEMU failed to start"; exit 1; }

# --- Subtest 1: Boot ---
if wait_for_boot 15; then
	pass "Boot message 'Illuminatrix' found (kernel boots OK)"
else
	fail "Boot message not found (timeout 15s)"
	qemu_stop
	exit 1
fi

sleep 2

# --- Subtest 2: ext2 mount message ---
if vga_contains "ext2: mounted at /"; then
	pass "'ext2: mounted at /' found in boot output"
else
	dump=$(vga_text)
	fail "'ext2: mounted at /' not found. VGA: [$(echo "$dump" | tail -c 200)]"
	errors=$((errors + 1))
fi

# --- Subtest 3: Shell prompt ---
if vga_contains ">"; then
	pass "Shell prompt '>' found in VGA memory"
else
	dump=$(vga_text | tail -c 120)
	fail "Shell prompt '>' not found. VGA tail: [$dump]"
	errors=$((errors + 1))
fi

# --- Subtest 4: ls / ---
for ch in l s spc slash; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

vga_full=$(vga_text)
if echo "$vga_full" | grep -q "hello.txt"; then
	pass "'ls /' shows 'hello.txt' on ext2"
else
	fail "'ls /' missing expected entries. VGA: [$(echo "$vga_full" | tail -c 200)]"
	errors=$((errors + 1))
fi

# --- Subtest 5: cat /hello.txt ---
for ch in c a t spc slash h e l l o dot t x t; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

vga_full=$(vga_text)
if echo "$vga_full" | grep -q "Hello from ext2"; then
	pass "'cat /hello.txt' printed file contents correctly"
else
	vga_escaped=$(echo "$vga_full" | sed 's/[[:space:]]/\\s/g' | head -c 400)
	fail "'Hello from ext2' not found. VGA trimmed: [$vga_escaped]"
	# Dump raw hex around line where "hello.txt" should appear
	raw=$(vga_dump)
	# Look for "Hello" in raw hex
	if echo "$raw" | grep -q "0x48"; then
		pass "  (found 'H' in VGA - might be timing issue)"
	fi
	errors=$((errors + 1))
fi

# --- Subtest 6: poweroff regression ---
for ch in p o w e r o f f; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU (regression OK)"
else
	if qemu_is_running; then
		fail "QEMU still running after 'poweroff' (timeout 10s)"
		errors=$((errors + 1))
	else
		pass "'poweroff' shut down QEMU"
	fi
fi

# --- Summary ---
if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED (see above)"
fi

qemu_stop
rm -f "$DISK_IMG"
exit $errors
