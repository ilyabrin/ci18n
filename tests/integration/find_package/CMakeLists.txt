# Consumes an installed ci18n. Run against an install prefix:
#
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
cmake_minimum_required(VERSION 3.14)
project(ci18n_find_package_consumer LANGUAGES C)

find_package(ci18n 2.0 REQUIRED)

add_executable(consumer ../consumer.c)
target_link_libraries(consumer PRIVATE ci18n::ci18n)

enable_testing()
add_test(NAME consume_installed COMMAND consumer)

# A catalogue compiled at build time, through the installed CMake module.
add_executable(compiled_consumer ../compiled_consumer.c)
target_link_libraries(compiled_consumer PRIVATE ci18n::ci18n)
ci18n_compile_translations(compiled_consumer hello ../hello.txt)
add_test(NAME consume_compiled COMMAND compiled_consumer)
