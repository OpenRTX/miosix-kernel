/***************************************************************************
 *   CK803S (C-SKY V2) / HR_C7000 interrupt layer for modern Miosix.        *
 *   GPL v2+ with the Miosix linking exception.                            *
 *                                                                          *
 *   Implements the interfaces/interrupts.h contract for the HR_C7000:           *
 *     - IRQinitIrqTable()         build+install the VBR table, init the PIC *
 *     - IRQregisterIrqOnCore()    bind a dynamic handler to a PIC source    *
 *     - IRQunregisterIrqOnCore()  unbind + mask                            *
 *     - IRQisIrqRegistered()                                               *
 *   plus the extern "C" C bodies the cskyv2_context.S entry stubs call.    *
 *                                                                          *
 *   Hardware recipe verified on real HR_C7000 silicon:                          *
 *                                                                          *
 *     timer/peripheral IRQ -> PIC @ 0x17000000 (level, active-high)        *
 *       -> CK803 autovectors via VBR (cr<1,0>): PC = *(VBR + vec*4)        *
 *       -> vector = HRC7000_PIC_VECTOR0(32) + PIC source number.               *
 *     IRQ end: peripheral clears its own request (e.g. read TimerN_EOI),   *
 *       then write PIC_COW1 = eoi(bit2).                                    *
 *                                                                          *
 *   The "id" used throughout the Miosix IRQ API is the PIC source number   *
 *   (0..63); its CK803S vector is id + HRC7000_PIC_VECTOR0.                     *
 ***************************************************************************/

#include <cstdio>
#include "interfaces/interrupts.h"
#include "interfaces/arch_registers.h"
#include "interfaces_private/cpu.h"             //ctxsave (resume-frame forensics)
#include "kernel/scheduler/scheduler.h"

namespace miosix {

//
// CK803S substitute for the Cortex-M PendSV-set bit (see cpu_impl.h).
//
volatile bool s_schedPending=false;

//True while inside the PIC dispatcher (IRQ context). IRQinvokeScheduler() uses
//this to decide: in IRQ context -> defer (s_schedPending); in thread context
//(even under the global lock / IE off) -> switch synchronously.
volatile bool s_inIrq=false;

//
// VBR table: CK803S_VBR_NVEC word entries, each a handler ADDRESS. CK803 loads the
// word at VBR + vec*4 and jumps to it. Must be 1 KiB aligned (VBR alignment).
//
static unsigned int g_vbrTable[CK803S_VBR_NVEC] __attribute__((aligned(1024)));

//
// Dynamic per-source handler table. One slot per PIC source (0..63).
//
static constexpr unsigned int NUM_PIC_SOURCES=64;
struct IrqSlot { void (*fn)(void*); void *arg; };
static IrqSlot g_irqSlots[NUM_PIC_SOURCES];

//
// Naked entry stubs (cskyv2_context.S).
//
extern "C" void yield_isr_entry();      // VBR[16] = trap 0  (cooperative yield)
extern "C" void generic_irq_entry();    // VBR[32..95] = PIC sources
extern "C" void fault_entry();          // all CPU-exception + stray vectors

//
// Board-supplied low-level early-console write (bring-up diagnostic path). Weak
// so the CPU layer links even on a board that provides no early console: if it
// is absent the fault reporter simply produces no text and still halts. The
// board implements it (e.g. over a debug UART); to be rerouted through the
// standard Miosix console once one is wired.
//
extern "C" void miosixEarlyConsoleWrite(const char *s) __attribute__((weak));

//
// Boot entry. The generic linker script sets ENTRY(miosix::Reset_Handler) and
// KEEPs .isr_vector at the very start of flash; the Dahua IAP jumps to the
// image base (0x0300d000) and EXECUTES there, so
// Reset_Handler must be the first code. It sets the boot stack, runs early
// clock init (IRQmemoryAndClockInit, board layer), then hands off to the kernel
// boot (IRQkernelBootEntryPoint does .data/.bss/ctors/IRQinitIrqTable/IRQbspInit
// /IRQosTimerInit/IRQstartKernel). Mirrors armv4's Reset_Handler. Naked: pure
// asm, no compiler-generated prologue, and it switches SP mid-function. The
// mangled callee names match the kernel's C++ symbols.
//
void Reset_Handler() __attribute__((naked,section(".isr_vector")));
void Reset_Handler()
{
    asm volatile(
        //Mirror RT-Thread's ck803 reset_handler entry: put the core in the known
        //mode it expects (PSR=0x80000000 = S/supervisor, IE/EE off) and clear the
        //random-prefetch enable (cr<31,0>[3]=RPE). Without this PSR setup, rte
        //from normal context does not transfer control on this core (HW-found).
        "lrw  r0, 0x80000000                        \n\t"
        "mtcr r0, psr                               \n\t"
        "mfcr r0, cr<31, 0>                         \n\t"
        "bclri r0, 3                                \n\t"
        "mtcr r0, cr<31, 0>                         \n\t"
        "lrw  r0, _irq_stack_top                    \n\t" //small stack in internal RAM
        "mov  sp, r0                                \n\t"
        "jbsr _ZN6miosix21IRQmemoryAndClockInitEv   \n\t" //early clock/memory init
        "lrw  r0, _heap_end                         \n\t" //big boot stack (top of heap)
        "mov  sp, r0                                \n\t"
        "jbsr _ZN6miosix23IRQkernelBootEntryPointEv \n\t" //never returns
    );
}

//
// CPU-fault reporter, called by fault_entry (cskyv2_context.S) with a pointer
// to the 72-byte register frame it snapshotted and the live PSR. Prints the
// faulting PC (EPC), PSR/EPSR and the full register file over the board debug
// UART, then halts so the message stays on the wire for capture.
//
// Frame layout (matches cskyv2_context.S): f[0..13]=r0..r13, f[14]=r15(lr),
// f[15]=EPC (faulting PC), f[16]=EPSR (faulting PSR), f[17]=pad. The faulting
// SP is the frame address + FRAME_SIZE (the frame was pushed onto that stack).
//
// The EPC is the key datum: disassemble the linked image at that address to see
// exactly which instruction trapped (an unaligned ld/st, a soft-float helper
// call, an illegal encoding, ...). Halting (rather than rebooting) is a
// deliberate bring-up choice — swap the final spin for IRQsystemReboot() once
// faults are diagnosed and the radio should self-recover in the field.
//
//
// CK803S exception-vector number -> human-readable cause. Numbers are per the
// CK803S VIC user guide §3.3.3 "Interrupt vector number", cross-checked against
// Linux arch/csky (asm/traps.h VEC_*) and RT-Thread's ck803 port. The faulting
// vector is carried in PSR.VEC = bits[23:16], i.e. (psr>>16)&0xff — the same
// decode as Linux trap_no().
//
static const char *faultVecName(unsigned int vec)
{
    switch(vec)
    {
        case 1:  return "unaligned access";
        case 2:  return "access error (bus)";
        case 3:  return "divide by zero";
        case 4:  return "illegal instruction";
        case 5:  return "privilege violation";
        case 6:  return "trace";
        case 7:  return "breakpoint";
        case 8:  return "unrecoverable error";
        case 9:  return "soft reset / idly";
        case 10: return "autovector irq";
        case 17: case 18: case 19: return "trap #1-3";
        case 22: return "tspend";
        default: return "unknown/reserved";
    }
}

extern "C" void csky_fault_handler(unsigned int *f, unsigned int psr)
{
    //Re-entrancy guard: if the reporter itself faults (e.g. the debug UART or
    //sniprintf trap), do not recurse — just halt on the second entry.
    static volatile bool reporting=false;
    if(!reporting)
    {
        reporting=true;
        if(miosixEarlyConsoleWrite)
        {
            char b[112];
            unsigned int vec=(psr>>16)&0xFFu;                        //PSR.VEC[23:16]
            unsigned int sp=reinterpret_cast<unsigned int>(f)+0x48u; //+FRAME_SIZE
            sniprintf(b,sizeof b,"\r\n*** CK803S CPU FAULT: vec=%u (%s) ***\r\n",
                      vec,faultVecName(vec));
            miosixEarlyConsoleWrite(b);
            sniprintf(b,sizeof b,"psr=%08x epc=%08x epsr=%08x sp=%08x lr=%08x\r\n",
                      psr,f[15],f[16],sp,f[14]);
            miosixEarlyConsoleWrite(b);
            sniprintf(b,sizeof b,"r0-3  =%08x %08x %08x %08x\r\n"
                                 "r4-7  =%08x %08x %08x %08x\r\n",
                      f[0],f[1],f[2],f[3],f[4],f[5],f[6],f[7]);
            miosixEarlyConsoleWrite(b);
            sniprintf(b,sizeof b,"r8-11 =%08x %08x %08x %08x\r\n"
                                 "r12-13=%08x %08x\r\n",
                      f[8],f[9],f[10],f[11],f[12],f[13]);
            miosixEarlyConsoleWrite(b);
            miosixEarlyConsoleWrite("disassemble the image at epc for the faulting "
                                    "instruction. halted.\r\n");
        }
    }
    for(;;) ;
}

//-----------------------------------------------------------------------------
// C bodies called by the .S entry stubs (between CTX_SAVE and CTX_RESTORE).
//-----------------------------------------------------------------------------

/// Run the scheduler (repoint ctxsave[]) — called by csky_yield_switch (after
/// it has saved the current thread) and by the IRQ dispatcher.
extern "C" void csky_isr_yield()
{
    Scheduler::IRQrunScheduler();
}

/// PIC path: service every fired+enabled source, PIC-EOI, then reschedule iff a
/// handler requested it (IRQinvokeScheduler set s_schedPending while IE was off).
extern "C" void csky_isr_dispatch()
{
    s_inIrq=true;   //handlers calling IRQinvokeScheduler defer (don't switch here)

    //Only sources 0..31 (the timer tick) are wired today; scan the high half
    //too so adding a >=32 source later "just works".
    unsigned int pend0=HRC7000_PIC_INT_ST  & ~HRC7000_PIC_MASK;
    unsigned int pend1=HRC7000_PIC_INT_ST1 & ~HRC7000_PIC_MASK1;
    for(unsigned int src=0; src<32; src++)
        if(pend0 & (1u<<src)) { IrqSlot &s=g_irqSlots[src]; if(s.fn) s.fn(s.arg); }
    for(unsigned int src=0; src<32; src++)
        if(pend1 & (1u<<src)) { IrqSlot &s=g_irqSlots[32+src]; if(s.fn) s.fn(s.arg); }

    //Clear the PIC's latched interrupt-request RECORD for every source we just
    //serviced. PIC_INT_ST is a write-1-to-clear record register (manual
    //§4.7.4.2: "Each interrupt source can only record one interrupt request.
    //Writing 1 to the corresponding bit clears the status."). COW1=eoi only pops
    //the single highest-priority in-service entry (the ISR) — it does NOT clear
    //these records. Without this clear, as soon as TWO sources are pending in the
    //same dispatch (which begins when the TIME_CH overflow at ~102s adds a 2nd
    //frequently-firing source alongside WAKE_CH) the un-popped record stays
    //latched forever and is re-dispatched on every subsequent IRQ — a spurious
    //IRQ storm that re-runs the overflow handler ~thousands/s, racing getTime()
    //and collapsing all sleeps into a busy-loop. Write AFTER the handlers run so
    //a level source they de-asserted stays clear; one still asserting re-latches.
    if(pend0) HRC7000_PIC_INT_ST =pend0;
    if(pend1) HRC7000_PIC_INT_ST1=pend1;
    HRC7000_PIC_COW1=HRC7000_PIC_COW1_EOI;          //PIC interrupt-end (pop in-service)
    asm volatile("":::"memory");

    s_inIrq=false;  //about to leave IRQ ctx (generic_irq_entry's CTX_RESTORE switches)
    if(s_schedPending) { s_schedPending=false; Scheduler::IRQrunScheduler(); }
}

//-----------------------------------------------------------------------------
// interfaces/interrupts.h contract
//-----------------------------------------------------------------------------

void IRQinitIrqTable() noexcept
{
    //All vectors -> fault_entry (report EPC/EPSR/regs + halt), except trap 0
    //(yield) and the PIC range. Every CK803S CPU exception (misaligned access,
    //access error, illegal instruction, privilege, div0, trap1..3) and any stray
    //vector is now diagnosable instead of silently spinning.
    for(unsigned int v=0; v<CK803S_VBR_NVEC; v++)
        g_vbrTable[v]=reinterpret_cast<unsigned int>(&fault_entry);
    g_vbrTable[CK803S_YIELD_VEC]=reinterpret_cast<unsigned int>(&yield_isr_entry);
    for(unsigned int src=0; src<NUM_PIC_SOURCES; src++)
        g_vbrTable[HRC7000_PIC_VECTOR0+src]=reinterpret_cast<unsigned int>(&generic_irq_entry);

    for(unsigned int i=0; i<NUM_PIC_SOURCES; i++) g_irqSlots[i]=IrqSlot{nullptr,nullptr};

    //Install VBR (cr<1,0>).
    csky_set_vbr(g_vbrTable);

    //PIC: level-triggered, active-high, all pending cleared, ALL masked. The
    //timer/peripheral sources are unmasked by IRQregisterIrqOnCore(). IE stays
    //OFF here — IRQportableStartKernel() enables it for the first switch.
    HRC7000_PIC_MODE   =0u;
    HRC7000_PIC_PO     =0xFFFFFFFFu;
    HRC7000_PIC_MODE1  =0u;
    HRC7000_PIC_PO1    =0xFFFFFFFFu;
    HRC7000_PIC_INT_ST =0xFFFFFFFFu;
    HRC7000_PIC_INT_ST1=0xFFFFFFFFu;
    HRC7000_PIC_MASK   =0xFFFFFFFFu;
    HRC7000_PIC_MASK1  =0xFFFFFFFFu;
}

void IRQregisterIrqOnCore(GlobalIrqLock&, unsigned char /*coreId*/, unsigned int id,
                          void (*handler)(void*), void *arg) noexcept
{
    if(id>=NUM_PIC_SOURCES) return;
    g_irqSlots[id]=IrqSlot{handler,arg};
    //Unmask the PIC source (bit=0 enables).
    if(id<32) HRC7000_PIC_MASK  &= ~(1u<<id);
    else      HRC7000_PIC_MASK1 &= ~(1u<<(id-32));
}

void IRQunregisterIrqOnCore(GlobalIrqLock&, unsigned char /*coreId*/, unsigned int id,
                            void (*/*handler*/)(void*), void */*arg*/) noexcept
{
    if(id>=NUM_PIC_SOURCES) return;
    //Mask the PIC source first, then drop the handler.
    if(id<32) HRC7000_PIC_MASK  |= (1u<<id);
    else      HRC7000_PIC_MASK1 |= (1u<<(id-32));
    g_irqSlots[id]=IrqSlot{nullptr,nullptr};
}

bool IRQisIrqRegistered(unsigned int id) noexcept
{
    if(id>=NUM_PIC_SOURCES) return false;
    return g_irqSlots[id].fn!=nullptr;
}

// Lock-free IRQ registration for EARLY BOOT only (single-threaded, IE off):
// bind a handler + unmask its PIC source, without constructing a GlobalIrqLock.
// Used by IRQosTimerInit, which runs before the kernel is started where the
// GlobalIrqLock machinery may not yet be usable.
void cskyRegisterIrqNoLock(unsigned int id, void (*handler)(void*), void *arg) noexcept
{
    if(id>=NUM_PIC_SOURCES) return;
    g_irqSlots[id]=IrqSlot{handler,arg};
    if(id<32) HRC7000_PIC_MASK  &= ~(1u<<id);
    else      HRC7000_PIC_MASK1 &= ~(1u<<(id-32));
}

// Non-SMP single-core convenience overloads: interfaces/interrupts.h declares
// (but does not inline) these when WITH_SMP is off — forward to the OnCore
// variant with coreId 0. (Default arg lives in the header declaration.)
void IRQregisterIrq(GlobalIrqLock& lock, unsigned int id,
                    void (*handler)(void*), void *arg) noexcept
{
    IRQregisterIrqOnCore(lock,0,id,handler,arg);
}

void IRQunregisterIrq(GlobalIrqLock& lock, unsigned int id,
                      void (*handler)(void*), void *arg) noexcept
{
    IRQunregisterIrqOnCore(lock,0,id,handler,arg);
}

} //namespace miosix
