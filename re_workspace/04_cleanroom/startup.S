/* Demonstration-only MIPS32 startup: NOT a receiver reset sequence.
 * Assumes a bootloader has already set up DRAM and copied this raw image
 * to 0x80000000. No CPU mode, clocks, exceptions or peripherals are set. */
    .section .text.entry, "ax", @progbits
    .set noreorder
    .globl _start
    .ent _start
_start:
    la    $sp, __stack_top
    la    $gp, _gp
    la    $t0, __bss_start
    la    $t1, __bss_end
1:
    sltu  $t2, $t0, $t1
    beq   $t2, $zero, 2f
    nop
    sw    $zero, 0($t0)
    b     1b
    addiu $t0, $t0, 4
2:
    jal   device_main
    nop
3:
    b     3b
    nop
    .end _start
