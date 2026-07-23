# Miosix port: Ailunce HD2 (HR_C7000 / CK803S)

A Miosix port for the **Ailunce HD2** handheld digital radio, built on the
**HR_C7000** SoC. It targets a new CPU architecture for Miosix — the C-SKY V2
**CK803S** core — and is structured as a normal three-layer Miosix port:

| Layer | Path | What it provides |
|-------|------|------------------|
| CPU   | `arch/cpu/cskyv2`      | CK803S context switch, interrupt/fault entry, atomics, endianness |
| Chip  | `arch/chip/hr_c7000`   | os-timer, GPIO, delays, cache (no-op), chip reboot |
| Board | `arch/board/hrc7000_hd2` | boot/clock init, BSP, console, linker script, board settings |

The CPU layer is intended to be reusable for any CK803S system; the chip layer
for any HR_C7000 system; only the board layer is HD2-specific.

## Hardware summary

- **CPU:** CK803S, C-SKY V2 (ABIv2), little-endian, **no FPU** (soft-float), **no
  MMU**, 16-register base file, a single stack pointer (interrupts nest on it).
- **SoC:** HR_C7000 — DW_apb_timers @ `0x14000000` (timebase measured at 42 MHz),
  an external PIC @ `0x17000000` (peripheral IRQs autovector through the CK803S
  VBR at `32 + source`), DW_apb_gpio banks, a 16550-style UART0 @ `0x14030000`,
  and the SOCSYS reset/clock controller.
- **Board:** the Dahua in-app-programmer (IAP) bootloader loads and jumps to the
  firmware flash slot at `0x0300d000` (length 614400). LEDs on PTB0/PTB1, a power
  self-latch on PTB13.

## Building

- **Toolchain:** `csky-miosix-elf` GCC (`-mcpu=ck803 -EL`, soft-float). Selected
  by `cmake/Toolchains/gcc-csky.cmake`. The C-SKY multilib emits legacy `.ctors`
  rather than `.init_array`; `tools/kernel_global_objects.pl` handles this.
- **Unikernel:** the kernel and the (kernelspace) application link into one image;
  there is no process pool. `unikernel.ld` is the board's default/reference linker
  script. An application with special memory needs supplies its **own** linker
  script out of tree via `MIOSIX_LINKER_SCRIPT` (absolute path accepted) rather
  than editing the kernel.
- **Flashing** is board/vendor-specific (the IAP's YMODEM/DFU loader); not part of
  the kernel.

## What works

- Tickless scheduler (`OS_TIMER_MODEL_UNIFIED`), threads, sync, timers, delays.
- **Fault handler** — any CK803S CPU exception is decoded (`PSR.VEC`) and reported
  (cause name + EPC/EPSR + register dump) over the debug UART, then halts. Makes
  otherwise-invisible faults diagnosable.
- **Console** — `stdout`/`printf`/`iprintf` and the kernel boot-log route to UART0
  (57600 8N1) via a polled console `Device`.
- **Idle power** — `sleepCpu()` issues the C-SKY `wait` instruction, stopping the
  CPU clock during idle (peripherals keep running; wakes on the timer/PIC IRQ).
- `shutdown()`/`reboot()` (power self-latch drop + SOCSYS soft-reset).

## Deliberate limitations & hardware notes

These are intrinsic to the HR_C7000/CK803S as integrated on this SoC, not TODOs:

- **Unikernel only — no processes / userspace / MPU.** The HR_C7000 has no MMU,
  so `WITH_PROCESSES` is not supported; `mpu_impl.h`/`userspace_impl.h` are
  intentionally no-ops (as on `arch/cpu/armv4`).
- **No filesystem** is wired (`WITH_FILESYSTEM` off); `bspInit2()` is empty.
- **No hardware PendSV / software-triggerable interrupt.** The CK803S core's
  tightly-coupled VIC (`0xE000E000`, with `ISPR`/Tspend) is **not wired** on this
  SoC — verified on silicon (a Tspend probe never fired); the external PIC
  delivers all interrupts instead. Consequently a context switch requested under
  the global lock cannot be taken by a hardware exception at lock release, so it
  is drained in `kernel/lock.h` (`FastGlobalIrqLock::unlock`) and `kernel/thread.cpp`.
  Both drains are guarded and compile to a no-op on architectures that *do* have
  PendSV. A WAKE-timer substitute was tried and reliably hangs at boot; do not
  re-attempt it. A clean upstream alternative would be a generic weak
  `IRQtakeDeferredSwitch()` hook the kernel calls at lock release.
- **No deep sleep** (`WITH_DEEP_SLEEP` unsupported). Deep sleep needs a wake
  source that survives a peripheral-clock stop; none is available here — CoreTim
  routes through the unwired VIC, the DW timers stop under `doze`/`stop`, and the
  RTC is second-granularity and I²C-accessed. Idle `wait` (above) is the power
  optimization this SoC supports.
- **Hand-rolled os-timer** (not the `TimerAdapter` CRTP): the DW_apb_timers have
  neither a match register nor a software-pend IRQ, so the os-timer interface is
  implemented directly with a free-running + one-shot two-channel scheme.
- **GPIO** exposes input/output only; pulls/alternate-function live in the SOCSYS
  IO manager and are not modelled here.

## Memory map (`unikernel.ld`)

- **flash:** `0x0300d000`, length 614400 — the IAP firmware slot; VMA == LMA.
- **ram:** `0x20000`..`0x4f000` (188 KiB). The low `0x10000`..`0x20000` is left to
  the IAP/BOOTROM. The top 4 KiB (`0x4f000`..`0x50000`) is excluded (the IAP
  scribbles there each boot).

## References

Hardware/architecture facts were taken from the CK803S core user guide (VIC,
system timer), the HR_C7000 SoC manual, and the C-SKY V2 ABI. The exception model
and idioms were cross-checked against the Linux `arch/csky` kernel port and the
RT-Thread CK803 port (both public).
