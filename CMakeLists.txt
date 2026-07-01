cmake_minimum_required(VERSION 3.15)

# Use dynamic runtime library (/MD for Release, /MDd for Debug) to match raylib
if(MSVC OR MSVC_IDE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

project("Little Wars" VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# Compiler flags (your original settings)
# ============================================================================
if(MSVC)
    add_compile_options(/wd4244 /wd4305)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)
    add_compile_options($<$<CONFIG:Debug>:/ZI>)
    add_link_options($<$<CONFIG:Debug>:/INCREMENTAL /DEBUG:FASTLINK>)
    add_compile_options($<$<CONFIG:Release>:/Ox /GL>)
    add_link_options($<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
    add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
    # RelWithDebInfo settings...
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /GL /DNDEBUG /Oy-" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob2 /GL /DNDEBUG /Oy-" CACHE STRING "" FORCE)
    add_link_options($<$<CONFIG:RelWithDebInfo>:/DEBUG /INCREMENTAL:NO /LTCG /OPT:REF /OPT:ICF>)
    add_compile_definitions($<$<CONFIG:RelWithDebInfo>:RELEASE_MODE>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-conversion)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O3 -flto)
        add_link_options(-flto -s)
    endif()
    add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
endif()

# ============================================================================
# Geist Framework (Local)
# ============================================================================
set(GEIST_DIR ${CMAKE_SOURCE_DIR}/Geist)

# Raylib
set(RAYLIB_INCLUDE_DIR ${GEIST_DIR}/ThirdParty/raylib/include)
set(RAYLIB_EXTERNAL_INCLUDE ${GEIST_DIR}/ThirdParty/raylib/external/include)

if(WIN32)
    set(RAYLIB_LIBRARY ${GEIST_DIR}/ThirdParty/raylib/lib/raylib.lib)
else()
    set(RAYLIB_LIBRARY ${GEIST_DIR}/ThirdParty/raylib/lib/libraylib.a)
endif()

add_library(raylib STATIC IMPORTED)
set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION ${RAYLIB_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR};${RAYLIB_EXTERNAL_INCLUDE}"
)

# Lua
set(LUA_SOURCES
        ${GEIST_DIR}/ThirdParty/lua/src/lapi.c
        ${GEIST_DIR}/ThirdParty/lua/src/lcode.c
        ${GEIST_DIR}/ThirdParty/lua/src/lctype.c
        ${GEIST_DIR}/ThirdParty/lua/src/ldebug.c
        ${GEIST_DIR}/ThirdParty/lua/src/ldo.c
        ${GEIST_DIR}/ThirdParty/lua/src/ldump.c
        ${GEIST_DIR}/ThirdParty/lua/src/lfunc.c
        ${GEIST_DIR}/ThirdParty/lua/src/lgc.c
        ${GEIST_DIR}/ThirdParty/lua/src/llex.c
        ${GEIST_DIR}/ThirdParty/lua/src/lmem.c
        ${GEIST_DIR}/ThirdParty/lua/src/lobject.c
        ${GEIST_DIR}/ThirdParty/lua/src/lopcodes.c
        ${GEIST_DIR}/ThirdParty/lua/src/lparser.c
        ${GEIST_DIR}/ThirdParty/lua/src/lstate.c
        ${GEIST_DIR}/ThirdParty/lua/src/lstring.c
        ${GEIST_DIR}/ThirdParty/lua/src/ltable.c
        ${GEIST_DIR}/ThirdParty/lua/src/ltm.c
        ${GEIST_DIR}/ThirdParty/lua/src/lundump.c
        ${GEIST_DIR}/ThirdParty/lua/src/lvm.c
        ${GEIST_DIR}/ThirdParty/lua/src/lzio.c
        ${GEIST_DIR}/ThirdParty/lua/src/lauxlib.c
        ${GEIST_DIR}/ThirdParty/lua/src/lbaselib.c
        ${GEIST_DIR}/ThirdParty/lua/src/lcorolib.c
        ${GEIST_DIR}/ThirdParty/lua/src/ldblib.c
        ${GEIST_DIR}/ThirdParty/lua/src/liolib.c
        ${GEIST_DIR}/ThirdParty/lua/src/lmathlib.c
        ${GEIST_DIR}/ThirdParty/lua/src/loslib.c
        ${GEIST_DIR}/ThirdParty/lua/src/lstrlib.c
        ${GEIST_DIR}/ThirdParty/lua/src/ltablib.c
        ${GEIST_DIR}/ThirdParty/lua/src/linit.c
        ${GEIST_DIR}/ThirdParty/lua/src/loadlib.c
        ${GEIST_DIR}/ThirdParty/lua/src/lutf8lib.c
)

add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${GEIST_DIR}/ThirdParty/lua/src)
target_compile_definitions(lua PRIVATE LUA_COMPAT_5_3)
if(WIN32)
    target_compile_definitions(lua PRIVATE LUA_BUILD_AS_DLL)
endif()

if(MSVC)
    set_property(TARGET lua PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# ============================================================================
# Geist Sources
# ============================================================================
set(GEIST_SOURCES
        ${GEIST_DIR}/Source/BaseUnits.cpp
        ${GEIST_DIR}/Source/Config.cpp
        ${GEIST_DIR}/Source/Engine.cpp
        ${GEIST_DIR}/Source/Globals.cpp
        ${GEIST_DIR}/Source/Gui.cpp
        ${GEIST_DIR}/Source/GuiElements.cpp
        ${GEIST_DIR}/Source/GuiManager.cpp
        ${GEIST_DIR}/Source/InputSystem.cpp
        ${GEIST_DIR}/Source/IO.cpp
        ${GEIST_DIR}/Source/Logging.cpp
        ${GEIST_DIR}/Source/ParticleSystem.cpp
        ${GEIST_DIR}/Source/Primitives.cpp
        ${GEIST_DIR}/Source/RaylibModel.cpp
        ${GEIST_DIR}/Source/ResourceManager.cpp
        ${GEIST_DIR}/Source/RNG.cpp
        ${GEIST_DIR}/Source/ScriptingSystem.cpp
        ${GEIST_DIR}/Source/SoundSystem.cpp
        ${GEIST_DIR}/Source/StateMachine.cpp
        ${GEIST_DIR}/Source/TooltipSystem.cpp
)

# ============================================================================
# Project Sources
# ============================================================================
set(PROJECT_SOURCES
        Source/CampaignSetupConfig.cpp
        Source/CastleDesignState.cpp
        Source/CombatState.cpp
        Source/CombatUnits.cpp
        Source/GameGlobals.cpp
        Source/LittlePeopleSprites.cpp
        Source/MapTilesSprites.cpp
        Source/Main.cpp
        Source/MainState.cpp
        Source/OverworldMap.cpp
        Source/Player.cpp
        Source/PlayerTasksConfig.cpp
        Source/OptionsState.cpp
        Source/RegionMinimap.cpp
        Source/RegionTerrainMesh.cpp
        Source/RegionView.cpp
        Source/SetupGameState.cpp
        Source/TerrainTextures.cpp
        Source/TitleState.cpp
)

# ============================================================================
# Executable
# ============================================================================
add_executable(LittleWars WIN32
        ${GEIST_SOURCES}
        ${PROJECT_SOURCES}
        $<$<PLATFORM_ID:Windows>:Source/littlewars.rc>
)

target_include_directories(LittleWars PRIVATE
        ${GEIST_DIR}/Source
        ${GEIST_DIR}/ThirdParty/nlohmann
        ${RAYLIB_EXTERNAL_INCLUDE}
)

target_link_libraries(LittleWars PRIVATE raylib lua)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(LittleWars PRIVATE
            ${X11_LIBRARIES}
            X11::Xrandr
            X11::Xi
            X11::Xinerama
            X11::Xcursor
            pthread dl m
    )
elseif(WIN32)
    target_link_libraries(LittleWars PRIVATE winmm)
    set_target_properties(LittleWars PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# Build config definitions
target_compile_definitions(LittleWars PRIVATE
        $<$<CONFIG:Debug>:DEBUG_MODE>
        $<$<CONFIG:Release>:RELEASE_MODE>
)

# Redist setup (unchanged)
set(REDIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Redist")

set_target_properties(LittleWars PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${REDIST_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${REDIST_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${REDIST_DIR}"
)

if(WIN32)
    set_target_properties(LittleWars PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${REDIST_DIR}")
else()
    set_target_properties(LittleWars PROPERTIES
            OUTPUT_NAME_DEBUG "littlewars_debug"
            OUTPUT_NAME_RELEASE "littlewars"
    )
endif()

add_custom_command(TARGET LittleWars POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REDIST_DIR}/Data"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Data/campaign_setup.json"
        "${REDIST_DIR}/Data/campaign_setup.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Data/player_tasks.json"
        "${REDIST_DIR}/Data/player_tasks.json"
        COMMENT "Copying campaign setup data to Redist folder"
)

# Asset copy
#add_custom_command(TARGET LittleWars POST_BUILD
#//        COMMAND ${CMAKE_COMMAND} -E make_directory "${REDIST_DIR}/assets"
#//        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
##//        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
#//        "${REDIST_DIR}/assets"
#//        COMMENT "Copying assets to Redist folder"
#//)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LittleWars)