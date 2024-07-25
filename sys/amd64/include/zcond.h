#ifndef _MACHINE_ZCOND_H
#define _MACHINE_ZCOND_H

// #include <sys/zcond.h>
#include <sys/types.h>

/*
 * __zcond_table is an ELF section which keeps
 * all the data related to the zcond mechanism.
 * A single entry describes a single ins_point.
*/
#define ZCOND_TABLE_ENTRY                         \
	".pushsection __zcond_table, \"aw\" \n\t" \
	".quad 1b \n\t"                           \
	".quad %l[l_true] \n\t"                   \
	".quad %c0 \n\t"                          \
	".quad 0 \n\t"                            \
	".quad 0 \n\t"                            \
	".popsection \n\t"

/* from Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2B
 * 4-165 */
static char nop_short_bytes[] = { 0x66, 0x90 };
static char nop_long_bytes[] = { 0x0f, 0x1f, 0x44, 0x00, 0x00 };

#define ZCOND_NOP_SHORT_ASM \
	".byte 0x66\n\t"    \
	".byte 0x90\n\t"

#define ZCOND_NOP_LONG_ASM \
	".byte 0x0f \n\t"  \
	".byte 0x1f \n\t"  \
	".byte 0x44 \n\t"  \
	".byte 0x00 \n\t"  \
	".byte 0x00 \n\t"

#define ZCOND_JMP_SHORT_OPCODE 0xeb
#define ZCOND_JMP_LONG_OPCODE  0xe9

#define ZCOND_INSN_SHORT_SIZE  2
#define ZCOND_INSN_LONG_SIZE   5
#define ZCOND_MAX_INSN_SIZE    5

struct zcond;
struct ins_point;

/*
 * Emits a __zcond_table entry, describing one ins_point.
 * Bakes in a nop instruction instruction, so the return value is initially false. 
*/
static __attribute__((always_inline)) bool
zcond_nop(struct zcond *const zcond_p)
{
	asm goto("1: " ZCOND_NOP_LONG_ASM ZCOND_TABLE_ENTRY
		 :
		 : "i"(zcond_p)
		 :
		 : l_true);

	return (false);
l_true:
	return (true);
}

/*
 * Emits a __zcond_table entry, describing one ins_point.
 * Bakes in a jmp instruction instruction, so the return value is initially true. 
*/
static __attribute__((always_inline)) bool
zcond_jmp(struct zcond *const zcond_p)
{
	asm goto("1: jmp %[l_true] \n\t" ZCOND_TABLE_ENTRY
		 :
		 : "i"(zcond_p)
		 :
		 : l_true);
	return (false);
l_true:
	return (true);
}

/*
 * Called before a single ins_point is patched.
*/
void zcond_before_patch(void);

/*
 * Called after a single ins_point was patched.
*/
void zcond_after_patch(void);

/*
 * Called before CPUs are parked. Use this hook to perform MD pmap loading
 * and other MD setup.
*/
void zcond_before_rendezvous(void);

/*
 * Called after the whole zcond is patched and CPUs are resumed.
 *  Use this hook to perform MD pmap cleanup.
*/
void zcond_after_rendezvous(void);

/*
 * Calculates the bytes of instruction with which the ins_p inspection point is to be patched with.
 * insn[] is populated with the instruction bytes and size is set to the number of instruction bytes.
*/
void zcond_get_patch_insn(struct ins_point *ins_p, unsigned char insn[],
    size_t *size);

#endif /*!_MACHINE_ZCOND_H*/
