# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Lightweight internationalization (i18n) library for pure C projects.

**Other languages:** [Русский](README.md)

## Features

- ✅ Single header file
- ✅ Pure C (C89/C99/C11)
- ✅ No external dependencies
- ✅ Multiple language support
- ✅ Load from files and buffers
- ✅ Fallback language
- ✅ Thread-safe mode (optional)
- ✅ UTF-8 compatible

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

## Compiling the Example

```bash
gcc -o example examples/example.c -I./include
./example
```

## License

MIT
