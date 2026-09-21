# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Lightweight internationalization (i18n) library for pure C projects.

**Other languages:** [Русский](README.ru.md)

## Features

- ✅ Single header file
- ✅ Pure C, C99 and newer
- ✅ No external dependencies
- ✅ Multiple language support
- ✅ Load from files and buffers
- ✅ Fallback language
- ✅ Thread-safe mode (optional)
- ✅ UTF-8 compatible, skips a BOM in translation files

## Quick Start

### 1. Include

In **one** .c file of your project:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

In other files:

```c
#include "ci18n.h"
```

### 2. Initialize

```c
ci18n_init();
ci18n_load_language("en", "translations/en.txt");
ci18n_load_language("ru", "translations/ru.txt");
ci18n_set_current("ru");
ci18n_set_fallback("en");
```

### 3. Usage

```c
printf("%s\n", ci18n_get("welcome_message"));
// or with fallback to key:
printf("%s\n", ci18n_get_or_key("missing_key"));
```

### 4. Cleanup

```c
ci18n_free();
```

## Translation File Format

```ini
# This is a comment
greeting=Hello!
farewell=Goodbye!
error=An error occurred
```

## Configuration

Define macros before including the header to configure:

```c
#define CI18N_MAX_KEY_LENGTH 256
#define CI18N_MAX_VALUE_LENGTH 4096
#define CI18N_MAX_LANGUAGES 32
#define CI18N_MAX_KEYS_PER_LANGUAGE 1024
#define CI18N_THREAD_SAFE  /* for thread safety */
#include "ci18n.h"
```

### Linkage

`CI18N_DEF` decorates every public function. Override it to change how the
library is linked:

```c
#define CI18N_DEF static                  /* keep the API private to one file */
#define CI18N_DEF __declspec(dllexport)   /* export from a Windows DLL */
#define CI18N_DEF __declspec(dllimport)   /* consume that DLL */
```

With `static`, expect `-Wunused-function` for any API you do not call.

### Version check

```c
#if CI18N_VERSION < CI18N_VERSION_NUMBER(1, 1, 0)
#error "ci18n 1.1.0 or newer is required"
#endif

printf("ci18n %s\n", CI18N_VERSION_STRING);
```

## API

| Function                                 | Description           |
| ---------------------------------------- | --------------------- |
| `ci18n_init()`                           | Initialize the system |
| `ci18n_free()`                           | Free resources        |
| `ci18n_load_language(code, path)`        | Load from file        |
| `ci18n_load_from_buffer(code, buf, len)` | Load from buffer      |
| `ci18n_set_current(code)`                | Set current language  |
| `ci18n_set_fallback(code)`               | Set fallback language |
| `ci18n_get(key)`                         | Get translation       |
| `ci18n_get_or_key(key)`                  | Translation or key    |
| `ci18n_has(key)`                         | Check if key exists   |
| `ci18n_set(lang, key, value)`            | Add translation       |
| `ci18n_remove(lang, key)`                | Remove translation    |
| `ci18n_clear(lang)`                      | Clear language        |
| `ci18n_count(lang)`                      | Entry count           |
| `ci18n_get_languages(&count)`            | List of languages     |

## Building

```bash
make            # build the example, then build and run the tests
make test       # tests only
make clean
```

Or without make:

```bash
gcc -Wall -Wextra -std=c99 -I./include -o example examples/example.c
./example
```

Run both from the repository root so the relative paths in
`translations/` resolve.

## Contributing

Bug reports and pull requests are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md) for how to build, test and submit a change,
and [SECURITY.md](SECURITY.md) for reporting a vulnerability.

This English README is the canonical one. The Russian translation is updated on
a best-effort basis, so when the two disagree, this file wins.

## License

MIT, see [LICENSE](LICENSE).
