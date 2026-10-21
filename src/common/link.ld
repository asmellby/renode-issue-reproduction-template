MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 64K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 60K
}

ENTRY(reset_handler)

SECTIONS
{
    .text : {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
    } > FLASH

    .bss (NOLOAD) : ALIGN(32) {
        *(.bss* COMMON)
        *(.data*)
    } > RAM

    /* Thread stack, 32-byte aligned for MPU regions, followed by the handler stack */
    .stacks (NOLOAD) : ALIGN(0x400) {
        __thread_stack_bottom = .;
        . += 0x800;
        __thread_stack_top = .;
        . += 0x800;
        __main_stack_top = .;
    } > RAM
}
