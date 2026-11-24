cmake_minimum_required(VERSION 3.21)

# Siv3D game collection for hako. Built with the siv3d_app tooling of the SDL3 port of
# OpenSiv3D (see docs chapter 2, "Siv3D"):
#   python3 <OpenSiv3D>/tools/siv3d_app.py --project packages/hako-siv3d-games --target rpmini all
#
# OpenSiv3D (SDL3) location (SIV3D_BASE = root of the OpenSiv3D repository):
#   1. -DSIV3D_BASE=<path>   2. environment variable SIV3D_BASE   3. ../OpenSiv3D next to this folder
# If none exists, cmake/FetchSiv3D.cmake clones the version named in siv3d-app.json.
if(NOT SIV3D_BASE)
  if(DEFINED ENV{SIV3D_BASE})
    set(SIV3D_BASE "$ENV{SIV3D_BASE}")
  else()
    get_filename_component(SIV3D_BASE "${CMAKE_CURRENT_LIST_DIR}/../OpenSiv3D" ABSOLUTE)
  endif()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/cmake/FetchSiv3D.cmake")

include("${SIV3D_BASE}/cmake/Siv3DAppPrelude.cmake")

project(siv3d-games LANGUAGES C CXX)

include("${SIV3D_BASE}/cmake/Siv3DApp.cmake")

file(GLOB HK_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/games/*.cpp")

siv3d_add_app(siv3d-games
  SOURCES
    src/Main.cpp
    src/Input.cpp
    src/Games.cpp
    ${HK_GAME_SOURCES}
  CONFIG siv3d-app.json
)
