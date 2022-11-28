/*---------------------------------------------------------------------------*\

Revision history:

	11/28/2022 Garhoogin:
		Add data8 hook type

	08/22/2022 Garhoogin:
		Add hooks to load for different scenes

	08/19/2022 Garhoogin:
		Add cache invalidation for RCM unloading
	
	06/12/2022 Garhoogin:
		Fix bug where relocations would sometimes be applied twice
	
	05/08/2022 Garhoogin:
		Implement relocations and remove dispatch hook type
	
	04/25/2022 Garhoogin:
		Initial commit

\*---------------------------------------------------------------------------*/

#include <nds.h>

#include "hook.h"

extern void *arc_get2dArcFile(const char *path);
extern void *arc_getCourseArcFile(const char *path);
extern void *arc_getKartModelArcFile(const char *path);
extern void *arc_getMainRaceMainEffectArcFile(const char *path);
extern void *arc_getSceneArcFile(const char *path);

#define SCENE_RACE 2

extern int scproc_getCurrentScene(void);


//
// Calls a module's load callbacks after loading
//
void LDR_CallLoadCallback(void *pMod) {
	RCM_HEADER *hdr = (RCM_HEADER *) pMod;
	HOOK_TABLE_ENTRY *hookTable = hdr->hookTable;
	u32 base = (u32) pMod;
	int i;
	
	for(i = 0; i < hdr->nHook; i++){
		if(hookTable[i].aux || hookTable[i].type != HOOK_TYPE_LOAD) continue;
		RCM_LOAD_CALLBACK callback = (RCM_LOAD_CALLBACK) (hookTable[i].branchDestAddr + base);
		callback(base);
	}
}

//
// Calls a module's unload callbacks after unloading
//
void LDR_CallUnloadCallback(void *pMod) {
	RCM_HEADER *hdr = (RCM_HEADER *) pMod;
	HOOK_TABLE_ENTRY *hookTable = hdr->hookTable;
	u32 base = (u32) pMod;
	int i;
	
	for(i = 0; i < hdr->nHook; i++){
		if(hookTable[i].aux || hookTable[i].type != HOOK_TYPE_UNLOAD) continue;
		RCM_UNLOAD_CALLBACK callback = (RCM_UNLOAD_CALLBACK) (hookTable[i].branchDestAddr + base);
		callback(base);
	}
}

//
// Process relocations in a module
//
void LDR_Relocate(void *pMod) {
	RCM_HEADER *hdr = (RCM_HEADER *) pMod;
	u32 base = (u32) pMod;
	if(hdr->relocated) return;
	
	u16 relocOffset = hdr->relocOffset;
	u16 nReloc = hdr->relocSize;
	u16 i;
	RELOCATION_ENTRY *relocs = (RELOCATION_ENTRY *) (base + relocOffset);
	for(i = 0; i < nReloc; i++) {
		RELOCATION_ENTRY *entry = relocs + i;
		u32 destAddr = base + entry->offset;
		u32 value = entry->value;
		int type = entry->type;
		
		switch(type) {
			case R_ARM_ABS32:
				*(u32 *) destAddr += value;
				break;
			case R_ARM_CALL:
			case R_ARM_JUMP24:
			{
				u32 instr = *(u32 *) destAddr;
				s32 offset = *(u32 *) destAddr;
				
				if((value & 1) == 0) { //not THUMB
					offset = (offset & 0x00FFFFFF) << 2;
					if(offset & 0x02000000) offset -= 0x04000000;
					offset = ((offset + value - destAddr) >> 2) & 0x00FFFFFF;
					*(u32 *) destAddr = (instr & 0xFF000000) | offset;
				} else { //to THUMB
					offset = ((offset & 0x00FFFFFF) << 2);
					if((instr & 0xF0000000) == 0xF0000000) offset |= (((offset >> 24) & 1) << 1); //only if the instruction is already in ARM->THUMB encoding
					if(offset & 0x02000000) offset -= 0x04000000;
					offset += (value & 0xFFFFFFFE) - destAddr;
					*(u32 *) destAddr = (instr & 0xFE000000) | ((offset >> 2) & 0x00FFFFFF) | (((offset >> 1) & 1) << 24) | 0xF0000000;
				}
				break;
			}
			case R_ARM_BASE_ABS:
				*(u32 *) destAddr += value + base;
				break;
		}
	}
}

//
// Load module by its base address.
//
void LDR_LoadModule(void *pMod) {
	RCM_HEADER *hdr = (RCM_HEADER *) pMod;
	int nHook = hdr->nHook;
	int i;
	
	//process relocation table
	LDR_Relocate(pMod);
	
	//iterate through the hooks and insert patches
	HOOK_TABLE_ENTRY *hookTable = hdr->hookTable;
	for(i = 0; i < nHook; i++){
		if(hookTable[i].aux) continue;
		
		u32 hookType = hookTable[i].type;
		u32 hookDest = hookTable[i].branchDestAddr;
		u32 hookSrc = hookTable[i].branchSrcAddr;
		
		//don't do writes for load/unload callback types
		if(hookType == HOOK_TYPE_LOAD || hookType == HOOK_TYPE_UNLOAD) continue;
		
		//data8 is special in its alignment requirements
		if (hookType == HOOK_TYPE_DATA8) {
			//swap byte to table
			u8 newByte = hookTable[i].newData[0];
			u8 oldByte = *(u8 *) hookSrc;
			*(u8 *) hookSrc = newByte;
			hookTable[i].oldData[0] = oldByte;
		} else {
			//read old data (NOTE: read in 16-bit units, thumb need not be 32-bit aligned)
			u16 ohw0 = *(u16 *) (hookSrc + 0);
			u16 ohw1 = *(u16 *) (hookSrc + 2);
			
			//create patch. 
			s32 rel = hookDest - (hookSrc + 8);
			switch(hookType) {
				case HOOK_TYPE_AREPL:
					*(u32 *) hookSrc = 0xEB000000 | ((rel >> 2) & 0x00FFFFFF);	//BL instruction
					break;
				case HOOK_TYPE_ANSUB:
					*(u32 *) hookSrc = 0xEA000000 | ((rel >> 2) & 0x00FFFFFF);	//B instruction
					break;
				case HOOK_TYPE_TREPL:
					rel += 4; //accommodate thumb encoding
					*(u16 *) (hookSrc + 0) = 0xF000 | ((rel >> 12) & 0x7FF);
					*(u16 *) (hookSrc + 2) = 0xE800 | ((rel >> 1) & 0x7FF);
					break;
				case HOOK_TYPE_DATA32:	//full word replacement
					*(u16 *) (hookSrc + 0) = hookTable[i].newDataHw0;
					*(u16 *) (hookSrc + 2) = hookTable[i].newDataHw1;
					break;
				case HOOK_TYPE_DATA16:	//halfword replacement, only first halfword used
					*(u16 *) hookSrc = hookTable[i].newDataHw0;
					break;
			}
			
			//write old data to table
			hookTable[i].oldDataHw0 = ohw0;
			hookTable[i].oldDataHw1 = ohw1;
		}
	}
	
	//flush DC, invalidate IC
	DC_FlushAll();
	IC_InvalidateAll();
	
	LDR_CallLoadCallback(pMod);
}

//
// Unload a module by its base address.
//
void LDR_UnloadModule(void *pMod) {
	RCM_HEADER *hdr = (RCM_HEADER *) pMod;
	int nHook = hdr->nHook;
	int i;
	
	HOOK_TABLE_ENTRY *hookTable = hdr->hookTable;
	for(i = nHook; i >= 0; i--) {
		if(hookTable[i].aux) continue;
		u16 *patchLocation = (u16 *) (u32) hookTable[i].branchSrcAddr;
		int type = hookTable[i].type;
		
		if(type != HOOK_TYPE_LOAD && type != HOOK_TYPE_UNLOAD) {
			//data8 has to be unaligned
			if (type == HOOK_TYPE_DATA8) {
				//swap byte to table
				u8 newByte = hookTable[i].newData[0];
				u8 oldByte = *(u8 *) patchLocation;
				*(u8 *) patchLocation = newByte;
				hookTable[i].oldData[0] = oldByte;
			} else {
				//write back in units of 16
				patchLocation[0] = hookTable[i].oldDataHw0;
				if(type != HOOK_TYPE_DATA16) patchLocation[1] = hookTable[i].oldDataHw1;
			}
		}
	}
	
	//flush DC, invalidate IC
	DC_FlushAll();
	IC_InvalidateAll();
	
	LDR_CallUnloadCallback(pMod);
}

//
// Try to load a Runtime Code Module for a given archive lookup function.
//
void TryLoadRCM(void *(*getFilePtrFn) (const char *)) {
	void *fptr = getFilePtrFn("code.rcm");
	if (fptr != NULL) {
		//found code module :0
		LDR_LoadModule(fptr);
	}
}

//
// Try to unload a Runtime Code Module for a given archive lookup function.
//
void TryUnloadRCM(void *(*getFilePtrFn) (const char *)) {
	void *fptr = getFilePtrFn("code.rcm");
	
	if (fptr != NULL) {
		//found code module :0
		LDR_LoadModule(fptr);
	}
}

/*
TODO:

Load
	-GeneralMenu		DONE (NEEDS TESTING)
	-MainRace			DONE (NEEDS TESTING)
	-Logo				DONE (NEEDS TESTING)
	-Menu				DONE (NEEDS TESTING)
	-Option				DONE (NEEDS TESTING)
	-Record				DONE (NEEDS TESTING)
	-Title				DONE (NEEDS TESTING)
	-Result				DONE (NEEDS TESTING)
	-WiFiMenu			DONE (NEEDS TESTING)
	-WLMenu				DONE (NEEDS TESTING)
*/

static inline int IsSceneGeneralMenu(void) {
	int scene = scproc_getCurrentScene();
	return scene != SCENE_RACE;
}

void TryLoadSceneRCM(void) {
	//if the current scene is a menu scene, load the general menu RCM
	if (IsSceneGeneralMenu()) {
		TryLoadRCM(arc_get2dArcFile);
	}
	
	//load scene-specific RCM
	TryLoadRCM(arc_getSceneArcFile);
}

void TryUnloadSceneRCM(void) {
	//if the current scene is a menu scene, unload the general menu RCM
	if (IsSceneGeneralMenu()) {
		TryUnloadRCM(arc_get2dArcFile);
	}

	//unload scene-specific RCM
	TryUnloadRCM(arc_getSceneArcFile);
}

void TryLoadRaceRCM() {
	TryLoadSceneRCM();
	TryLoadRCM(arc_getCourseArcFile);
}

void TryUnloadRaceRCM() {
	TryUnloadRCM(arc_getCourseArcFile);
	TryUnloadSceneRCM();
}
