#!/bin/bash
# Common helpers for Illuminatrix integration tests
# All functions assume CWD is tests/

export TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_BIN="${TESTS_DIR}/../kernel.bin"
export MONITOR_SOCK="${TESTS_DIR}/qemu-monitor.sock"
export QEMU_PID=""
export DISK_IMG="${TESTS_DIR}/ext2-disk.img"

PASS="PASS"
FAIL="FAIL"

cleanup() {
	if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
		kill "$QEMU_PID" 2>/dev/null
		wait "$QEMU_PID" 2>/dev/null
	fi
	rm -f "$MONITOR_SOCK"
}
trap cleanup EXIT
trap cleanup INT TERM

check_deps() {
	local missing=0
	for cmd in "$@"; do
		if ! command -v "$cmd" &>/dev/null; then
			echo "ERROR: $cmd not found" >&2
			missing=1
		fi
	done
	return $missing
}

prepare_basic_disk() {
	local shell_bin="${TESTS_DIR}/../shell/shell"
	local init_bin="${TESTS_DIR}/../init/init"
	if [ ! -f "$shell_bin" ]; then
		echo "ERROR: shell binary not found at $shell_bin" >&2
		return 1
	fi
	if [ ! -f "$init_bin" ]; then
		echo "ERROR: init binary not found at $init_bin" >&2
		return 1
	fi
	rm -f "$DISK_IMG"
	dd if=/dev/zero of=.part.img bs=1M count=31 2>/dev/null
	mkfs.ext2 -F -E revision=0 -b 1024 .part.img 2>/dev/null
	debugfs -w .part.img -R "rmdir /lost+found" 2>/dev/null
	debugfs -w .part.img -R "mkdir /dev" 2>/dev/null
	debugfs -w .part.img -R "mkdir /bin" 2>/dev/null
	debugfs -w .part.img -R "write $shell_bin /bin/shell" 2>/dev/null
	debugfs -w .part.img -R "write $init_bin /bin/init" 2>/dev/null
	for cmd in poweroff greeting uname ls cat stat hello pwd cd; do
		local src="${TESTS_DIR}/../shell/bin/$cmd"
		if [ -f "$src" ]; then
			debugfs -w .part.img -R "write $src /bin/$cmd" 2>/dev/null
		fi
	done
	dd if=/dev/zero of="$DISK_IMG" bs=1M count=32 2>/dev/null
	printf '2048,,L,*\n' | sfdisk "$DISK_IMG" 2>/dev/null
	dd if=.part.img of="$DISK_IMG" bs=512 seek=2048 conv=notrunc 2>/dev/null
	rm -f .part.img
	return 0
}

qemu_start() {
	qemu-system-i386 -kernel "$KERNEL_BIN" \
		-display none \
		-monitor "unix:${MONITOR_SOCK},server,nowait" \
		-no-reboot \
		-append "root=/dev/hda1 init=/bin/init" \
		&
	QEMU_PID=$!
	local waited=0
	while [ ! -S "$MONITOR_SOCK" ] && [ $waited -lt 10 ]; do
		sleep 0.5
		waited=$((waited + 1))
	done
	if [ ! -S "$MONITOR_SOCK" ]; then
		echo "ERROR: QEMU monitor socket not created after 5s" >&2
		return 1
	fi
}

qemu_start_with_disk() {
	qemu-system-i386 -kernel "$KERNEL_BIN" \
		-drive "file=${DISK_IMG},format=raw,if=ide" \
		-display none \
		-monitor "unix:${MONITOR_SOCK},server,nowait" \
		-no-reboot \
		-append "root=/dev/hda1 init=/bin/init" \
		&
	QEMU_PID=$!
	local waited=0
	while [ ! -S "$MONITOR_SOCK" ] && [ $waited -lt 10 ]; do
		sleep 0.5
		waited=$((waited + 1))
	done
	if [ ! -S "$MONITOR_SOCK" ]; then
		echo "ERROR: QEMU monitor socket not created after 5s" >&2
		return 1
	fi
}

qemu_stop() {
	if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
		kill "$QEMU_PID" 2>/dev/null
		wait "$QEMU_PID" 2>/dev/null
	fi
}

qemu_is_running() {
	[ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null
}

qemu_wait_exit() {
	local timeout="${1:-5}"
	local waited=0
	while qemu_is_running && [ $waited -lt $timeout ]; do
		sleep 1
		waited=$((waited + 1))
	done
	! qemu_is_running
}

wait_for_boot() {
	local timeout="${1:-10}"
	local waited=0
	while [ $waited -lt $timeout ]; do
		if vga_contains "Illuminatrix" 2>/dev/null; then
			return 0
		fi
		sleep 1
		waited=$((waited + 1))
	done
	return 1
}

monitor_cmd() {
	echo "$1" | socat - "UNIX-CONNECT:${MONITOR_SOCK}" 2>/dev/null
}

vga_dump() {
	monitor_cmd "xp /4000bx 0xB8000"
}

# Parse QEMU "xp" hex dump (from stdin) into ASCII text.
# Strips address prefixes, parses hex bytes, outputs character bytes (even indices).
vga_decode() {
	local idx=0
	while IFS= read -r line; do
		line="${line#"${line%%[![:space:]]*}"}"
		line="${line%"${line##*[![:space:]]}"}"
		[ -z "$line" ] && continue
		[[ "$line" == "(qemu)"* ]] && continue
		# Remove address prefix (e.g., "B8000:" or "00000000000B8000:")
		if [[ "$line" == *":"* ]]; then
			line="${line#*: }"
		fi
		for hex in $line; do
			hex="${hex#0x}"
			[ ${#hex} -eq 2 ] || continue
			byte=$((16#$hex))
			if [ $((idx % 2)) -eq 0 ]; then
				if [ $byte -ge 32 ] && [ $byte -le 126 ]; then
					printf "\\$(printf '%03o' $byte)"
				elif [ $byte -eq 0 ]; then
					printf " "
				fi
			fi
			idx=$((idx + 1))
		done
	done
	echo
}

vga_text() {
	vga_dump | vga_decode
}

vga_contains() {
	local pattern="$1"
	vga_text | grep -F -q "$pattern"
}

send_keys() {
	local keys="$1"
	local delay="${2:-0.05}"
	for ((i=0; i<${#keys}; i++)); do
		local ch="${keys:$i:1}"
		local key_name
		case "$ch" in
			$'\n') key_name="ret" ;;
			' ')   key_name="spc" ;;
			-)     key_name="minus" ;;
			_)     key_name="shift-minus" ;;
			=)     key_name="equal" ;;
			+)     key_name="shift-equal" ;;
			/)     key_name="slash" ;;
			.)     key_name="dot" ;;
			,)     key_name="comma" ;;
			!)     key_name="shift-1" ;;
			"?")   key_name="shift-slash" ;;
			[a-z]) key_name="$ch" ;;
			[A-Z]) key_name="shift-$(echo "$ch" | tr '[:upper:]' '[:lower:]')" ;;
			[0-9]) key_name="$ch" ;;
			*)     key_name="$ch" ;;
		esac
		monitor_cmd "sendkey $key_name" >/dev/null 2>&1
		sleep "$delay"
	done
}

# Check that the current CPU mode is ring 3 (user mode) via CS register
# CS should be 0x001b (GDT_USER_CODE index 3, RPL=3)
# Returns 0 if user mode, 1 if not
check_user_mode() {
	local regs
	regs=$(monitor_cmd "info registers" 2>/dev/null)
	local cs_val
	cs_val=$(echo "$regs" | grep -E '^CS\s*=' | sed 's/.*=[[:space:]]*\([0-9a-fA-F]\{4\}\).*/\1/' | tr '[:upper:]' '[:lower:]')
	[ "$cs_val" = "001b" ]
}

assert_user_mode() {
	local msg="${1:-User mode (ring 3) verification}"
	local retries="${2:-5}"
	local i
	for ((i = 0; i < retries; i++)); do
		if check_user_mode; then
			pass "$msg"
			return 0
		fi
		sleep 0.2
	done
	local cs_val
	cs_val=$(monitor_cmd "info registers" 2>/dev/null | grep -E '^CS\s*=' | sed 's/.*=[[:space:]]*\([0-9a-fA-F]\{4\}\).*/\1/')
	fail "$msg - CS=0x${cs_val}, expected 0x001b (ring 3)"
	return 1
}

pass() {
	printf "  \033[32m$PASS\033[0m $1\n"
}

fail() {
	printf "  \033[31m$FAIL\033[0m $1\n"
}
