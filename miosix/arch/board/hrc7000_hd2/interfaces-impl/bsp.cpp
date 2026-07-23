/***************************************************************************
 *   Board support package for the Ailunce HD2 (HR_C7000 / CK803S).         *
 *   GPL v2+ with the Miosix linking exception.                            *
 *                                                                          *
 *   Provides: IRQbspInit (board peripherals), bspInit2, shutdown, reboot.  *
 *   Early PLL bring-up (IRQmemoryAndClockInit) lives in boot.cpp.          *
 *                                                                          *
 *   A polled-TX UART0 debug console (hd2_dbg_puts, 57600 8N1) is wired      *
 *   for bring-up tracing; bring-up is also observable via the LEDs         *
 *   (bsp_impl.h ledOn/ledOff = GPIOB.0 green). Full Miosix stdio/iprintf   *
 *   routing through a Device is not yet wired.                             *
 *                                                                          *
 *   NOTE: after a YMODEM flash the IAP does NOT reliably leave DFU on the  *
 *   '3' boot byte (radio sits solid-red); a cold power-cycle (battery      *
 *   pull) boots the freshly flashed image. Capture serial across that.     *
 ***************************************************************************/

#include <sys/ioctl.h>
#include "interfaces/bsp.h"
#include "interfaces_private/bsp_private.h"
#include "interfaces/poweroff.h"
#include "interfaces/arch_registers.h"
#include "kernel/lock.h"
#include "miosix_settings.h"
#include "filesystem/console/console_device.h"
#include "filesystem/devfs/devfs.h"

namespace miosix {

//-----------------------------------------------------------------------------
// Polled serial on UART0 (0x14030000, the loader/debug UART the flashing bridge
// reads at 57600). DesignWare 16550: RBR/THR/DLL +0x00, DLH/IER +0x04, FCR
// +0x08, LCR +0x0c, LSR +0x14. hd2_dbg_puts() is a bare puts the board and the
// CPU fault path can call directly (bounded spin, cannot hang); the HD2Console
// Device below wraps it and is registered as the default console, so
// stdout/printf/bootlog route here too. We do NOT touch the divisor — the IAP
// already set UART0 to 57600 and we inherit it (baud confirmed empirically).
//-----------------------------------------------------------------------------
#define HD2_UART0(off)  (*(volatile unsigned int*)(0x14030000u+(off)))
#define UART0_THR       HD2_UART0(0x00u)
#define UART0_RBR       HD2_UART0(0x00u)   //RX buffer (read side of +0x00)
#define UART0_DLL       HD2_UART0(0x00u)
#define UART0_DLH       HD2_UART0(0x04u)
#define UART0_FCR       HD2_UART0(0x08u)
#define UART0_LCR       HD2_UART0(0x0cu)
#define UART0_LSR       HD2_UART0(0x14u)
#define UART_LSR_THRE   0x20u   //THR empty: ok to write
#define UART_LSR_DR     0x01u   //RX data ready

} //namespace miosix

//clk_init_pll halves the UART ref clock (84->42 MHz), so the IAP's divisor is
//now wrong. Reprogram for 57600 8N1 at 42 MHz: DLL = 42e6/(16*57600) ~= 46
//(the HW-verified value). MUST run AFTER
//IRQmemoryAndClockInit (clk_init_pll), which IRQbspInit does.
extern "C" void hd2_dbg_init()
{
    UART0_LCR=0x80u;        //DLAB=1
    UART0_DLL=46u;
    UART0_DLH=0u;
    UART0_LCR=0x03u;        //8N1, DLAB=0
    UART0_FCR=0x07u;        //enable + reset both FIFOs
}

extern "C" void hd2_dbg_putc(char c)
{
    for(unsigned int g=0; g<200000u && (UART0_LSR & UART_LSR_THRE)==0u; ++g) {}
    UART0_THR=static_cast<unsigned char>(c);
}

extern "C" void hd2_dbg_puts(const char *s)
{
    while(*s) hd2_dbg_putc(*s++);
}

//Neutral early-console hook the CPU-layer fault reporter (cskyv2/interrupts.cpp)
//calls via a weak symbol; route it to the board debug UART.
extern "C" void miosixEarlyConsoleWrite(const char *s){ hd2_dbg_puts(s); }

namespace miosix {

// HD2 GPIO bank B (DW_apb_gpio): DR +0x00, DDR +0x04. LEDs + power latch.
#define HD2_GPIOB_BASE    0x14100000u
#define HD2_GPIOB_DR      (*(volatile unsigned int*)(HD2_GPIOB_BASE+0x00u))
#define HD2_GPIOB_DDR     (*(volatile unsigned int*)(HD2_GPIOB_BASE+0x04u))
#define HD2_LED_GREEN     (1u<<0)    // PTB0, active-high
#define HD2_LED_RED       (1u<<1)    // PTB1, active-high
#define HD2_PWR_HOLD      (1u<<13)   // PTB13: power self-latch (HIGH=hold, LOW=cut)

/**
 * Minimal polled console Device for UART0, registered as the default console so
 * stdout/printf/iprintf and the kernel bootlog/error path route to the debug
 * UART. Output reuses hd2_dbg_putc (a bounded polled write — cannot hang);
 * IRQwrite() gives the kernel an IRQ-safe path for boot/fault logs; input is a
 * non-blocking poll of the RX FIFO (stdin is unused by this port today).
 */
class HD2Console : public Device
{
public:
    HD2Console() : Device(Device::STREAM) {}

    ssize_t writeBlock(const void *buffer, size_t size, off_t) override
    {
        const char *p=static_cast<const char*>(buffer);
        for(size_t i=0;i<size;i++) hd2_dbg_putc(p[i]);
        return static_cast<ssize_t>(size);
    }

    ssize_t readBlock(void *buffer, size_t size, off_t) override
    {
        char *p=static_cast<char*>(buffer);
        size_t n=0;
        while(n<size && (UART0_LSR & UART_LSR_DR)) p[n++]=static_cast<char>(UART0_RBR);
        return static_cast<ssize_t>(n);
    }

    void IRQwrite(const char *str) override { hd2_dbg_puts(str); }

    int ioctl(int cmd, void *) override
    {
        //Polled TX is synchronous, so a sync request is a no-op success.
        return cmd==IOCTL_SYNC ? 0 : -1;
    }
};

void IRQbspInit()
{
    //LEDs (PTB0/PTB1) as outputs, off; keep the power self-latch (PTB13) held.
    HD2_GPIOB_DDR|=(HD2_LED_GREEN|HD2_LED_RED|HD2_PWR_HOLD);
    HD2_GPIOB_DR =(HD2_GPIOB_DR & ~(HD2_LED_GREEN|HD2_LED_RED)) | HD2_PWR_HOLD;

    //Bring up the polled-TX UART0 debug console (57600 8N1). Must run after
    //IRQmemoryAndClockInit (clk_init_pll) so the divisor matches the 42 MHz ref.
    hd2_dbg_init();

    //Route stdout/printf/iprintf and the kernel bootlog through UART0. Must be
    //done here in IRQbspInit (IRQsetDefaultConsole contract); new is permitted
    //during boot.
    IRQsetDefaultConsole(intrusive_ref_ptr<Device>(new HD2Console));
}

void bspInit2()
{
    //Nothing yet (no filesystem). bspInit2 runs after the kernel is up.
}

void shutdown()
{
    ioctl(STDOUT_FILENO,IOCTL_SYNC,0);
    FastGlobalIrqLock::lock();
    //Vendor V2.1.3 power-off, done atomically with
    //IRQs off: drop the PTB13 power self-latch, then trigger a full system
    //soft-reset via the SOCSYS reset controller. BOTH steps matter:
    // - On battery the latch-drop removes the rail and the CPU stops here.
    // - When the rail is held up externally (USB) the latch-drop alone caused a
    //   partial brown-out that reset the CPU but left the PLL live, so the
    //   re-entry re-ran clk_init on a live PLL and HUNG (the "won't power up
    //   until the cable is pulled" wedge; a bootrom jump had the same flaw).
    //   SYS_SOFT_RSTN is a CLEAN reset that also resets the clock manager, so
    //   the cold boot that follows re-runs clk_init from a known state.
    //SOCSYS @0x11000000: QUAD_ENABLE(+0x5c)=0x1000000 (matches the vendor), then
    //SYS_SOFT_RSTN(+0x00) — active-low reset bits (assert = clear).
    //
    //We must reset the MODEM/AUDIO blocks here, not just CPU+system. Clearing
    //only bit8(cpu)/bit7(sys) (the old 0xfffffe7f) leaves the HR_C7000 modem
    //blocks (bits0-6: protocol/phy/fm/audio/codec/adc) RUNNING across the reset.
    //When the application has started the modem (any radio/audio bring-up),
    //those live blocks leave the clock tree in a state where the post-reset
    //clk_init HANGS -- the same "won't power up until the cable is pulled"
    //wedge, now re-triggered by the modem instead of the PLL. A build that
    //never brings the modem up never hit it.
    //Clearing bits0-8 (0xfffffe00) resets the whole modem+codec+adc+CPU+system
    //together, so the cold boot re-runs clk_init from a truly known state. At
    //power-off it is safe to also reset the volume ADC (bit6) -- the CPU is going
    //down anyway (unlike the runtime modem reset, which must spare bit6).
    HD2_GPIOB_DR&=~HD2_PWR_HOLD;
    *reinterpret_cast<volatile unsigned int*>(0x1100005cu)=0x01000000u;
    *reinterpret_cast<volatile unsigned int*>(0x11000000u)=0xfffffe00u;
    for(;;) { }   //the soft-reset takes effect; never returns
}

void reboot()
{
    ioctl(STDOUT_FILENO,IOCTL_SYNC,0);
    FastGlobalIrqLock::lock();
    IRQsystemReboot();
}

} //namespace miosix
