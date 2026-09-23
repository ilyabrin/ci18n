# ci18n_compile_translations: build a translation file into a program.
#
#   ci18n_compile_translations(app ru translations/ru.txt)
#   ci18n_compile_translations(app keys translations/en.txt KEYS_ONLY)
#
# Generates <build>/ci18n_compiled/<name>.h from the file whenever the file
# changes, and puts that directory on the target's include path. The program
# then does:
#
#   #include "ru.h"
#   ci18n_use_compiled("ru", &ci18n_compiled_ru);
#
# KEYS_ONLY writes only the key macros, for CI18N_KEY() with translations
# loaded at run time. STRICT fails the build on a key that is not a C
# identifier, instead of listing it.
#
# The generator is built from tools/ci18n_compile.c the first time it is
# needed. When cross-compiling it has to run on the build machine, not the
# target, so set CI18N_COMPILE_EXECUTABLE to a copy built for the host.

set(CI18N_COMPILE_EXECUTABLE "" CACHE FILEPATH
    "A ci18n_compile built for the host, needed when cross-compiling")

function(ci18n_compile_translations target name file)
  cmake_parse_arguments(ARG "KEYS_ONLY;STRICT" "" "" ${ARGN})

  if(CI18N_COMPILE_EXECUTABLE)
    set(_tool "${CI18N_COMPILE_EXECUTABLE}")
  else()
    if(CMAKE_CROSSCOMPILING)
      message(FATAL_ERROR
        "ci18n: cross-compiling, so the generator cannot be built for this "
        "target and run here. Build tools/ci18n_compile.c for the host and "
        "set CI18N_COMPILE_EXECUTABLE to it.")
    endif()
    if(NOT TARGET ci18n_compile)
      add_executable(ci18n_compile "${CI18N_COMPILE_SOURCE}")
      target_link_libraries(ci18n_compile PRIVATE ci18n::ci18n)
    endif()
    set(_tool $<TARGET_FILE:ci18n_compile>)
  endif()

  set(_flags)
  if(ARG_KEYS_ONLY)
    list(APPEND _flags --keys-only)
  endif()
  if(ARG_STRICT)
    list(APPEND _flags --strict)
  endif()

  get_filename_component(_input "${file}" ABSOLUTE)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/ci18n_compiled")
  set(_output "${_dir}/${name}.h")

  add_custom_command(
    OUTPUT "${_output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_dir}"
    COMMAND ${_tool} -o "${_output}" ${_flags} ${name} "${_input}"
    DEPENDS "${_input}" ${_tool}
    COMMENT "ci18n: compiling ${file}"
    VERBATIM)

  target_sources(${target} PRIVATE "${_output}")
  target_include_directories(${target} PRIVATE "${_dir}")
endfunction()
