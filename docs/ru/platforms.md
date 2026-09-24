# Платформы

Каждый пуш собирается с предупреждениями как ошибками и прогоняет юнит-тесты
на каждой из этих платформ. «Только сборка» значит, что в CI результат
запустить не на чем.

| Платформа | Компиляторы | Тесты запускаются |
| --- | --- | --- |
| Linux x86-64, ARM64, 32-битный x86 | gcc, clang, gcc 9, clang 12 | да, плюс санитайзеры и фаззинг |
| Linux на s390x (big-endian), ARMv7, RISC-V 64 | gcc, в эмуляторе | да |
| Linux с musl (Alpine) | gcc | да |
| macOS на Apple silicon и Intel | Apple clang | да |
| Windows x64 и ARM64 | MSVC, clang-cl, MinGW gcc | да |
| FreeBSD, OpenBSD, NetBSD | системный cc | да |
| iOS | Apple clang | да, в симуляторе; для устройства только сборка |
| Android arm64, armv7, x86-64 | clang из NDK | только сборка |
| WebAssembly | Emscripten | да, под Node |
| Cortex-M0+, M3, M4, M7 | arm-none-eabi-gcc, newlib | да на M3, под QEMU |
| RISC-V 32, класс ESP32-C3 | riscv gcc, picolibc | да, под QEMU |
| AVR: ATmega2560, ATmega328P из Arduino Uno | avr-gcc, avr-libc; Arduino IDE, PlatformIO | да, под QEMU |

Стандарт C это C99, и CI собирает ещё как C11 и C17. Каждый модуль, который
можно выключить, собирается и тестируется отдельно и все вместе.

Самая маленькая цель это Arduino Uno с 2 КБ RAM; см.
[Arduino и AVR](embedded-and-size.md#arduino-и-avr).

Задачи лежат в [.github/workflows/platforms.yml](../../.github/workflows/platforms.yml)
и [.github/workflows/ci.yml](../../.github/workflows/ci.yml).
