##
## Makefile for Miosix embedded OS
##
## This file contains the options required by the build system to build
## Miosix on various target architectures. All options start with OPT_
## to be easily recognizable.
## All architecture specific build code is grouped at the end of this file.
##

OPT_BOARD := openrtx

##
## Optimization flags, choose one.
## -O0 produces large and slow code, but is useful for in circuit debugging.
## -O2 is recomended otherwise, as it provides a good balance between code
## size and speed
##
#OPT_OPTIMIZATION := -O0
#OPT_OPTIMIZATION := -O2
#OPT_OPTIMIZATION := -O3
OPT_OPTIMIZATION := -Os

##
## C++ Exception/rtti support disable flags.
## To save code size if not using C++ exceptions (nor some STL code which
## implicitly uses it) uncomment this option.
## the -D__NO_EXCEPTIONS is used by Miosix to know if exceptions are used.
##
OPT_EXCEPT := -fno-exceptions -fno-rtti -D__NO_EXCEPTIONS

##
## Then, initialize C/C++ flags
##
CXXSTD        := -std=c++14
CFLAGS_BASE   := -D_MIOSIX_BOARDNAME=\"$(OPT_BOARD)\" -D_DEFAULT_SOURCE=1 \
                 -ffunction-sections -Wall -Werror=return-type -g
CXXFLAGS_BASE := -D_MIOSIX_BOARDNAME=\"$(OPT_BOARD)\" -D_DEFAULT_SOURCE=1 \
                 $(CXXSTD) -ffunction-sections -Wall -Werror=return-type -g


ARCH_INC  := arch/cortexM4_stm32f4/common
BOARD_INC := arch/cortexM4_stm32f4/M4_openrtx
BOOT_FILE :=
ARCH_SRC  :=

CFLAGS_BASE   += -D_OPENRTX
CXXFLAGS_BASE += -D_OPENRTX

## Select compiler
PREFIX := arm-miosix-eabi-

## From compiler prefix form the name of the compiler and other tools
CC  := $(PREFIX)gcc
CXX := $(PREFIX)g++
LD  := $(PREFIX)ld
AR  := $(PREFIX)ar
AS  := $(PREFIX)as
CP  := $(PREFIX)objcopy
OD  := $(PREFIX)objdump
SZ  := $(PREFIX)size

## Select appropriate compiler flags for both ASM/C/C++/linker
AFLAGS_BASE   := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS_BASE   += -D_ARCH_CORTEXM4_STM32F4 $(CLOCK_FREQ) $(XRAM) $(SRAM_BOOT)\
                 -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
                 $(OPT_OPTIMIZATION) -c
CXXFLAGS_BASE += -D_ARCH_CORTEXM4_STM32F4 $(CLOCK_FREQ) $(XRAM) $(SRAM_BOOT)\
                 -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
                 $(OPT_EXCEPT) $(OPT_OPTIMIZATION) -c
LFLAGS_BASE   := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
                 -Wl,--gc-sections,-Map,main.map $(OPT_EXCEPT)              \
                 $(OPT_OPTIMIZATION) -nostdlib

## Select architecture specific files
## These are the files in arch/<arch name>/common
ARCH_SRC +=                                              \
arch/common/core/interrupts_cortexMx.cpp                 \
arch/common/core/mpu_cortexMx.cpp                        \
arch/common/drivers/dcc.cpp                              \
$(ARCH_INC)/interfaces-impl/portability.cpp              \
$(ARCH_INC)/interfaces-impl/gpio_impl.cpp
