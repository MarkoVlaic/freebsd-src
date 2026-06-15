#include <sys/systm.h>
#include <machine/cpu.h>
#include <machine/zcond.h>

#define INSN_SIZE 4

static const uint32_t nop_insn = 0xe320f000u;

static bool
patch_addr_valid(uintptr_t patch_addr, uintptr_t target)
{
	int32_t offset;

	if (patch_addr == target ||
	    (patch_addr & (INSN_SIZE - 1)) != 0 ||
	    (target & (INSN_SIZE - 1)) != 0 ||
	    patch_addr + 2 * INSN_SIZE < patch_addr)
		return (false);

	offset = target - (patch_addr + 2 * INSN_SIZE);
	if (offset < -(1 << 24) || offset > (1 << 24))
		return (false);
	return (true);
}

void
zcond_patchpoint_patch(uintptr_t patch_addr, uintptr_t target)
{
	uint32_t instr;

	KASSERT(patch_addr_valid(patch_addr, target),
	    ("%s: invalid tracepoint %#x -> %#x",
	    __func__, patch_addr, target));

	if(*((uint32_t*)patch_addr) == nop_insn) {
		// Replace nop with jump
		instr =
		    (((target - (patch_addr + 2 * INSN_SIZE)) >> 2) & ((1 << 24) - 1)) |
		        0xea000000;
	} else {
		// Replace jump with nop
		instr = nop_insn;
	}

	memcpy((void *)patch_addr, &instr, sizeof(instr));
	icache_sync(patch_addr, INSN_SIZE);
}
