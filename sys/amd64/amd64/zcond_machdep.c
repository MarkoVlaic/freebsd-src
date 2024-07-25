#include <sys/types.h>
#include <sys/zcond.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpufunc.h>
#include <machine/zcond.h>

// static bool wp;
static uint64_t cr3;
extern struct pmap zcond_patching_pmap;
void
zcond_before_patch(void)
{
	// wp = disable_wp();
	// cr3 = rcr3();
}

void
zcond_after_patch(void)
{
	// restore_wp(wp);
	// load_cr3(cr3);
}

void
zcond_before_rendezvous(void)
{
	cr3 = rcr3();
	load_cr3(zcond_patching_pmap.pm_cr3);
}

void
zcond_after_rendezvous(void)
{
	load_cr3(cr3);
}

static void
insn_nop(unsigned char insn[], size_t size)
{
	int i;
	if (size == ZCOND_INSN_SHORT_SIZE) {
		for (i = 0; i < ZCOND_INSN_SHORT_SIZE; i++) {
			insn[i] = nop_short_bytes[i];
		}
	} else {
		for (i = 0; i < ZCOND_INSN_LONG_SIZE; i++) {
			insn[i] = nop_long_bytes[i];
		}
	}
}

static void
insn_jmp(unsigned char insn[], size_t size, vm_offset_t offset)
{
	int i;
	if (size == 2) {
		insn[0] = ZCOND_JMP_SHORT_OPCODE;
		insn[1] = offset;
	} else {
		insn[0] = ZCOND_JMP_LONG_OPCODE;
		for (i = 0; i < ZCOND_INSN_LONG_SIZE - 1; i++) {
			insn[i + 1] = (offset >> (i * 8)) & 0xFF;
		}
	}
}

void
zcond_get_patch_insn(struct ins_point *p, unsigned char insn[], size_t *size)
{
	unsigned char *patch_addr = (unsigned char *)p->patch_addr;
	vm_offset_t offset;

	if (*patch_addr == nop_short_bytes[0]) {
		// two byte nop
		*size = ZCOND_INSN_SHORT_SIZE;
		goto nop;
	} else if (*patch_addr == nop_long_bytes[0]) {
		*size = ZCOND_INSN_LONG_SIZE;
		goto nop;
	} else if (*patch_addr == ZCOND_JMP_SHORT_OPCODE) {
		// two byte jump
		*size = ZCOND_INSN_SHORT_SIZE;
		goto jmp;
	} else if (*patch_addr == ZCOND_JMP_LONG_OPCODE) {
		// five byte jump
		*size = ZCOND_INSN_LONG_SIZE;
		goto jmp;
	} else {
		panic("unexpected opcode: %02hhx", *patch_addr);
	}

nop:
	// replace nop with jmp
	offset = p->lbl_true_addr - p->patch_addr - *size;
	insn_jmp(insn, *size, offset);
	return;

jmp:
	// replace jmp with nop
	insn_nop(insn, *size);
}
