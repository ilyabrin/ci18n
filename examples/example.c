/*
 * Пример использования ci18n.h
 * Компиляция: gcc -o example example.c -I../include
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include <stdio.h>

int main(void) {
    /* Инициализация */
    if (!ci18n_init()) {
        fprintf(stderr, "Failed to initialize ci18n\n");
        return 1;
    }

    /* Загрузка переводов из файлов */
    ci18n_load_language("en", "translations/en.txt");
    ci18n_load_language("ru", "translations/ru.txt");
    ci18n_load_language("es", "translations/es.txt");

    /* Или загрузка из буфера */
    ci18n_load_from_buffer("fr",
        "greeting=Bonjour\n"
        "farewell=Au revoir\n"
        "welcome=Bienvenue",
        60);

    /* Установка текущего языка */
    ci18n_set_current("en");
    ci18n_set_fallback("en");  /* Язык по умолчанию */

    /* Получение переводов */
    printf("=== English ===\n");
    printf("%s\n", ci18n_get("greeting"));
    printf("%s\n", ci18n_get("farewell"));

    /* Переключение языка */
    ci18n_set_current("ru");
    printf("\n=== Russian ===\n");
    printf("%s\n", ci18n_get("greeting"));
    printf("%s\n", ci18n_get("farewell"));

    /* Использование ci18n_get_or_key (вернёт ключ если перевод не найден) */
    printf("\n=== Missing key ===\n");
    printf("%s\n", ci18n_get_or_key("nonexistent_key"));

    /* Проверка наличия ключа */
    printf("\n=== Has key ===\n");
    printf("Has 'greeting': %s\n", ci18n_has("greeting") ? "yes" : "no");
    printf("Has 'missing': %s\n", ci18n_has("missing") ? "yes" : "no");

    /* Программное добавление перевода */
    ci18n_set("en", "dynamic_key", "This was added at runtime!");
    printf("\n=== Dynamic ===\n");
    printf("%s\n", ci18n_get("dynamic_key"));

    /* Список доступных языков */
    size_t count;
    const char** langs = ci18n_get_languages(&count);
    printf("\n=== Available languages (%u) ===\n", (unsigned int)count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", langs[i]);
    }

    /* Очистка */
    ci18n_free();

    return 0;
}
