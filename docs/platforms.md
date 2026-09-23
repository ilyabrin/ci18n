# Platforms

Every push builds with warnings as errors and runs the unit tests on each of
these. "Build only" means there is nothing in CI to run the result on.

| Platform | Compilers | Tests run |
| --- | --- | --- |
| Linux x86-64, ARM64, 32-bit x86 | gcc, clang, gcc 9, clang 12 | yes, plus sanitizers and fuzzing |
| Linux on s390x (big-endian), ARMv7, RISC-V 64 | gcc, emulated | yes |
| Linux with musl (Alpine) | gcc | yes |
| macOS on Apple silicon and Intel | Apple clang | yes |
| Windows x64 and ARM64 | MSVC, clang-cl, MinGW gcc | yes |
| FreeBSD, OpenBSD, NetBSD | system cc | yes |
| iOS | Apple clang | yes, on the simulator; device build only |
| Android arm64, armv7, x86-64 | NDK clang | build only |
| WebAssembly | Emscripten | yes, under Node |
| Cortex-M0+, M3, M4, M7 | arm-none-eabi-gcc, newlib | yes on M3, under QEMU |
| RISC-V 32, the ESP32-C3 class | riscv gcc, picolibc | yes, under QEMU |

The C standard is C99, and CI also builds as C11 and C17. Every module that
can be left out is built and tested on its own and all together.

The smallest target is a 32-bit microcontroller with 64 KB of RAM. 8-bit AVR
boards such as the Arduino Uno are not supported; see
[Embedded and size](embedded-and-size.md#small-devices).

The jobs live in [.github/workflows/platforms.yml](../.github/workflows/platforms.yml)
and [.github/workflows/ci.yml](../.github/workflows/ci.yml).
