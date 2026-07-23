/***************************************************************************
 *   Copyright (C) 2013-2024 by Terraneo Federico                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#pragma once

#ifndef COMPILING_MIOSIX
#error "This is header is private, it can't be used outside Miosix itself."
#error "If your code depends on a private header, it IS broken."
#endif //COMPILING_MIOSIX

namespace miosix {

/**
 * \internal
 * Each architecture-specific boot code must call this function to start the
 * Miosix boot process.
 *
 * Preconditions for calling this functions are that:
 * - Interrupts are disabled.
 *   CPUs that boot with interrupts enabled require to disable interrupts first.
 * - The CPU is running at the desired operating frequency.
 *   For architectures that include a PLL or other software-configurable clock
 *   option, it is assumed that the configuration occurred before this function
 *   is called.
 * - The whole RAM memory as specified by the linker script is accessible.
 *   For boards that include an external (SRAM or SDRAM) memory that requires
 *   architecture-specific code to be made accessible, it is assumed that the
 *   configuration occurred before this function is called.
 *   For boards that require the FLASH memory wait states to be configured,
 *   it is assumed code to configure the FLASH wait states to a value
 *   appropriate for the CPU desired operating frequency was already executed
 *   before this function is called.
 *
 * This function initiates the Miosix boot as follows:
 * - Global/static variables in .data and .bss are initialized.
 * - The interrupt table is initialized by calling back IRQinitIrqTable() into
 *   the architecture-specific code.
 * - Global C++ constructors of the kernel and device drivers are called.
 *   These are called early at boot as the kernel relies on them.
 * - The first part of the architecture-specific bsp is run, IRQbspInit().
 *   From this point it is assumed the console is available to print boot logs.
 * - The OS timer is started.
 *   From this point, the kernel starts keeping track of time.
 * - Interrupts are enabled and the first context switch is performed.
 * - The second part of the architecture-specific bsp is run, bspInit2().
 *   From this point if filesystem support is enabled, it is assumed the root
 *   filesystem is mounted.
 * - Global C++ constructors of the kernelspace applications are called.
 *   These are called late at boot as they may require access to the filesystem
 *   and/or to functionalities that are available only after the kernel is
 *   started, such as creating threads or accessing blocking device drivers.
 * - The kernelspace main() is called.
 *   If userspace support is enabled, it is the purpose of this user-supplied
 *   function to spawn userspace processes, e.g: /bin/init
 *
 * When the kernelspace main() function returs, the kernel performs a system
 * shutdown.
 *
 * This function, kernelBootEntryPoint(), shall not return unless the boot fails
 */
void kernelBootEntryPoint();

/**
 * \internal
 * This function will perform the part of system initialization that must be
 * done after the kernel is started. At the end, it will call main()
 * It is an internal detail of how the kernel works, architecture-specific code
 * doesn't need to interact with it.'
 * \param argv ignored parameter
 */
void *mainLoader(void *argv);

} //namespace miosix
