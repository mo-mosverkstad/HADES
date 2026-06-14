OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY {
    DATA (rw) : ORIGIN = 0x0000, LENGTH = 4K
    CODE (rx) : ORIGIN = 0x1000, LENGTH = 12K
    STACK (rw) : ORIGIN = 0x4000, LENGTH = 4K
}

SECTIONS {
    .text : { *(.text*) } > CODE
    .data : { *(.data*) } > DATA
    .bss  : { *(.bss*)  } > DATA
}