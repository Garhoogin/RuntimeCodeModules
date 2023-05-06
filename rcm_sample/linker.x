OUTPUT_ARCH(arm)

SECTIONS {
	
	.text : {
		
		FILL (0x1234)
		
		__file_start = . ;
		
		/* file header */
		SHORT((__ftbl_end - __ftbl_start) >> 3)	/* number of hook table entries */
		SHORT(0)								/* relocated (0 for file) */
		SHORT(0)								/* offset of relocations (unsupported, leave 0) */
		SHORT(0)								/* size of relocations (unsupported, leave 0) */
		__ftbl_start = . ;
		*(.ftbl)
		__ftbl_end = . ;
		
		*(.init)
		__text_start = . ;
		*(.text)
		*(.data)
		*(.ctors)
		*(.dtors)
		*(.rodata)
		*(.rodata*)
		*(.fini)
		*(COMMON)
		
		__text_end  = . ;
		
		__bss_start__ = . ;
		*(.bss)
		__bss_end__ = . ;
		
		_end = __bss_end__ ;
		__end__ = __bss_end__ ;
	}
}
