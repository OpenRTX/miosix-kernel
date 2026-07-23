/***************************************************************************
 *   HR_C7000 (CK803S) OS timer for modern Miosix — custom (not TimerAdapter)*
 *   GPL v2+ with the Miosix linking exception.                            *
 *                                                                          *
 *   WHY CUSTOM (not the TimerAdapter CRTP): the on-chip DesignWare         *
 *   DW_apb_timers block has neither a match register nor a software-pend   *
 *   IRQ (TimerAdapter needs both: IRQsetTimerMatchReg + IRQforcePendingIrq,*
 *   the latter being armv4's VICSoftInt — the HR_C7000 PIC has no equivalent,   *
 *   see cpu_impl.h). So we implement the os_timer interface directly with  *
 *   a TWO-channel scheme, borrowing TimerAdapter's proven "pending-bit     *
 *   trick" for 64-bit timekeeping:                                         *
 *                                                                          *
 *     TIME_CH (ch0 = Timer1, PIC src1): free-running 32-bit DOWN-counter   *
 *       from 0xFFFFFFFF. Its virtual UP-counter value is (0xFFFFFFFF-CUR), *
 *       which behaves exactly like a HW up-counter (wraps 0xFFFFFFFF->0    *
 *       when CUR reloads 0->0xFFFFFFFF). Overflow IRQ bumps the upper 32    *
 *       bits; the pending-bit trick covers the read/IRQ race.              *
 *                                                                          *
 *     WAKE_CH (ch1 = Timer2, PIC src2): one-shot (emulated: disabled in    *
 *       its own ISR) used as the match. IRQosTimerSetInterrupt(absNs)      *
 *       loads it with the RELATIVE tick count to the deadline; if the       *
 *       deadline is already past it loads 1 so the IRQ fires ASAP — this   *
 *       replaces IRQforcePendingIrq.                                       *
 *                                                                          *
 *   Single-core UNIFIED timer model (OS_TIMER_MODEL_UNIFIED, no SMP): the  *
 *   scheduler uses IRQosTimerSetInterrupt() for BOTH wakeup and preemption,*
 *   so IRQosTimerSetPreemption() is not required.                          *
 *                                                                          *
 *   DW timer freq 42 MHz, measured on silicon. Register map + control      *
 *   bits HW-verified on hardware.                                          *
 ***************************************************************************/

#include "kernel/lock.h"
#include "kernel/timeconversion.h"
#include "interfaces/arch_registers.h"
#include "interfaces/interrupts.h"
#include "interfaces_private/os_timer.h"

namespace miosix {

//
// DW_apb_timers register access (per-channel stride 0x14; HRC7000_TMR from
// arch_registers_impl.h). Per-channel registers:
//   +0x00 LoadCount  +0x04 CurrentValue  +0x08 ControlReg
//   +0x0c EOI (READ clears the channel IRQ)  +0x10 IntStatus (masked, R/O)
//
static constexpr unsigned int TIME_CH=0u;   // Timer1 -> PIC source 1
static constexpr unsigned int WAKE_CH=1u;   // Timer2 -> PIC source 2
static constexpr unsigned int TIME_PIC_SRC=1u;
static constexpr unsigned int WAKE_PIC_SRC=2u;

#define TMR_LOAD(ch)    HRC7000_TMR((ch),0x00u)
#define TMR_CURVAL(ch)  HRC7000_TMR((ch),0x04u)
#define TMR_CTRL(ch)    HRC7000_TMR((ch),0x08u)
#define TMR_EOI(ch)     HRC7000_TMR((ch),0x0cu)   // read clears IRQ
#define TMR_INTST(ch)   HRC7000_TMR((ch),0x10u)   // non-clearing status

#define TMR_CTRL_ENABLE   (1u<<0)
#define TMR_CTRL_USERDEF  (1u<<1)   // 1 = user-defined (reload from LoadCount); 0 = free-run
#define TMR_CTRL_INTMASK  (1u<<2)   // 1 = IRQ masked

static constexpr unsigned long long upperIncr=(1ULL<<32);

//Software-extended upper bits of the 64-bit timekeeping counter, and the
//pending wakeup time in ns (numeric_limits<long long>::max() == "none set").
static long long upperTimeTick=0;
static long long irqNs=0x7FFFFFFFFFFFFFFFLL;

//Fixed-point tick<->ns conversion (Miosix TimeConversion, 32.32 ~0.03 ppm), set
//up in IRQosTimerInit from the timer frequency. Verified on CK803S silicon:
//42e6 tk -> 1000000030 ns, round-trip -> 41999999 tk (verified on silicon).
//(An earlier integer 500/21 fallback — from a misdiagnosed "TimeConversion hangs"
//— was ~3% coarse; TimeConversion runs fine here, soft-float and all.) ns2tick()
//is non-reentrant, but every caller below holds the global lock or runs in IRQ.
static TimeConversion tc;
static inline long long ticksToNs(long long t)  { return tc.tick2ns(t); }
static inline long long nsToTicks(long long ns) { return tc.ns2tick(ns); }

//----- timekeeping (pending-bit trick on the free-running down-counter) -------

/// Virtual up-counter = 0xFFFFFFFF - CurrentValue (down-counter).
static inline unsigned int IRQtimeCounter()
{
    return 0xFFFFFFFFu-TMR_CURVAL(TIME_CH);
}
/// Overflow pending, read WITHOUT clearing (per-channel IntStatus +0x10).
static inline bool IRQtimeOverflowFlag()
{
    return (TMR_INTST(TIME_CH) & 1u)!=0u;
}

static inline long long IRQgetTimeTick()
{
    //TimerAdapter's pending-bit trick: extend the 32-bit HW counter to 64 bit,
    //accounting for an overflow that fired but whose IRQ has not run yet.
    unsigned int counter=IRQtimeCounter();
    if(IRQtimeOverflowFlag() && IRQtimeCounter()>=counter)
        return (upperTimeTick | static_cast<long long>(counter)) + upperIncr;
    return upperTimeTick | static_cast<long long>(counter);
}

long long getTime() noexcept
{
    FastGlobalIrqLock dLock;
    return ticksToNs(IRQgetTimeTick());
}

long long IRQgetTime() noexcept
{
    return ticksToNs(IRQgetTimeTick());
}

//----- wakeup / preemption interrupt (one-shot on WAKE_CH) --------------------

// Largest relative horizon the 32-bit WAKE counter can represent, in ns (~102s
// at 42 MHz). The scheduler arms IRQosTimerSetInterrupt(numeric_limits::max())
// when idle with no sleeper; ns must be turned into a RELATIVE delay and clamped
// to this BEFORE nsToTicks(), otherwise nsToTicks(huge_abs) overflows 64-bit ->
// garbage -> rel<1 -> the timer fires ASAP forever (runaway flood that starves
// every thread). Found 2026-06-03; this was THE Thread::sleep blocker.
static const long long WAKE_MAX_HORIZON_NS = (0xFFFFFFFFLL * 500LL) / 21LL;

void IRQosTimerSetInterrupt(long long ns) noexcept
{
    //ns is an ABSOLUTE time point. Program WAKE_CH for the RELATIVE delay,
    //computing the delay in ns and clamping BEFORE the ns->ticks conversion so
    //an "infinity" deadline can't overflow nsToTicks() (see horizon note above).
    irqNs=ns;
    long long now=IRQgetTimeTick();
    long long relNs=ns-ticksToNs(now);
    if(relNs<0) relNs=0;
    if(relNs>WAKE_MAX_HORIZON_NS) relNs=WAKE_MAX_HORIZON_NS;
    long long rel=nsToTicks(relNs);
    if(rel<1) rel=1;                       //past/now -> fire ASAP (no force-pend HW)
    if(rel>0xFFFFFFFFLL) rel=0xFFFFFFFFLL;  //32-bit timer ceiling (~102 s)

    TMR_CTRL(WAKE_CH)=TMR_CTRL_USERDEF;                 //stop, user-defined (reload) mode
    (void)TMR_EOI(WAKE_CH);                              //clear any stale IRQ
    TMR_LOAD(WAKE_CH)=static_cast<unsigned int>(rel);
    TMR_CTRL(WAKE_CH)=TMR_CTRL_USERDEF|TMR_CTRL_ENABLE; //enable, IRQ unmasked (bit2=0)
}

/// WAKE_CH ISR body (registered via IRQregisterIrq; runs inside csky_isr_dispatch).
static void wakeIrqHandler(void*)
{
    //One-shot: disable so it doesn't reload+refire. Clear the channel IRQ.
    TMR_CTRL(WAKE_CH)=TMR_CTRL_USERDEF|TMR_CTRL_INTMASK; //disabled, masked
    (void)TMR_EOI(WAKE_CH);

    long long now=irqNs;
    irqNs=0x7FFFFFFFFFFFFFFFLL;
    IRQwakeThreads(now);   //kernel wakes due threads + reschedules (sets s_schedPending)
}

/// TIME_CH overflow ISR body: clear the channel IRQ (Timer1EOI is read-clear,
/// manual §4.8.5.4) and extend the upper 32 bits. NB: the spurious-IRQ storm
/// once seen at the first overflow was NOT a timer-clear problem — it was the
/// PIC dispatcher failing to clear its latched interrupt-request record (fixed
/// in csky_isr_dispatch / interrupts.cpp). This handler is the plain form.
static void timeOverflowIrqHandler(void*)
{
    (void)TMR_EOI(TIME_CH);        //clear the overflow IRQ
    upperTimeTick+=upperIncr;
}

//----- init -------------------------------------------------------------------

// Lock-free early-boot IRQ registration (interrupts.cpp); avoids constructing a
// pre-kernel GlobalIrqLock here (suspected boot hang).
void cskyRegisterIrqNoLock(unsigned int id, void (*handler)(void*), void *arg) noexcept;

void IRQosTimerInit()
{
    //Set up the fixed-point tick<->ns conversion from the timer frequency
    //(verified on silicon — see the ticksToNs/nsToTicks note above).
    tc=TimeConversion(HRC7000_TIMER_HZ);

    //TIME_CH: free-running down-counter from 0xFFFFFFFF, overflow IRQ unmasked.
    TMR_CTRL(TIME_CH)=0u;                       //disable to reload
    TMR_LOAD(TIME_CH)=0xFFFFFFFFu;
    (void)TMR_EOI(TIME_CH);                      //clear stale
    TMR_CTRL(TIME_CH)=TMR_CTRL_ENABLE;           //enable, free-run (bit1=0), IRQ unmasked (bit2=0)

    //WAKE_CH: idle (disabled+masked) until IRQosTimerSetInterrupt arms it.
    TMR_CTRL(WAKE_CH)=TMR_CTRL_USERDEF|TMR_CTRL_INTMASK;
    (void)TMR_EOI(WAKE_CH);

    //Route both timer IRQs through the PIC dispatcher (id = PIC source number).
    //Lock-free: early boot is single-threaded with IE off.
    cskyRegisterIrqNoLock(TIME_PIC_SRC,timeOverflowIrqHandler,nullptr);
    cskyRegisterIrqNoLock(WAKE_PIC_SRC,wakeIrqHandler,nullptr);

    upperTimeTick=0;
    irqNs=0x7FFFFFFFFFFFFFFFLL;
}

unsigned int osTimerGetFrequency()
{
    return HRC7000_TIMER_HZ;
}

} //namespace miosix
