#pragma once

#ifndef __ASSEMBLER__

#ifdef SDK_ARM9
#include <nitro.h>
#else  //SDK_ARM9
#include <nds.h>
#endif //SDK_ARM9

#endif //__ASSEMBLER__


#define R_ARM_ABS32			2
#define R_ARM_CALL			28
#define R_ARM_JUMP24		29
#define R_ARM_BASE_ABS		31

#define R_TYPE_MASK         0x7F
#define R_ZERO_VALUE        0x80

#define HOOK_TYPE_AREPL     0		//ARM branch-and-link replacement
#define HOOK_TYPE_ANSUB     1		//ARM branch replacement
#define HOOK_TYPE_TREPL     2		//thumb branch-link-exchange replacement
#define HOOK_TYPE_DATA32    3		//word replacement
#define HOOK_TYPE_DATA16    4		//halfword replacement
#define HOOK_TYPE_LOAD		5		//called on load
#define HOOK_TYPE_UNLOAD    6		//called on unload
#define HOOK_TYPE_DATA8		7		//single byte replacement

#define HOOK_SHIFT          28
#define HOOK_MASK           0x70000000
#define ADDRESS_SHIFT       0
#define ADDRESS_MASK        0x0FFFFFFF
#define AUX_SHIFT           31
#define AUX_MASK            0x80000000

#ifndef __ASSEMBLER__

typedef struct HOOK_TABLE_ENTRY_ {
	u32 branchSrcAddr : 28;			//memory location of patch source
	u32 type : 3;					//type of patch
	u32 aux : 1;					//is followed by auxiliary entry
	union {
		struct {
			u16 oldDataHw0;			//first halfword of old data
			u16 oldDataHw1;			//second halfword of old data
		};
		u8 oldData[4];
		struct {
			u16 newDataHw0;			//first halfword of new data
			u16 newDataHw1;			//second halfword of new data
		};
		u8 newData[4];
		u32 branchDestAddr;			//destination address of branch (relative to image base)
	};
} HOOK_TABLE_ENTRY;

typedef struct RELOCATION_ENTRY_ {
	u32 offset : 24;	//offset of relocation
	u32 type : 8;		//type of relocation
	u32 value;			//value of relocation (optional)
} RELOCATION_ENTRY;

typedef struct RCM_HEADER_ {
	u16 nHook;			//number of hooks
	u16 relocated;		//set to 1 after relocations process
	u16 relocOffset;	//offset to relocation table
	u16 relocSize;		//size of relocations
	HOOK_TABLE_ENTRY hookTable[0];
} RCM_HEADER;

//
//callback typedefs
//
typedef void (*RCM_LOAD_CALLBACK) (u32 imageBase);
typedef void (*RCM_UNLOAD_CALLBACK) (u32 imageBase);


#define HOOK_DATA8(addr,b)			((void *) ((HOOK_TYPE_DATA8		<< HOOK_SHIFT) + (u32) (addr))), (void *) b
#define HOOK_DATA16(addr,hw)		((void *) ((HOOK_TYPE_DATA16	<< HOOK_SHIFT) + (u32) (addr))), (void *) hw
#define HOOK_DATA32(addr,w)			((void *) ((HOOK_TYPE_DATA32	<< HOOK_SHIFT) + (u32) (addr))), (void *) w
#define HOOK_LOAD(dest)				((void *) ((HOOK_TYPE_LOAD		<< HOOK_SHIFT))), (void *) dest
#define HOOK_UNLOAD(dest)			((void *) ((HOOK_TYPE_UNLOAD	<< HOOK_SHIFT))), (void *) dest
#define HOOK_AREPL(addr,dest)		((void *) ((HOOK_TYPE_AREPL		<< HOOK_SHIFT) + (u32) (addr))), (void *) dest
#define HOOK_ANSUB(addr,dest)		((void *) ((HOOK_TYPE_ANSUB		<< HOOK_SHIFT) + (u32) (addr))), (void *) dest
#define HOOK_TREPL(addr,dest)		((void *) ((HOOK_TYPE_TREPL		<< HOOK_SHIFT) + (u32) (addr))), (void *) dest
#define HOOK_TNSUB(addr,dest)		HOOK_DATA16(addr, 0xB500), HOOK_TREPL(((u32) (addr)) + 2, dest), HOOK_DATA16(((u32) (addr)) + 6, 0xBD00)

#else //__ASSEMBLER__

#define HOOK_DATA8(addr,b)           ((HOOK_TYPE_DATA8  << HOOK_SHIFT) + addr), b
#define HOOK_DATA16(addr,hw)         ((HOOK_TYPE_DATA16 << HOOK_SHIFT) + addr), hw
#define HOOK_DATA32(addr,w)          ((HOOK_TYPE_DATA32 << HOOK_SHIFT) + addr), w
#define HOOK_LOAD(dest)              ((HOOK_TYPE_LOAD   << HOOK_SHIFT)), dest
#define HOOK_UNLOAD(dest)            ((HOOK_TYPE_UNLOAD << HOOK_SHIFT)), dest
#define HOOK_AREPL(addr,dest)        ((HOOK_TYPE_AREPL  << HOOK_SHIFT) + addr), dest
#define HOOK_ANSUB(addr,dest)        ((HOOK_TYPE_ANSUB  << HOOK_SHIFT) + addr), dest
#define HOOK_TREPL(addr,dest)        ((HOOK_TYPE_TREPL  << HOOK_SHIFT) + addr), dest
#define HOOK_TNSUB(addr,dest)        HOOK_DATA16(addr, 0xB500), HOOK_TREPL((addr) + 2, dest), HOOK_DATA16((addr) + 6, 0xBD00)

#endif //__ASSEMBLER__


//NCP-like synonyms
#define HOOK_JUMP_AA				HOOK_ANSUB
#define HOOK_JUMP_TA				HOOK_TNSUB
#define HOOK_JUMP					HOOK_ANSUB
#define HOOK_REPL_AA				HOOK_AREPL
#define HOOK_REPL_TA				HOOK_TREPL
#define HOOK_REPL					HOOK_AREPL
#define HOOK_SET8					HOOK_DATA8
#define HOOK_SET16					HOOK_DATA16
#define HOOK_SET32					HOOK_DATA32


#ifndef __ASSEMBLER__

#define HOOK_TABLE(...)  static volatile void *__attribute__((section(".ftbl"))) __attribute__((__used__)) __ftbl[] = {__VA_ARGS__}

#else  //__ASSEMBLER__

#define HOOK_TABLE(...)  .section .ftbl ; .word __VA_ARGS__ ; .section .text

#endif //__ASSEMBLER__
