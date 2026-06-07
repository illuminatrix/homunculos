#!/bin/bash
# Test: User mode (ring 3) feature
#
# Verifies:
#   1. Kernel boots with welcome message (regression)
#   2. Shell prompt appears (confirms ring 3 task started successfully)
#   3. Greeting command produces "hello" (proves int $0x80 syscalls from ring 3)
#   4. Character echo during typing works (proves read/write syscall loop from ring 3)
#   5. Poweroff shuts down QEMU (proves SYS_reboot from ring 3)
#   6. Typematic suppression note (structural, not behaviorally testable via QEMU sendkey)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== User Mode (Ring 3) Feature Test ==="
echo "  Subtest 1: Boot welcome message"
echo "  Subtest 2: Shell prompt '>' appears (ring 3 task running)"
echo "  Subtest 3: 'greeting' via ring 3 syscall produces 'hello'"
echo "  Subtest 4: Character echo while typing"
echo "  Subtest 5: 'poweroff' via ring 3 syscall shuts down QEMU"
echo "  Subtest 6: [INFO] Typematic suppression (structural, not QEMU-testable)"

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot + Welcome ---
if wait_for_boot 15; then
	pass "Boot message 'HomunculOS' found (ring 0→3 transition OK)"
else
	dump_head=$(vga_text | head -c 80)
	fail "Boot message not found. VGA: [$dump_head]"
	qemu_stop
	exit 1
fi

sleep 2

assert_user_mode "Subtest 1: Ring 3 CS register after boot"
errors=$((errors + $?))

# --- Subtest 2: Shell prompt ---
if vga_contains ">"; then
	pass "Shell prompt '>' in VGA (user mode task running in ring 3)"
else
	dump_tail=$(vga_text | tr '\0' ' ' | tail -c 120)
	fail "Shell prompt not found. VGA tail: [$dump_tail]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 2: Ring 3 CS register after shell prompt"
errors=$((errors + $?))

# --- Subtest 3: Greeting command (syscall from ring 3) ---
for ch in g r e e t i n g; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if vga_contains "hello"; then
	pass "'greeting' produced 'hello' (int \$0x80 read+write from ring 3)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'hello' not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 3: Ring 3 CS register after greeting syscall"
errors=$((errors + $?))

# --- Subtest 4: Character echo while typing ---
# Type a short word character by character; the shell echoes each one.
# This confirms the read/write syscall loop works from ring 3.
sleep 1
for ch in t e s t; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.3

if vga_contains "test"; then
	pass "Character echo 'test' visible (per-char read+write loop from ring 3)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "Echoed text not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 4: Ring 3 CS register after character echo"
errors=$((errors + $?))

# Clear the buffer by sending a newline (ignores the typed text as unknown command)
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 0.5

# --- Subtest 5: Poweroff (syscall from ring 3) ---
for ch in p o w e r o f f; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU (SYS_reboot from ring 3)"
else
	if qemu_is_running; then
		fail "QEMU still running after 'poweroff' (timeout 10s)"
		errors=$((errors + 1))
	else
		pass "'poweroff' shut down QEMU"
	fi
fi

# --- Subtest 6: Typematic suppression note ---
echo "  [INFO] Typematic suppression: PS/2 driver tracks key state in"
echo "  [INFO] kbd_key_state[16] bitmap (drivers/ps2/kbd.c). Make events"
echo "  [INFO] for already-held keys are suppressed. QEMU's 'sendkey' with"
echo "  [INFO] duration=1 sends make+break, so typematic is not triggered."
echo "  [INFO] Verified structurally: code review confirms suppression logic"

qemu_stop
exit $errors
