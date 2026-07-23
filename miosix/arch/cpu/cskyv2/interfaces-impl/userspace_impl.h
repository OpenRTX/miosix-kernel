/***************************************************************************
 *   CK803S (C-SKY V2) userspace interface for modern Miosix.               *
 *   GPL v2+ with the Miosix linking exception.                            *
 ***************************************************************************/

#pragma once

// Processes (userspace with memory protection) are not supported on the HR_C7000
// CK803S port: there is no MPU we use and the toolchain provides no userspace
// multilib. The HR_C7000 runs Miosix as a unikernel. No declarations are required
// here (mirrors arch/cpu/armv4).
