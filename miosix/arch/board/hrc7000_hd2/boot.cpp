/***************************************************************************
 *   Boot-time memory and clock initialisation for the Ailunce HD2          *
 *   (HR_C7000 / CK803S).  GPL v2+ with the Miosix linking exception.       *
 *                                                                          *
 *   IRQmemoryAndClockInit() is called from the cpu Reset_Handler before    *
 *   .data/.bss are set up, so it must not touch initialised globals.  It   *
 *   disables the IAP watchdog and brings the APLL/BPLL up, switching the   *
 *   baseband + SoC clocks onto them (the rest of the board init lives in   *
 *   bsp.cpp / IRQbspInit).                                                 *
 ***************************************************************************/

namespace miosix {

//-----------------------------------------------------------------------------
// HD2 SOCSYS clock/PLL registers (0x11000000) — the subset clk_init_pll needs.
// Named registers (no magic numbers). Mirrors the HW-verified vendor
// clock/PLL bring-up sequence.
//-----------------------------------------------------------------------------
#define HRC7000_SOCSYS_BASE   0x11000000u
#define HRC7000_SOCSYS(off)   (*(volatile unsigned int*)(HRC7000_SOCSYS_BASE+(off)))
#define SOCSYS_CLK_CTRL   HRC7000_SOCSYS(0x04u)   // [31]pll_ld(RO) [30]clk_rdy(RO) [3]bclk_sel [2]aclk_sel [1:0]re_cfg
#define SOCSYS_APLL       HRC7000_SOCSYS(0x08u)
#define SOCSYS_REG10      HRC7000_SOCSYS(0x10u)   // BPLL
#define SOCSYS_CLKDIV_18  HRC7000_SOCSYS(0x18u)
#define SOCSYS_CLKDIV_1C  HRC7000_SOCSYS(0x1cu)
#define SOCSYS_CLKDIV_20  HRC7000_SOCSYS(0x20u)
#define SOCSYS_REG24      HRC7000_SOCSYS(0x24u)
#define SOCSYS_REG28      HRC7000_SOCSYS(0x28u)
#define SOCSYS_REG2C      HRC7000_SOCSYS(0x2cu)   // gated-clock enable
#define SOCSYS_REG30      HRC7000_SOCSYS(0x30u)

#define CLK_CTRL_PLL_LD        0x80000000u
#define CLK_CTRL_CLK_RDY       0x40000000u
#define CLK_CTRL_ACLK_WORK_SEL 0x04u
#define CLK_CTRL_BCLK_WORK_SEL 0x08u
#define CLK_CTRL_RE_CFG        0x03u

#define SOCSYS_APLL_CFG        0x05040eb2u
#define SOCSYS_CLKDIV_18_CFG   0x100a0c0cu
#define SOCSYS_CLKDIV_1C_CFG   0x2e002900u
#define SOCSYS_CLKDIV_20_CFG   0xa5771177u
#define SOCSYS_REG2C_CFG       0xfff0ff3cu

//-----------------------------------------------------------------------------

static void clkBusyDelay()
{
    //Vendor FUN_00030ab8(100); ~25k cycles is generous at the ck803s default.
    for(volatile unsigned int i=0;i<25000u;++i) { }
}

/// Wait for a CLK_CTRL flag asserted for >10 consecutive reads (manual ch.04
/// steps 8 & 10). Returns true if confirmed stable, false on timeout.
static bool clkWaitBitStable(unsigned int mask)
{
    unsigned int consecutive=0;
    for(unsigned int guard=0;guard<4000u;++guard)
    {
        if((SOCSYS_CLK_CTRL & mask)!=0u) { if(++consecutive>10u) return true; }
        else consecutive=0;
    }
    return false;
}

void IRQmemoryAndClockInit()
{
    //--- Disable the watchdog FIRST. The Dahua IAP enables the DW_apb_wdt
    //(0x14010000); a standalone firmware that never "knocks" it gets reset-
    //looped (continuous IAP re-run + UART junk, LEDs stuck early). Unlock
    //(WDG_LOCK=0x5ada7200) then WDG_EN=0 (manual §4.10, Register Table 8).
    *reinterpret_cast<volatile unsigned int*>(0x14010000u)=0x5ada7200u; //unlock
    *reinterpret_cast<volatile unsigned int*>(0x14010010u)=0u;          //WDG_EN=0

    //Port of vendor V2.1.3 FUN_00030b6c. Brings the
    //APLL/BPLL up and switches the baseband+SoC clocks onto them — without this
    //the DW timer/peripherals are at the IAP-default clock and the measured
    //42 MHz timebase (HRC7000_TIMER_HZ) does not hold. Pure register writes; safe
    //to run before .data/.bss init. No external RAM to bring up (all internal).
    SOCSYS_APLL     =SOCSYS_APLL_CFG;
    SOCSYS_CLKDIV_18=SOCSYS_CLKDIV_18_CFG;
    SOCSYS_CLKDIV_1C=SOCSYS_CLKDIV_1C_CFG;
    SOCSYS_CLKDIV_20=SOCSYS_CLKDIV_20_CFG;

    SOCSYS_REG30=2u;
    SOCSYS_REG10=(SOCSYS_REG10 & 0xff000000u) | 0x712u;
    SOCSYS_REG28=6u;
    SOCSYS_REG24=(SOCSYS_REG24 & 0xfffffff0u) | 4u;

    SOCSYS_CLK_CTRL|=CLK_CTRL_RE_CFG;          //engage both PLLs
    clkWaitBitStable(CLK_CTRL_PLL_LD);         //confirm PLL lock before switching

    SOCSYS_CLK_CTRL&=~CLK_CTRL_ACLK_WORK_SEL;  //baseband -> APLL
    clkBusyDelay();
    SOCSYS_CLK_CTRL&=~CLK_CTRL_BCLK_WORK_SEL;  //SoC -> BPLL
    clkWaitBitStable(CLK_CTRL_CLK_RDY);        //confirm clock-switch ready

    SOCSYS_REG2C=SOCSYS_REG2C_CFG;             //enable peripheral clock gates
}

} //namespace miosix
