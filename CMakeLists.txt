cmake_minimum_required(VERSION 3.5)
## start setting
PROJECT(midi_test)
set(CMAKE_CXX_STANDARD 14)

option(USE_STATIC          "Set to ON to build Static Library"  OFF)
option(USE_IMGUI           "Set to ON to build with imgui test"  ON)

SET(SDK_SOUNDFONT_DIR ${PROJECT_SOURCE_DIR}/soundfont/)
SET(SDK_DEMO_DIR ${PROJECT_SOURCE_DIR}/midi_demo/)
CONFIGURE_FILE(Config.h.in Config.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations -Wno-writable-strings")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-writable-strings")

add_subdirectory(libmidi)
set_target_properties(
    midi
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
include_directories(
    ${MIDI_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
)

SET(TEST_SOURCE_FILE
    test/timidity.c
)

ADD_EXECUTABLE(
    timidity
    ${TEST_SOURCE_FILE}
)

TARGET_LINK_LIBRARIES(
    timidity
    ${MIDI_LIBRARYS}
)

ADD_EXECUTABLE(
    sample
    test/sample.c
)

TARGET_LINK_LIBRARIES(
    sample
    ${MIDI_LIBRARYS}
)

if (USE_IMGUI)
set(IMGUI_ICONS ON CACHE BOOL "Enable Internal Icons Build by Force" FORCE)
set(IMGUI_VULKAN_SHADER OFF CACHE BOOL "Disable VkShader Build by Force" FORCE)
set(IMGUI_VULKAN_SHADER OFF CACHE BOOL "Disable VkShader Build by Force" FORCE)
add_subdirectory(imgui)
set_target_properties(
    imgui
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

include_directories(
    ${IMGUI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
)
add_executable(
    immidi
    test/immidi.cpp
    ${IMGUI_APP_ENTRY_SRC}
    ${PROJECT_BINARY_DIR}/Config.h
)
target_link_libraries(
    immidi
    ${MIDI_LIBRARYS}
    ${IMGUI_LIBRARYS}
)
endif(USE_IMGUI)

ADD_EXECUTABLE(
    bin2c
    test/bin2c.c
)

TARGET_LINK_LIBRARIES(
    bin2c
    ${EXTRA_LIBS}
)
