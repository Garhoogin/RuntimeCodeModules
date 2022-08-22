# Runtime Code Modules

## Introduction
Runtime Code Modules (RCM files) are relocatable files designed to allow for dynamically loading code and patches from a file. The way that these patches are constructed is intended to be similar in concept to traditional code hacks patched by New Super Mario Bros. Editor, with the addition of new hook types to ease replacement of non-code data in memory.

The RCM loader is called in a few key places, those being on race startup just before map objects are initialized, as well as on various menus. The loader looks for a file named “code.rcm” in the root of the archive corresponding to the point where it is supposed to be loaded. If no code.rcm file is present, then none will be loaded and no side effects will be felt. If one is present, then the file is first relocated, then patches are applied. It is done in this order to allow more versatility in data replacement hook types without the need for specialized relative offset versions.

## Building RCM Files
The prerequisites for building Runtime Code Modules are devkitARM with libnds. To build an RCM file, navigate to its project directory in a command prompt, then run make. If all has proceeded correctly, there will now be a file named code.rcm in the project directory. Due to a current deficiency in GNU make, it does not allow for file paths with spaces in their name, so if the project directory or any of its parents have whitespace in their names, the project directory should be renamed or moved to one without any whitespaces in it. 

## Implementing an RCM File
To implement an RCM file into the game, simply take the built code.rcm file and place it in the root directory of one of the supported archives and the loader will automatically load and unload the file when appropriate. If not done already, add the loader code to the main (non-RCM) code for the project and compile it. With these two done, the file should now appropriately load and patch.

## Writing an RCM File
To write an RCM file, first set up an empty directory for the RCM. Copy the files from the RCM template into this new directory to get started. To start off, write the hack as though it were a typical code hack patched with New Super Mario Bros. Editor, only don’t bother with putting `ansub`, `arepl`, `tnsub` or `trepl` in the label names to create hooks, these will be ignored and make for uglier label names. Instead, the hooks are defined in a hook table. The hook table contains a list of hooks to apply to the game when the file is loaded, in much the same way as a traditional code hack is done, only at runtime. Make sure to have, before defining the hook table, an `#include "hook.h"`. After this, a hook table can be defined in the following way:

```C
HOOK_TABLE(
	//list of hooks...
);
```

Within the parentheses, the list of hooks is written separated by commas. For example, if a hook were created that patched address `02000300` with a branch to `MyFunction`, and a hook patching address `02000400` with a branch-and-link to `MyOtherFunction`, the hook table would look as follows:

```C
HOOK_TABLE(
	HOOK_ANSUB(0x02000300, MyFunction),
	HOOK_AREPL(0x02000400, MyOtherFunction)
);
```

The hook names derive from those offered in a typical code hack, only the place they’re written differs. This is not the only set of hooks permissible by the Runtime Code Modules, however. A full list follows:

|  Hook Macro   | Description |
| ------------- | ----------- |
| `HOOK_AREPL`  | Replace target with BL to hook destination |
| `HOOK_ANSUB`  | Replace target with B to hook destination |
| `HOOK_TREPL`  | Replace target with BLX to hook destination |
| `HOOK_TNSUB`  | Replace target with an 8-byte sequence to branch to the hook destination |
| `HOOK_DATA32` | Replace target with a 32-bit word |
| `HOOK_DATA16` | Replace target with a 16-bit halfword |
| `HOOK_LOAD`   | Run a function immediately after load time (target is ignored) |
| `HOOK_UNLOAD` | Run a function immediately after unload time (target is ignored) |

The data replacement hooks do also allow for relocations to be applied. Since relocations are applied before the hooks are, a data hook may point to a function or variable within the RCM file. 

## RCM File Format
The RCM file format has four main sections to it: the file header, hook table, body, and relocations. The header is 8 bytes large and takes the following structure:

```C
typedef struct RCM_HEADER_ {
    u16 nHook;          //number of hooks
    u16 relocated;      //files should set this to 0
    u16 relocOffset;    //offset to relocation table
    u16 relocSize;      //number of relocations
} RCM_HEADER;
```

Immediately following the header comes the hook table. Each hook entry takes 8 bytes and has the following structure:

```C
typedef struct HOOK_TABLE_ENTRY_ {
    u32 branchSrcAddr : 28;
    u32 type : 3;
    u32 aux : 1;
    union {
    	struct {
    		u16 oldDataHw0;
    		u16 oldDataHw1;
    	};
    	struct {
    		u16 newDataHw0;
    		u16 newDataHw1;
    	};
    	u32 branchDestAddr;
    };
} HOOK_TABLE_ENTRY;
```

The `branrchSrcAddr` field points to the location in memory where the patch is applied. The `type` field indicates which type of hook is to be applied there. The `aux` field is not currently used but is intended to function as a marker that a hook table entry is a continuation of a previous long hook table entry. The union that follows these fields initially stores either new data or a branch destination. It is split into u16s since patches are only required to be halfword aligned due to the halfword patch type. During patching however, the original data from before the patch was applied gets written into the hook table in place of the new data or branch destination. This allows for the loader, on unloading, to read data from the hook table and write it back to the destination address. For `tnsub` type hooks that take up 8 bytes instead of 4, they are, in reality, implemented as a series of 3 hooks: 2 being halfword replacements and 1 being a trepl type hook for compatibility with older style patching. This hook type is cumbersome and best avoided if possible.

Following the code body may be a generated thunk table. These thunks are generated primarily to allow for code that branches to thumb code to work properly. Since there is no encoding for a branch-and-exchange over long distances to immediate relative addresses on ARM, the thunk table allows this code to work correctly. The `B` instruction is replaced with a branch to an entry in the thunk table, which handles the transitioning to thumb mode. Currently this is done by using an instruction of the form `ldr r15, =dest`. Because of this, each entry takes up 8 bytes. 

After the thunks table in the file comes the relocation table. Each relocation entry takes 8 bytes. The structure of a relocation entry is as follows:

```C
typedef struct RELOCATION_ENTRY_ {
	u32 offset : 24;
	u32 type : 8;
	u32 value;
} RELOCATION_ENTRY;
```

The offset field holds the offset relative to the beginning of the file that the relocation should happen. The type of relocation controls how the relocation is applied. The currently supported relocation types are `R_ARM_ABS32`, `R_ARM_CALL`, `R_ARM_JUMP24`, and `R_ARM_BASE_ABS`. Unsupported relocation types are ignored.

