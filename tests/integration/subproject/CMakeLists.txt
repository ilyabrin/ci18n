# Consumes ci18n as a subproject, the code path FetchContent_MakeAvailable()
# takes once it has the sources.
cmake_minimum_required(VERSION 3.14)
project(ci18n_subproject_consumer LANGUAGES C)

add_subdirectory(../../.. ci18n_build)

add_executable(consumer ../consumer.c)
target_link_libraries(consumer PRIVATE ci18n::ci18n)

if(NOT TARGET ci18n::ci18n)
  message(FATAL_ERROR "ci18n::ci18n is missing")
endif()

# A subproject must not drag in the library's own tests or example.
foreach(unwanted test_ci18n test_thread_local ci18n_example)
  if(TARGET ${unwanted})
    message(FATAL_ERROR "ci18n added ${unwanted} as a subproject, it should not")
  endif()
endforeach()

enable_testing()
add_test(NAME consume_subproject COMMAND consumer)
