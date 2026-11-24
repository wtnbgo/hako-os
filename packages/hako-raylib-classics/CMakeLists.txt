cmake_minimum_required(VERSION 3.20)
project(hako-raylib-classics C)

# raylib from the submodule of hako-raylib-hello (one copy in the repository), statically,
# on its SDL platform (SDL3 found via find_package).
set(PLATFORM SDL CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../hako-raylib-hello/raylib raylib EXCLUDE_FROM_ALL)

file(GLOB HK_GAMES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/games/*.c)
add_executable(raylib-classics src/main.c src/hk_input.c ${HK_GAMES})
# the classic sources are old C; keep their warnings quiet
set_source_files_properties(${HK_GAMES} PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(raylib-classics PRIVATE raylib m)
if(TARGET SDL3::SDL3)
  target_link_libraries(raylib-classics PRIVATE SDL3::SDL3)
endif()

include(GNUInstallDirs)
install(TARGETS raylib-classics RUNTIME DESTINATION lib/hako/raylib-classics)
install(FILES licenses/raylib-classics.txt licenses/raylib.txt licenses/raylib-external.txt licenses/hako-raylib-classics.txt
        DESTINATION lib/hako/raylib-classics/licenses)
install(FILES hako-raylib-classics.app DESTINATION ${CMAKE_INSTALL_DATADIR}/hako/apps)
