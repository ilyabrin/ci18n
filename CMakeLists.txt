# ci18n build. There is nothing to compile in the library itself, so the
# target is an INTERFACE one: it carries the include path and the C standard,
# and consumers get the implementation by defining CI18N_IMPLEMENTATION in one
# of their own source files.
#
#   cmake -B build && cmake --build build && ctest --test-dir build
#   cmake --install build --prefix /usr/local
#
# With a multi-config generator, Visual Studio being the common one, ctest
# needs to be told which configuration to run:
#
#   cmake --build build --config Release
#   ctest --test-dir build -C Release
#
# As a subproject, through FetchContent or add_subdirectory, the tests, the
# example and the install rules all switch themselves off.

cmake_minimum_required(VERSION 3.14)

# Single source of truth for the version: parse it out of the header rather
# than repeating it here, where it would drift.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/ci18n.h" _ci18n_header)
string(REGEX MATCH "CI18N_VERSION_STRING \"([0-9]+\\.[0-9]+\\.[0-9]+)\""
       _ci18n_version_match "${_ci18n_header}")

if(NOT _ci18n_version_match)
  message(FATAL_ERROR "cannot find CI18N_VERSION_STRING in include/ci18n.h")
endif()

set(CI18N_VERSION "${CMAKE_MATCH_1}")
unset(_ci18n_header)

project(ci18n
  VERSION ${CI18N_VERSION}
  DESCRIPTION "Single-header internationalization library for C"
  HOMEPAGE_URL "https://github.com/ilyabrin/ci18n"
  LANGUAGES C)

# Everything optional defaults to on only when ci18n is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(_ci18n_is_top_level ON)
else()
  set(_ci18n_is_top_level OFF)
endif()

option(CI18N_BUILD_TESTS "Build and register the test suite" ${_ci18n_is_top_level})
option(CI18N_BUILD_EXAMPLE "Build the example program" ${_ci18n_is_top_level})
option(CI18N_INSTALL "Generate install rules" ${_ci18n_is_top_level})
option(CI18N_BUILD_BENCH "Build the benchmark" ${_ci18n_is_top_level})

# ---------------------------------------------------------------------------
# The library
# ---------------------------------------------------------------------------

# Needed here, before the target: CMAKE_INSTALL_INCLUDEDIR goes into the
# target's INSTALL_INTERFACE below, and including GNUInstallDirs later would
# leave that empty, so find_package() would succeed and the consumer would
# still fail to find the header.
include(GNUInstallDirs)

add_library(ci18n INTERFACE)
add_library(ci18n::ci18n ALIAS ci18n)

target_include_directories(ci18n INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# Requires C99, does not force it on the consumer's other targets.
target_compile_features(ci18n INTERFACE c_std_99)

# ci18n_compile_translations(), for compiled catalogues. Available to a
# project that adds ci18n as a subproject, and installed for find_package.
set(CI18N_COMPILE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci18n_compile.c")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ci18nCompile.cmake")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(CI18N_BUILD_TESTS)
  enable_testing()

  add_executable(test_ci18n tests/test_ci18n.c)
  target_link_libraries(test_ci18n PRIVATE ci18n::ci18n)

  # The file tests write their fixtures into the working directory, and the
  # example reads translations/ by relative path, so both run from the source
  # tree rather than the build directory.
  add_test(NAME unit COMMAND test_ci18n WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  # Compiled catalogues against the same files loaded at run time. Needs the
  # generator to run here, so not when cross-compiling without a host copy.
  if(NOT CMAKE_CROSSCOMPILING OR CI18N_COMPILE_EXECUTABLE)
    add_executable(test_compiled tests/test_compiled.c)
    target_link_libraries(test_compiled PRIVATE ci18n::ci18n)
    ci18n_compile_translations(test_compiled stress tests/compiled/stress.txt)
    ci18n_compile_translations(test_compiled tr_en translations/en.txt)
    ci18n_compile_translations(test_compiled tr_ru translations/ru.txt)
    ci18n_compile_translations(test_compiled tr_es translations/es.txt)
    ci18n_compile_translations(test_compiled sync_ru examples/cli_sync/locales/ru.txt)
    add_test(NAME compiled COMMAND test_compiled WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  # Thread-local context tests. The gate is pthreads specifically, not
  # Threads_FOUND: with MSVC that reports success and hands you Win32 threads,
  # and this test includes pthread.h.
  find_package(Threads)
  if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    add_executable(test_thread_local tests/test_thread_local.c)
    target_link_libraries(test_thread_local PRIVATE ci18n::ci18n Threads::Threads)
    add_test(NAME threads COMMAND test_thread_local)
  else()
    message(STATUS "ci18n: no pthreads, skipping the thread-local context tests")
  endif()
endif()

# ---------------------------------------------------------------------------
# Example
# ---------------------------------------------------------------------------

if(CI18N_BUILD_EXAMPLE)
  add_executable(ci18n_example examples/example.c)
  target_link_libraries(ci18n_example PRIVATE ci18n::ci18n)
  set_target_properties(ci18n_example PROPERTIES OUTPUT_NAME example)

  # The three larger examples, each checked against the output it is known
  # to give. /utf-8 because their strings are Cyrillic and Arabic, and MSVC
  # otherwise reads the source in the system code page.
  add_executable(ci18n_example_cli_sync examples/cli_sync/i18n_check.c)
  add_executable(ci18n_example_ui examples/embedded_ui/ui.c)
  foreach(_lang en ru ar)
    ci18n_compile_translations(ci18n_example_ui ${_lang}
      examples/embedded_ui/locales/${_lang}.txt)
  endforeach()
  set(_ci18n_examples ci18n_example_cli_sync ci18n_example_ui)

  # The server includes pthread.h, so the same gate as the thread tests.
  find_package(Threads)
  if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    add_executable(ci18n_example_server examples/server/server.c)
    target_link_libraries(ci18n_example_server PRIVATE Threads::Threads)
    list(APPEND _ci18n_examples ci18n_example_server)
  endif()

  foreach(_example IN LISTS _ci18n_examples)
    target_link_libraries(${_example} PRIVATE ci18n::ci18n)
    target_compile_options(${_example} PRIVATE $<$<C_COMPILER_ID:MSVC>:/utf-8>)
  endforeach()

  if(CI18N_BUILD_TESTS)
    set(_check "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_example.cmake")
    set(_locales "examples/cli_sync/locales")

    # Exits 1 on purpose: one of the sample files is broken.
    add_test(NAME example_cli_sync
      COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:ci18n_example_cli_sync>
        "-DARGS=${_locales}/en.txt|${_locales}/ru.txt|${_locales}/de.txt"
        -DEXIT_CODE=1
        -DEXPECTED=examples/cli_sync/expected.txt
        -P "${_check}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(NAME example_ui
      COMMAND ${CMAKE_COMMAND}
        -DPROGRAM=$<TARGET_FILE:ci18n_example_ui>
        -DEXIT_CODE=0
        -DEXPECTED=examples/embedded_ui/expected.txt
        -P "${_check}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    if(TARGET ci18n_example_server)
      add_test(NAME example_server
        COMMAND ${CMAKE_COMMAND}
          -DPROGRAM=$<TARGET_FILE:ci18n_example_server>
          -DEXIT_CODE=0
          -DEXPECTED=examples/server/expected.txt
          -P "${_check}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
  endif()
endif()

# ---------------------------------------------------------------------------
# Benchmark
# ---------------------------------------------------------------------------

# Built, not run: timings from a shared CI machine mean nothing, but building
# it everywhere keeps it compiling everywhere. Run it yourself from a Release
# build. The thread and gettext benchmarks need pthreads and glibc, so they
# live in the Makefile only.
if(CI18N_BUILD_BENCH)
  add_executable(ci18n_bench bench/bench.c)
  target_link_libraries(ci18n_bench PRIVATE ci18n::ci18n)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

if(CI18N_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(TARGETS ci18n EXPORT ci18nTargets)
  install(FILES include/ci18n.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

  # The generator ships as source, built on first use by the consumer's own
  # compiler, so one install serves any toolchain.
  install(FILES tools/ci18n_compile.c DESTINATION "${CMAKE_INSTALL_DATADIR}/ci18n")
  install(FILES cmake/ci18nCompile.cmake DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ci18n")

  install(EXPORT ci18nTargets
    FILE ci18nTargets.cmake
    NAMESPACE ci18n::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ci18n")

  # A header-only library works with any compiler, so the version file is
  # compatible with anything from the same major version.
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ci18nConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)

  configure_package_config_file(
    cmake/ci18nConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ci18nConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ci18n")

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ci18nConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ci18nConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ci18n")

  # The same pkg-config template the Makefile installs, so both routes give a
  # consumer the same file.
  #
  # Generated at install time rather than configure time on purpose:
  # `cmake --install --prefix` overrides the prefix afterwards, and a
  # pkg-config file with a stale prefix baked in is worse than no file at all.
  # DESTDIR has to be honoured by hand here, since this writes the file
  # itself instead of going through install(FILES).
  install(CODE "
set(PREFIX \"\${CMAKE_INSTALL_PREFIX}\")
set(INCLUDEDIR \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}\")
set(VERSION \"${PROJECT_VERSION}\")
set(_pc \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/ci18n.pc\")
configure_file(\"${CMAKE_CURRENT_SOURCE_DIR}/ci18n.pc.in\" \"\${_pc}\" @ONLY)
message(STATUS \"Installing: \${_pc}\")
")
endif()
