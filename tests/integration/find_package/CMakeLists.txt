# Consumes an installed ci18n. Run against an install prefix:
#
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
cmake_minimum_required(VERSION 3.14)
project(ci18n_find_package_consumer LANGUAGES C)

find_package(ci18n 1.0 REQUIRED)

add_executable(consumer ../consumer.c)
target_link_libraries(consumer PRIVATE ci18n::ci18n)

enable_testing()
add_test(NAME consume_installed COMMAND consumer)
