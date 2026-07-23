/*
 * Linker script for the Ailunce HD2 (HR_C7000 / CK803S).
 * Compiles Miosix as a unikernel: all RAM for the kernel + kernelspace app,
 * no process pool.
 *
 * This is the board's default/reference script. An application with special
 * memory needs — e.g. reserving a fixed RAM window for a resident, non-
 * position-independent code blob — supplies its own linker script out of tree
 * (the kernel CMake accepts an absolute-path MIOSIX_LINKER_SCRIPT) rather than
 * baking that app-specific layout in here.
 *
 * Memory map:
 *   flash  the application slot the vendor IAP bootloader loads and jumps to.
 *          VMA == LMA (we run straight from the slot, no remap). Base
 *          0x0300d000, length 614400 (600*1024), matching the IAP flash slot.
 *   ram    on-chip SRAM. The low region 0x10000..0x20000 is left to the
 *          IAP/BOOTROM (vector table + early-boot use); the kernel takes
 *          0x20000..0x4f000 (188 KiB). The top 4 KiB (0x4f000..0x50000) is
 *          excluded because the IAP scribbles on it on every boot
 *          (~0x4fe00..0x50000, measured across a real WDT crash cycle), and it
 *          is a convenient spot for a board reset-surviving scratch block.
 *
 *          NB: this RAM base is chosen just above the BOOTROM/IAP low region; it
 *          has not been boot-validated on silicon via this script directly (no
 *          in-tree target links it — application builds provide their own).
 *
 * The generic Miosix script (included below) defines all sections/symbols
 * (_etext, _data, _bss_start/_end, init arrays, ctors, _heap_end,
 * _irq_stack_top, and ENTRY(miosix::Reset_Handler), which the cpu interrupts.cpp
 * places first in flash via .isr_vector). The board only specifies MEMORY +
 * tunables.
 */

/* IRQ stack: CK803S has a single SP; interrupts nest on it (CTX_SAVE pushes
 * the 72-byte frame). This stack is used during early boot and would also back
 * any deep nesting before the first context switch -- keep it comfortable. */
_irq_stack_size = 1024;

MEMORY
{
    flash(rx) : ORIGIN = 0x0300d000, LENGTH = 614400
    ram(wx)   : ORIGIN = 0x00020000, LENGTH = 0x2F000   /* 188 KiB: 0x20000..0x4f000 */
}

INCLUDE ldscripts/miosix-flash-ram.ld
