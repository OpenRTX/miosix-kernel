
##
## ----------------------- Common kernel sources ----------------------------
##

miosix_inc = ['lib/miosix-kernel/miosix',
              'lib/miosix-kernel/miosix/util',
              'lib/miosix-kernel/miosix/arch/common',
              'lib/miosix-kernel/miosix/arch/common/CMSIS/Include']

miosix_src = ['lib/miosix-kernel/miosix/kernel/kernel.cpp',
              'lib/miosix-kernel/miosix/kernel/sync.cpp',
              'lib/miosix-kernel/miosix/kernel/error.cpp',
              'lib/miosix-kernel/miosix/kernel/pthread.cpp',
              'lib/miosix-kernel/miosix/kernel/stage_2_boot.cpp',
              'lib/miosix-kernel/miosix/kernel/timeconversion.cpp',
              'lib/miosix-kernel/miosix/kernel/intrusive.cpp',
              'lib/miosix-kernel/miosix/kernel/scheduler/priority/priority_scheduler.cpp',
              'lib/miosix-kernel/miosix/kernel/error.cpp',
              'lib/miosix-kernel/miosix/filesystem/console/console_device.cpp',
              'lib/miosix-kernel/miosix/filesystem/devfs/devfs.cpp',
              'lib/miosix-kernel/miosix/stdlib_integration/libc_integration.cpp',
              'lib/miosix-kernel/miosix/stdlib_integration/libstdcpp_integration.cpp',
              'lib/miosix-kernel/miosix/arch/common/core/interrupts_cortexMx.cpp',
              'lib/miosix-kernel/miosix/arch/common/core/cache_cortexMx.cpp',
              'lib/miosix-kernel/miosix/arch/common/core/mpu_cortexMx.cpp',
              'lib/miosix-kernel/miosix/util/util.cpp',
              'lib/miosix-kernel/miosix/util/version.cpp']

miosix_def = {'COMPILING_MIOSIX' : '', '_POSIX_PRIORITY_SCHEDULING': '', '_MIOSIX_BOARDNAME' : '"OpenRTX"'}

##
## ----------------------- ARM Cortex M4 ----------------------------
##

##
## STM32F4
##

stm32f405_inc = miosix_inc + ['lib/miosix-kernel/miosix/arch/common/CMSIS/Device/ST/STM32F4xx/Include',
                              'lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/common',
                              'lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/common/interfaces-impl',
                              'lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/stm32f405_generic',
                              'lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/stm32f405_generic/interfaces-impl',
                              'lib/miosix-kernel/miosix/config/arch/cortexM4_stm32f4/stm32f405_generic']

stm32f405_src = miosix_src + ['lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/common/interfaces-impl/portability.cpp',
                              'lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/common/interfaces-impl/delays.cpp',
                              'lib/miosix-kernel/miosix/arch/common/drivers/stm32_gpio.cpp',
                              'lib/miosix-kernel/miosix/arch/common/core/stm32_32bit_os_timer.cpp',
                              'lib/miosix-kernel/miosix/arch/common/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c',
                              'lib/miosix-kernel/miosix/arch/cortexM4_stm32f4/stm32f405_generic/core/stage_1_boot.cpp']

stm32f405_def = miosix_def + {'STM32F405xx':'', '_ARCH_CORTEXM4_STM32F4':'', 'SYSCLK_FREQ_168MHz':'168000000'}

##
## MK22FN512
##

mk22fn512_inc = miosix_inc + ['lib/miosix-kernel/miosix/arch/common/CMSIS/Device/NXP/MK22FN512xxx12/Include',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/common',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/common/interfaces-impl',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/mk22fn512vxx12_generic',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/mk22fn512vxx12_generic/interfaces-impl',
                              'lib/miosix-kernel/miosix/config/arch/cortexM4_mk22/mk22fn512vxx12_generic']

mk22fn512_src = miosix_src + ['lib/miosix-kernel/miosix/arch/cortexM4_mk22/common/interfaces-impl/portability.cpp',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/common/interfaces-impl/delays.cpp',
                              'lib/miosix-kernel/miosix/arch/common/drivers/mk22_gpio.cpp',
                              'lib/miosix-kernel/miosix/arch/common/core/mk22_os_timer.cpp',
                              'lib/miosix-kernel/miosix/arch/common/CMSIS/Device/NXP/MK22FN512xxx12/Source/system_MK22F51212.c',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/common/interfaces-impl/portability.cpp',
                              'lib/miosix-kernel/miosix/arch/cortexM4_mk22/mk22fn512vxx12_generic/core/stage_1_boot.cpp']

mk22fn512_def = miosix_def + {'_ARCH_CORTEXM4_MK22':'', 'CPU_MK22FN512VLH12':'', 'MK22FN512xx':''}

##
## ----------------------- ARM Cortex M7 ----------------------------
##

##
## STM32H7
##

stm32h743_inc = miosix_inc + ['lib/miosix-kernel/miosix/arch/common/CMSIS/Device/ST/STM32H7xx/Include',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/common',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/common/interfaces-impl',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/stm32h743_generic',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/stm32h743_generic/interfaces-impl',
                              'lib/miosix-kernel/miosix/config/arch/cortexM7_stm32h7/stm32h743_generic']

stm32h743_src = miosix_src + ['lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/common/interfaces-impl/portability.cpp',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/common/interfaces-impl/delays.cpp',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/common/drivers/pll.cpp',
                              'lib/miosix-kernel/miosix/arch/common/drivers/stm32_gpio.cpp',
                              'lib/miosix-kernel/miosix/arch/common/core/stm32_32bit_os_timer.cpp',
                              'lib/miosix-kernel/miosix/arch/common/CMSIS/Device/ST/STM32H7xx/Source/Templates/system_stm32h7xx.c',
                              'lib/miosix-kernel/miosix/arch/cortexM7_stm32h7/stm32h743_generic/core/stage_1_boot.cpp']

stm32h743_def = miosix_def + {'STM32H743xx':'', '_ARCH_CORTEXM7_STM32H7':'', 'SYSCLK_FREQ_400MHz':'400000000'}
