cmake_minimum_required(VERSION 3.16)
project(darcnes_sdl2 C)

set(CMAKE_C_STANDARD 99)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Try multiple ways to find SDL2
set(SDL2_FOUND FALSE)
find_package(SDL2 CONFIG QUIET)
if (SDL2_FOUND)
  message(STATUS "Found SDL2 via package config")
else()
  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
    if (SDL2_FOUND)
      message(STATUS "Found SDL2 via pkg-config")
    endif()
  endif()
  if (NOT SDL2_FOUND)
    find_program(SDL2_CONFIG_EXECUTABLE sdl2-config)
    if (SDL2_CONFIG_EXECUTABLE)
      execute_process(COMMAND ${SDL2_CONFIG_EXECUTABLE} --cflags OUTPUT_VARIABLE SDL2_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
      execute_process(COMMAND ${SDL2_CONFIG_EXECUTABLE} --libs OUTPUT_VARIABLE SDL2_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
      set(SDL2_FOUND TRUE)
    endif()
  endif()
endif()
if (NOT SDL2_FOUND)
  message(FATAL_ERROR "SDL2 not found. Install SDL2 dev files or set SDL2_DIR/CMAKE_PREFIX_PATH.")
endif()

# Collect all C sources
file(GLOB DN_SOURCES
  "*.c"
)

# Exclude non-SDL frontends and platform-specific Unix code
list(REMOVE_ITEM DN_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/ui_x.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui_allegro.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/video_allegro.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/video_svga.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/snd_unix.c"
)

# Add our SDL2 backends
list(APPEND DN_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/ui_sdl2.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/video_sdl2.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/snd_sdl2.c"
)

add_executable(darcnes ${DN_SOURCES})

if (TARGET SDL2::SDL2)
  target_link_libraries(darcnes PRIVATE SDL2::SDL2)
  if (TARGET SDL2::SDL2main)
    target_link_libraries(darcnes PRIVATE SDL2::SDL2main)
  endif()
  get_target_property(_sdl2_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
  if (_sdl2_inc)
    target_include_directories(darcnes PRIVATE ${_sdl2_inc})
  endif()
elseif (DEFINED SDL2_INCLUDE_DIRS OR DEFINED SDL2_INCLUDE_DIR)
  target_include_directories(darcnes PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
  if (DEFINED SDL2_LIBRARIES)
    target_link_libraries(darcnes PRIVATE ${SDL2_LIBRARIES})
  else()
    target_link_libraries(darcnes PRIVATE ${SDL2_LIBS})
  endif()
elseif (DEFINED SDL2_CFLAGS OR DEFINED SDL2_LIBS)
  separate_arguments(SDL2_CFLAGS)
  separate_arguments(SDL2_LIBS)
  target_compile_options(darcnes PRIVATE ${SDL2_CFLAGS})
  target_link_libraries(darcnes PRIVATE ${SDL2_LIBS})
endif()

if(WIN32)
  # Ensure subsystem console to see stdout
  set_target_properties(darcnes PROPERTIES LINK_FLAGS "")
endif()

