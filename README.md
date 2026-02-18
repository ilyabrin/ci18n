# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Лёгкая библиотека интернационализации (i18n) для проектов на чистом C.

**In English here:** [English](README.en.md)

## Возможности

- ✅ Один заголовочный файл
- ✅ Чистый C (C89/C99/C11)
- ✅ Нет внешних зависимостей
- ✅ Поддержка нескольких языков
- ✅ Загрузка из файлов и буфера
- ✅ Fallback язык
- ✅ Потокобезопасный режим (опционально)
- ✅ UTF-8 совместимый

## Быстрый старт

### 1. Подключение

В **одном** .c файле вашего проекта:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

В остальных файлах:

```c
#include "ci18n.h"
```

### 2. Инициализация

```c
ci18n_init();
ci18n_load_language("en", "translations/en.txt");
ci18n_load_language("ru", "translations/ru.txt");
ci18n_set_current("ru");
ci18n_set_fallback("en");
```

### 3. Использование

```c
printf("%s\n", ci18n_get("welcome_message"));
// или с fallback на ключ:
printf("%s\n", ci18n_get_or_key("missing_key"));
```

### 4. Очистка

```c
ci18n_free();
```

## Формат файлов переводов

```ini
# Это комментарий
greeting=Привет!
farewell=До свидания!
error=Произошла ошибка
```

## Конфигурация

Определите макросы перед включением заголовка для настройки:

```c
#define CI18N_MAX_KEY_LENGTH 256
#define CI18N_MAX_VALUE_LENGTH 4096
#define CI18N_MAX_LANGUAGES 32
#define CI18N_MAX_KEYS_PER_LANGUAGE 1024
#define CI18N_THREAD_SAFE  /* для потокобезопасности */
#include "ci18n.h"
```

## API

| Функция                                  | Описание                 |
| ---------------------------------------- | ------------------------ |
| `ci18n_init()`                           | Инициализация системы    |
| `ci18n_free()`                           | Освобождение ресурсов    |
| `ci18n_load_language(code, path)`        | Загрузка из файла        |
| `ci18n_load_from_buffer(code, buf, len)` | Загрузка из буфера       |
| `ci18n_set_current(code)`                | Установить текущий язык  |
| `ci18n_set_fallback(code)`               | Установить fallback язык |
| `ci18n_get(key)`                         | Получить перевод         |
| `ci18n_get_or_key(key)`                  | Перевод или ключ         |
| `ci18n_has(key)`                         | Проверка наличия ключа   |
| `ci18n_set(lang, key, value)`            | Добавить перевод         |
| `ci18n_remove(lang, key)`                | Удалить перевод          |
| `ci18n_clear(lang)`                      | Очистить язык            |
| `ci18n_count(lang)`                      | Количество записей       |
| `ci18n_get_languages(&count)`            | Список языков            |

## Компиляция примера

```bash
gcc -o example examples/example.c -I./include
./example
```

## Лицензия

MIT
