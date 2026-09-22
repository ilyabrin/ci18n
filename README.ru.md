# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Лёгкая библиотека интернационализации (i18n) для проектов на чистом C.

**Документация на других языках:** [English](README.md)

## Возможности

- ✅ Один заголовочный файл
- ✅ Чистый C, стандарт C99 и новее
- ✅ Нет внешних зависимостей
- ✅ Поддержка нескольких языков
- ✅ Загрузка из файлов и буфера
- ✅ Fallback язык
- ✅ Контекст на поток (опционально)
- ✅ UTF-8 совместимый, BOM в файлах перевода пропускается

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

Чтобы получить список загруженного, передайте свой буфер. Возвращается общее
число языков, и оно может превышать вашу ёмкость:

```c
const char *codes[CI18N_MAX_LANGUAGES];
size_t total = ci18n_get_languages(codes, CI18N_MAX_LANGUAGES);

for (size_t i = 0; i < total; i++) {
    printf("  - %s\n", codes[i]);
}
```

Передайте `NULL`, чтобы узнать только количество: `ci18n_get_languages(NULL, 0)`.

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
#define CI18N_THREAD_LOCAL_CONTEXT  /* по контексту на поток */
#include "ci18n.h"
```

### Компоновка

`CI18N_DEF` применяется к каждой публичной функции. Переопределите его, чтобы
изменить способ компоновки:

```c
#define CI18N_DEF static                  /* API виден только в одном файле */
#define CI18N_DEF __declspec(dllexport)   /* экспорт из DLL */
#define CI18N_DEF __declspec(dllimport)   /* импорт из DLL */
```

С `static` ожидайте `-Wunused-function` для функций, которые вы не вызываете.

### Потоки

`CI18N_THREAD_LOCAL_CONTEXT` даёт каждому потоку свой контекст. Понимать это нужно
буквально: это изоляция, а не общая потокобезопасность. Каждый поток стартует
пустым и сам вызывает `ci18n_init()` и загрузку, а язык, загруженный в одном
потоке, не виден в остальных. Это подходит воркеру, который рендерит в локали
одного пользователя.

Чего макрос не даёт, так это сценария «загрузил один раз, читаю из N
потоков». Без макроса контекст один глобальный и без блокировок, так что
одновременные `ci18n_set()` или `ci18n_load_*()` с `ci18n_get()` это гонка
данных. Если потоки только читают, а вся загрузка закончилась до их старта,
обычный глобал уже безопасен.

`CI18N_THREAD_SAFE` это старое имя того же макроса. Оно продолжает работать и
значит то же самое, но выводит предупреждение об устаревании.

### Время жизни указателей

Любой `const char *`, который возвращает API, указывает во внутреннее
хранилище, поэтому он валиден только до следующего вызова, который меняет
этот язык:

```c
const char *greeting = ci18n_get("greeting");
ci18n_load_language("en", "extra.txt");   /* может сделать realloc */
puts(greeting);                           /* висячий указатель */
```

`ci18n_set()`, `ci18n_remove()` и функции загрузки могут переаллоцировать
массив записей. `ci18n_clear()`, `ci18n_free()` и `ci18n_set_current()`
инвалидируют указатели сразу. Читайте перевод непосредственно перед
использованием, это дешёво, либо копируйте, если нужно сохранить.

### Проверка версии

```c
#if CI18N_VERSION < CI18N_VERSION_NUMBER(1, 1, 0)
#error "ci18n 1.1.0 or newer is required"
#endif

printf("ci18n %s\n", CI18N_VERSION_STRING);
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
| `ci18n_get_languages(out, cap)`          | Список языков            |

## Сборка

```bash
make            # собрать пример, затем собрать и прогнать тесты
make test       # только тесты
make clean
```

Или без make:

```bash
gcc -Wall -Wextra -std=c99 -I./include -o example examples/example.c
./example
```

Запускать из корня репозитория, иначе не разрешатся относительные пути
к `translations/`.

## Участие в разработке

Баг-репорты и пулл-реквесты приветствуются. Как собрать, протестировать и
оформить изменение, описано в [CONTRIBUTING.md](CONTRIBUTING.md), а про
сообщения об уязвимостях в [SECURITY.md](SECURITY.md).

Канонический README английский, [README.md](README.md). Этот перевод
обновляется по мере сил, так что при расхождении верен английский.

## Лицензия

MIT, см. [LICENSE](LICENSE).
