#include <sys/param.h>
#include <sys/systm.h>
#include <sys/zcond.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/zcond.h>

#include <amd64/vmm/amd/svm.h>

void
zcond_before_patch(void)
{
}

void
zcond_after_patch(void)
{
}

void
zcond_before_rendezvous(void)
{
}

void
zcond_after_rendezsvouz(void)
{
}

static void
insn_nop(unsigned char insn[])
{
	int i;
	for (i = 0; i < MAX_INSN_SIZE; i++) {
		insn[i] = nop_bytes[i];
	}
}

static void
insn_jmp(unsigned char insn[], vm_offset_t offset)
{
	vm_offset_t imm26;
	uint32_t instr;
	int i;

	imm26 = offset >> 2;
	instr = (imm26 & 0x3fffffful) | 0x14000000;

	for (i = 0; i < MAX_INSN_SIZE; i++) {
		insn[i] = (instr >> (i * 8)) & 0xFF;
	}
}

void
zcond_get_patch_insn(struct ins_point *p, unsigned char insn[], size_t *size)
{
	unsigned char *patch_addr;
	vm_offset_t offset;

	patch_addr = (unsigned char *)p->patch_addr;
	*size = MAX_INSN_SIZE;
	printf("patch opcode: %02hhx at %p", *patch_addr, patch_addr);
	if (*patch_addr == nop_bytes[0]) {
		offset = p->lbl_true_addr - p->patch_addr;
		insn_jmp(insn, offset);
	} else if ((*(patch_addr + 3) & ~(0x3)) == 0x14) {
		insn_nop(insn);
	} else {
		panic("unexpected opcode: %02hhx", *patch_addr);
	}
}
