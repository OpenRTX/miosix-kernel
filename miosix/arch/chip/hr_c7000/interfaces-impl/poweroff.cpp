/***************************************************************************
 *   HR_C7000 (CK803S) low-level reboot for modern Miosix.                  *
 *   GPL v2+ with the Miosix linking exception.                            *
 *                                                                          *
 *   Only IRQsystemReboot() belongs to the chip layer; shutdown()/reboot()  *
 *   are provided by the board bsp.cpp (they need the HR_C7000 power latch        *
 *   on GPIOB.13).                                                           *
 ***************************************************************************/

#include "interfaces/poweroff.h"
#include "interfaces/arch_registers.h"   // HRC7000_BOOTROM_RESET

namespace miosix {

void IRQsystemReboot()
{
    //Re-enter the BOOTROM reset entry. A clean
    //reboot must NOT jump to stage-1 0x03000000, which expects power-on
    //register state and panics with dirty regs.
    reinterpret_cast<void(*)()>(HRC7000_BOOTROM_RESET)();
    for(;;) ;
}

} //namespace miosix
