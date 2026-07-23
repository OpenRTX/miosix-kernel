/***************************************************************************
 *   CK803S (C-SKY V2) / HR_C7000 register definitions for modern Miosix.  *
 *   Named MMIO + control-register accessors (no magic numbers). Addresses  *
 *   verified on real HR_C7000 silicon. Register/bit names and reset defaults    *
 *   transcribed from the HR_C7000 user guide.                              *
 *   GPL v2+ with the Miosix linking exception.                            *
 ***************************************************************************/

#pragma once

#include <stdint.h>

/* ---- DW_apb_timers @ 0x14000000, channel stride 0x14 --------------------
 * ch0/Timer1 = free-running getTick/delay timebase. ch1/Timer2 = OS tick.   */
#define HRC7000_TIMER_BASE   0x14000000u
#define HRC7000_TMR(ch,off)  (*(volatile uint32_t*)(HRC7000_TIMER_BASE+(ch)*0x14u+(off)))
#define HRC7000_T1_LOAD      HRC7000_TMR(0u,0x00u)
#define HRC7000_T1_CURVAL    HRC7000_TMR(0u,0x04u)
#define HRC7000_T1_CTRL      HRC7000_TMR(0u,0x08u)
#define HRC7000_T2_LOAD      HRC7000_TMR(1u,0x00u)
#define HRC7000_T2_CURVAL    HRC7000_TMR(1u,0x04u)
#define HRC7000_T2_CTRL      HRC7000_TMR(1u,0x08u)   /* b0 en, b1 reload, b2 imask */
#define HRC7000_T2_EOI       HRC7000_TMR(1u,0x0cu)   /* READ clears Timer2 IRQ */
#define HRC7000_TIMER_HZ     42000000u           /* measured on silicon */

/* ---- PIC interrupt controller @ 0x17000000 ------------------------------ */
#define HRC7000_PIC_BASE     0x17000000u
#define HRC7000_PIC_MODE     (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x00u))
#define HRC7000_PIC_PO       (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x04u))
#define HRC7000_PIC_MASK     (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x08u)) /* bit=0 ENABLES */
#define HRC7000_PIC_COW1     (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x10u)) /* int-end: write */
#define HRC7000_PIC_COW1_EOI 0x4u                                        /* bit2 = eoi */
#define HRC7000_PIC_INT_ST   (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x44u))
#define HRC7000_PIC_INT_ST1  (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x48u))
#define HRC7000_PIC_MODE1    (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x60u))
#define HRC7000_PIC_PO1      (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x64u))
#define HRC7000_PIC_MASK1    (*(volatile uint32_t*)(HRC7000_PIC_BASE+0x68u)) /* srcs 32-63 */

/* PIC autovector base: VEC for source x = x + HRC7000_PIC_VECTOR0 (PIC_VECTOR=0x0c,
 * reset default 0x20). So PIC sources occupy VBR vectors 32..95. */
#define HRC7000_PIC_VECTOR0  32u
#define CK803S_VBR_NVEC     128u   /* CK803S vector space 0..127 (PIC valid 32..112) */

/* Timer2 = PIC source 2 -> autovector 32+2 = 34. trap0 -> CPU-exc vector 16. */
#define HRC7000_TIMER2_SRC   2u
#define HRC7000_TIMER2_VEC   34u
#define CK803S_YIELD_VEC    16u    /* VBR[16] = trap0 (scheduler-invoke) handler */

/* PSR bit positions (standard CK803S; verify on silicon). */
#define CK803S_PSR_IE_BIT   6u
#define CK803S_PSR_EE_BIT   8u

/* BOOTROM reset entry: jump to 0x4, not 0x03000000. */
#define HRC7000_BOOTROM_RESET 0x00000004u

static inline unsigned int csky_get_psr(void)
{ unsigned int v; asm volatile("mfcr %0, psr":"=r"(v)); return v; }
static inline void csky_set_psr(unsigned int v)
{ asm volatile("mtcr %0, psr"::"r"(v)); }
static inline void csky_set_vbr(const void *table)
{ asm volatile("mtcr %0, cr<1, 0>"::"r"(table)); }   /* VBR = cr<1,0> */
