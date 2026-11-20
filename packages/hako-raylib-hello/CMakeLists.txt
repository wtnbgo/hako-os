cmake_minimum_required(VERSION 3.16)
project(hako-raylib-hello C)

# raylib from the submodule, statically, on its SDL platform (SDL3 found via
# find_package). Audio stays raylib's own miniaudio (ALSA -> PipeWire).
set(PLATFORM SDL CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(raylib EXCLUDE_FROM_ALL)

add_executable(hako-raylib-hello src/main.c)
target_link_libraries(hako-raylib-hello PRIVATE raylib m)

include(GNUInstallDirs)
install(TARGETS hako-raylib-hello RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES hako-raylib-hello.app DESTINATION ${CMAKE_INSTALL_DATADIR}/hako/apps)
