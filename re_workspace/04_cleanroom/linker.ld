/* PROVISIONAL diagnostic memory layout, NOT a verified receiver linker map.
 * The 0xE00 gap is deliberately blank, NOT a valid exception vector table. */
ENTRY(_start)
PHDRS { image PT_LOAD FLAGS(7); }
SECTIONS
{
    . = 0x80000000;
    .prefix : {
        BYTE(0);
        . = . + 0xdff; /* 0xe00 bytes, so .text.entry begins at 0x80000e00 */
    } :image

    .text 0x80000e00 : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } :image
    . = ALIGN(16);
    .rodata : { *(.rodata .rodata.*) } :image
    . = ALIGN(16);
    .data : { *(.data .data.*) *(.sdata .sdata.*) } :image
    __image_end = .;

    .bss (NOLOAD) : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss .bss.*) *(COMMON) *(.sbss .sbss.*)
        . = ALIGN(16);
        __bss_end = .;
    } :image
    .stack (NOLOAD) : {
        . = ALIGN(16);
        . = . + 0x4000;
        __stack_top = .;
    } :image

    _gp = __image_end + 0x7ff0;
    /DISCARD/ : {
        *(.comment) *(.note*) *(.eh_frame*) *(.pdr)
        *(.MIPS.abiflags) *(.reginfo) *(.gnu.attributes) *(.mdebug*)
    }
    ASSERT(_start == 0x80000e00, "entry address must be 0x80000e00")
}
