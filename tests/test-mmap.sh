#!/bin/bash
# Test: Memory management functions (mm_map_at, mm_alloc_at) compile and page tables are sane
#
# These functions are defined in arch/i386/mm.c but not yet called during boot
# (they are infrastructure for the upcoming ELF32 loader). This test verifies:
#   1. Kernel boots (proves the new functions compiled and linked)
#   2. Page directory (via CR3) has valid identity-mapped PDEs for 0-16MB
#   3. PDEs above 16MB are not present
#   4. Page table entries map identity (virtual == physical) for the first 32KB
#   5. User mode (ring 3) still works (regression)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Memory Management (mm_map_at / mm_alloc_at) Test ==="

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot ---
if wait_for_boot 15; then
	pass "Boot message found (kernel compiled with mm_map_at / mm_alloc_at)"
else
	dump_head=$(vga_text | head -c 80)
	fail "Boot message not found. VGA: [$dump_head]"
	qemu_stop
	exit 1
fi

sleep 2

# --- Helper: extract hex dword values from xp output ---
# Filters out (qemu) prompt/banner, strips address prefixes, returns one 8-digit hex per line
xp_dwords() {
	local cmd="$1"
	monitor_cmd "$cmd" 2>/dev/null | tr -d '\r' | while IFS= read -r line; do
		# Skip empty lines and (qemu) prompt/banner
		[ -z "$line" ] && continue
		[[ "$line" == "(qemu)"* ]] && continue
		# Strip address prefix (e.g., "0010b000: " or "0x0010b000: ")
		line="${line#*: }"
		# Extract 8-digit hex values with 0x prefix
		for token in $line; do
			if [[ "$token" =~ ^0x[0-9a-fA-F]{8}$ ]]; then
				echo "${token#0x}"
			fi
		done
	done
}

# --- Subtest 2: Read CR3 and verify page directory PDEs ---
regs=$(monitor_cmd "info registers" 2>/dev/null)
cr3_hex=$(echo "$regs" | grep -oP 'CR3=\K[0-9a-fA-F]+' | head -1)

if [ -z "$cr3_hex" ] || [ "$cr3_hex" = "00000000" ]; then
	fail "Could not read CR3 or CR3 is zero"
	errors=$((errors + 1))
	cr3_valid=0
else
	cr3_valid=1
	pass "CR3 = 0x${cr3_hex} (page directory physical address)"

	# Dump first 8 PDEs (4 identity + 4 beyond)
	mapfile -t pde_vals < <(xp_dwords "xp /8wx 0x${cr3_hex}")

	if [ ${#pde_vals[@]} -lt 8 ]; then
		fail "Only got ${#pde_vals[@]} PDE values from xp, expected 8"
		errors=$((errors + 1))
	else
		# Check PDEs 0-3: Each must have Present bit set, valid page table addr
		for i in 0 1 2 3; do
			pde=$((16#${pde_vals[$i]}))
			if [ $((pde & 0x001)) -ne 0 ]; then
				pt_addr=$((pde & ~0xFFF))
				flags=$((pde & 0xFFF))
				if [ $pt_addr -ne 0 ]; then
					pass "PDE[$i] = 0x$(printf '%08X' $pde) -> PT=0x$(printf '%08X' $pt_addr) flags=0x$(printf '%03X' $flags)"
				else
					fail "PDE[$i] Present but PT address is zero: 0x$(printf '%08X' $pde)"
					errors=$((errors + 1))
				fi
			else
				fail "PDE[$i] not Present: 0x$(printf '%08X' $pde) (expected identity map 0-16MB)"
				errors=$((errors + 1))
			fi
		done

		# Check PDEs 4-7: Should NOT be present (no mapping above 16MB)
		for i in 4 5 6 7; do
			pde=$((16#${pde_vals[$i]}))
			if [ $((pde & 0x001)) -eq 0 ]; then
				pass "PDE[$i] = 0x$(printf '%08X' $pde) (not present, correct)"
			else
				fail "PDE[$i] = 0x$(printf '%08X' $pde) (Present, but expected no mapping above 16MB)"
				errors=$((errors + 1))
			fi
		done

		# --- Subtest 3: Verify page table entries (first PT, first 8 PTEs) ---
		pde0=$((16#${pde_vals[0]}))
		pt0_addr=$((pde0 & ~0xFFF))

		mapfile -t pte_vals < <(xp_dwords "xp /8wx ${pt0_addr}")

		if [ ${#pte_vals[@]} -lt 8 ]; then
			fail "Only got ${#pte_vals[@]} PTE values from xp, expected 8"
			errors=$((errors + 1))
		else
			for i in 0 1 2 3 4 5 6 7; do
				pte=$((16#${pte_vals[$i]}))
				expected_pa=$((i * 0x1000))
				pte_pa=$((pte & ~0xFFF))
				pte_flags=$((pte & 0xFFF))
				if [ $((pte & 0x001)) -ne 0 ]; then
					if [ "$pte_pa" -eq "$expected_pa" ]; then
						pass "PTE[0][$i] = 0x$(printf '%08X' $pte) -> 0x$(printf '%08X' $pte_pa) (identity, flags=0x$(printf '%03X' $pte_flags))"
					else
						fail "PTE[0][$i] -> 0x$(printf '%08X' $pte_pa), expected 0x$(printf '%08X' $expected_pa) (identity)"
						errors=$((errors + 1))
					fi
				else
					fail "PTE[0][$i] not present: 0x$(printf '%08X' $pte)"
					errors=$((errors + 1))
				fi
			done
		fi
	fi
fi

# --- Subtest 4: User mode regression ---
assert_user_mode "User mode (ring 3) after memory management test"
errors=$((errors + $?))

# --- Summary ---
sleep 1
if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED (see above)"
fi

qemu_stop
exit $errors
