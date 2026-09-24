# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Интернационализация для C в одном заголовке: переводы, плюралы для 71
языка, форматирование и поддержка письма справа налево, без зависимостей.

**Другие языки:** [English](README.md)

## За 30 секунд

`ru.txt`:

```ini
greeting=Привет, {name}!
files[one]={count} файл
files[few]={count} файла
files[many]={count} файлов
```

`main.c`:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

int main(void)
{
    char text[128];

    ci18n_init();
    ci18n_load_language("ru", "ru.txt");
    ci18n_set_current("ru");

    ci18n_format(text, sizeof(text), "greeting", "name", "Анна", NULL);
    puts(text);                                          /* Привет, Анна! */

    ci18n_format_plural(text, sizeof(text), "files", 3, NULL);
    puts(text);                                          /* 3 файла */

    ci18n_free();
}
```

```bash
cc -std=c99 -Iinclude main.c -o hello && ./hello
```

Дальше: [Начало работы](docs/ru/getting-started.md).

## Установка

Скопируйте [include/ci18n.h](include/ci18n.h) в свой проект. Больше ничего
не нужно: ни библиотеки для компоновки, ни шага сборки.

Или через CMake:

```cmake
include(FetchContent)
FetchContent_Declare(ci18n
  GIT_REPOSITORY https://github.com/ilyabrin/ci18n.git
  GIT_TAG v2.18.0)
FetchContent_MakeAvailable(ci18n)

target_link_libraries(your_target PRIVATE ci18n::ci18n)
```

Или установите в систему, тогда будут `find_package(ci18n)` и файл
pkg-config:

```bash
cmake -B build && cmake --build build && cmake --install build --prefix /usr/local
make install PREFIX=$HOME/.local           # the same, without CMake
cc $(pkg-config --cflags ci18n) -o app app.c
```

`ci18n::ci18n` это INTERFACE-цель: она несёт путь к заголовкам и требует
C99, а компоновать ничего не нужно. Как подпроект она не собирает ни тестов,
ни примеров и не добавляет правил установки. `make install DESTDIR=...`
готовит сборку пакета, а `make uninstall` всё откатывает.

Для Arduino и PlatformIO это обычная библиотека; см.
[Arduino и AVR](docs/ru/embedded-and-size.md#arduino-и-avr).

## Что умеет

| | |
| --- | --- |
| **Переводы** | Файлы `key=value` или буферы, слияние по языкам, запасной язык для того, чего нет. [Переводы](docs/ru/translations.md) |
| **Плюралы и порядковые числительные** | 1 file, 3 файла, 22nd: правила CLDR для 71 языка, сверенные с CLDR тестами. [Плюралы](docs/ru/translations.md#плюралы) |
| **Форматирование** | Именованные плейсхолдеры, ваши форматтеры для дат и числа в стиле каждого языка. [Форматирование](docs/ru/formatting.md) |
| **Справа налево** | Направление текста для каждого языка и изоляция bidi для смешанных предложений. [Unicode и направление текста](docs/ru/unicode-and-direction.md) |
| **UTF-8** | Строгая проверка, подсчёт символов, обрезка, которая не делит символ. [UTF-8 хелперы](docs/ru/unicode-and-direction.md#utf-8-хелперы) |
| **gettext** | Загрузка `.mo` напрямую или однократная конвертация `.po`. [Переход с gettext](docs/ru/from-gettext.md) |
| **Библиотеки и потоки** | Независимые каталоги, режим отдельного контекста на поток и shared-режим, где чтение масштабируется по ядрам. [Каталоги и потоки](docs/ru/catalogs-and-threads.md) |
| **Встроенные переводы** | Переводы как константные данные: без кучи, без загрузки, а `CI18N_KEY(greeting)` делает опечатку в ключе ошибкой компиляции. [Скомпилированные каталоги](docs/ru/compiled-catalogs.md) |
| **Маленькая** | 28 КБ кода при `-Os` со всем включённым, 15 КБ с `CI18N_MINIMAL`, 7,6 КБ на Cortex-M4, и она работает на Arduino Uno. [Встраиваемые системы и размер](docs/ru/embedded-and-size.md) |
| **Проверенная** | Предупреждения как ошибки, санитайзеры и фаззинг, на Linux, macOS, Windows, BSD, iOS, Android, WebAssembly, голом железе ARM и RISC-V и 8-битных AVR. [Платформы](docs/ru/platforms.md) |

Поиск занимает около 28 нс, а `gettext` из glibc около 140 нс на той же
машине. [Производительность](docs/ru/performance.md)

## Чего не делает

Нет сортировки, часовых поясов, дат, валют, нормализации и разбиения на
слова: для них нужны данные ICU, а для дат и валют ci18n вместо этого вызовет
вашу функцию. [Границы](docs/ru/scope.md) объясняют, где проходит линия и
когда выбирать gettext или ICU.

## Примеры

Три программы из тех, что с этой библиотекой пишут на самом деле, у каждой
короткий README. `make examples` собирает все три и сверяет их вывод.

| Пример | Что это | Что показывает |
| --- | --- | --- |
| [cli_sync](examples/cli_sync/) | Проверка для CI: пропавшие ключи, формы плюрала, плейсхолдеры | Каталоги, `ci18n_foreach`, статистика загрузки, запасной язык |
| [server](examples/server/) | Потоки-обработчики отвечают на языке, который просит каждый запрос | Shared-режим, `Accept-Language`, форматтеры, перезагрузка под читателями |
| [embedded_ui](examples/embedded_ui/) | Дисплей 20x4 на английском, русском и арабском | Малые лимиты, пакеты извне, подгонка UTF-8, письмо справа налево |

[examples/example.c](examples/example.c) это короткий обзор основ.

## Документация

Вся она в [docs/ru/](docs/ru/README.md): страница начала работы, по странице
на каждую возможность, [справочник API](docs/ru/api.md) и цифры. Каждая
функция описана ещё и рядом со своим объявлением в заголовке.

## Сборка и тесты

```bash
make              # build the example, then build and run the tests
make test         # the unit tests only
make examples     # build the three examples and check their output
make test-threads test-shared   # the threading modes, needs pthreads
make bench        # timings, see bench/README.md
make clean
```

Запускайте из корня репозитория, чтобы нашлись относительные пути в
`translations/`. В [CONTRIBUTING.md](CONTRIBUTING.md) перечислены все цели,
включая фаззер и проверки gettext.

## Участие в разработке

Сообщения об ошибках и пулл-реквесты приветствуются. Как собрать, проверить
и отправить изменение, описано в [CONTRIBUTING.md](CONTRIBUTING.md), как
сообщить об уязвимости, в [SECURITY.md](SECURITY.md), а как принято себя
вести, в [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). История релизов в
[CHANGELOG.md](CHANGELOG.md).

Русская документация, [README.ru.md](README.ru.md) и
[docs/ru/](docs/ru/README.md), это перевод английской, и CI проверяет, что
они совпадают во всём, что можно сверить автоматически: разделы, примеры
кода, таблицы, ссылки и числа. Если текст всё же разойдётся, верна
английская версия.

## Лицензия

MIT, см. [LICENSE](LICENSE).
