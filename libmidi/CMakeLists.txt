cmake_minimum_required(VERSION 3.8)
## start setting
PROJECT(midi)
set(CMAKE_MACOSX_RPATH 0)
set(CMAKE_CXX_STANDARD 14)
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#if(APPLE)
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 
#endif(APPLE)

option(USE_STATIC          "Set to ON to build Static Library"  OFF)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations -Wno-writable-strings")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-writable-strings")

find_package(PkgConfig)
find_package(Curses REQUIRED)
pkg_search_module(SDL2 REQUIRED sdl2)

# Apple try to find openmp if we using clang
if (APPLE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang\$")
        set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_C_LIB_NAMES "omp")
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(OpenMP_C_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/opt/homebrew/opt/libomp/lib ${OpenMP_C_LIB_NAMES})
        else()
            set(OpenMP_C_INCLUDE_DIR "/usr/local/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/usr/local/opt/libomp/lib ${OpenMP_C_LIB_NAMES})
        endif()
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang\$")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/opt/homebrew/opt/libomp/lib ${OpenMP_CXX_LIB_NAMES})
        else()
            set(OpenMP_CXX_INCLUDE_DIR "/usr/local/opt/libomp/include")
            set(OpenMP_omp_LIBRARY -L/usr/local/opt/libomp/lib ${OpenMP_CXX_LIB_NAMES})
        endif()
    endif()
endif()

find_package(OpenMP)
if (APPLE AND OPENMP_FOUND)
    message(STATUS "libmidi(Apple) Use OpenMP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    include_directories(${OpenMP_CXX_INCLUDE_DIR})
    set(LINK_LIBS ${LINK_LIBS} ${OpenMP_omp_LIBRARY})
elseif(OPENMP_FOUND)
    if(WIN32 OR MINGW)
        if (NOT IMGUI_STATIC)
            message(STATUS "libmidi(Windows) Use OpenMP")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
            if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                set(LINK_LIBS ${LINK_LIBS} omp pthread)
            else ()
                set(LINK_LIBS ${LINK_LIBS} gomp pthread)
            endif ()
        endif()
    else()
        message(STATUS "imlibmidigui Use OpenMP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(LINK_LIBS ${LINK_LIBS} omp pthread)
        else ()
            set(LINK_LIBS ${LINK_LIBS} gomp pthread)
        endif ()
    endif()
endif()

set(MIDI_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (CURSES_FOUND)
    set(MIDI_INC_DIRS
        ${MIDI_INC_DIRS}
        ${CURSES_INCLUDE_DIRS}
    )
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${CURSES_LIBRARIES})
endif(CURSES_FOUND)

if(SDL2_FOUND)
    set(MIDI_INC_DIRS
        ${MIDI_INC_DIRS}
        ${SDL2_INCLUDE_DIRS}
    )
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${SDL2_STATIC_LDFLAGS})
endif(SDL2_FOUND)

add_definitions(-DNO_MIDI_CACHE=1)

## core library
SET(MIDI_SOURCE_FILES
    common.c
    mid_defs.c
    audio_cnv.c
    aq.c
    instrum.c
    mix.c
    m2m.c
    memb.c
    mfi.c
    rcp.c
    smplfile.c
    mblock.c
    output.c
    filter.c
    effect.c
    miditrace.c
    playmidi.c
    readmidi.c
    resample.c
    reverb.c
    recache.c
    tables.c
    sndfont.c
    sffile.c
    sfitem.c
    mt19937ar.c
    quantity.c
    strtab.c
    freq.c
    fft4g.c
    url.c
    url_dir.c
    url_file.c
    url_cache.c
    url_mem.c
    url_pipe.c
    sbkconv.c
    wave_a.c
    raw_a.c
    midi_a.c
    sdl_a.c
    bitset.c
    controls.c
    dumb_c.c
    ncurs_c.c
    sfgm.c
)

SET(MIDI_HEADER_FILES
    sysdep.h
    common.h
    audio_cnv.h
    aq.h
    filter.h
    instrum.h
    mix.h
    freq.h
    fft4g.h
    memb.h
    mblock.h
    output.h
    playmidi.h
    readmidi.h
    resample.h
    reverb.h
    recache.h
    newton_table.h
    tables.h
    strtab.h
    url.h
    sffile.h
    sfitem.h
    sflayer.h
    mt19937ar.h
    quantity.h
    server_defs.h
    bitset.h
    controls.h
)

include_directories(${MIDI_INC_DIRS})

set(MIDI_VERSION_MAJOR 2)
set(MIDI_VERSION_MINOR 15)
set(MIDI_VERSION_PATCH 4)
set(MIDI_VERSION_STRING ${MIDI_VERSION_MAJOR}.${MIDI_VERSION_MINOR}.${MIDI_VERSION_PATCH})

if (USE_STATIC)
    ADD_LIBRARY(midi STATIC ${MIDI_SOURCE_FILES} ${MIDI_HEADER_FILES})
else()
    ADD_LIBRARY(midi SHARED ${MIDI_SOURCE_FILES} ${MIDI_HEADER_FILES})
    target_link_libraries(midi ${EXTRA_LIBS} ${LINK_LIBS})
    set_property(TARGET midi PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_target_properties(midi PROPERTIES VERSION ${MIDI_VERSION_STRING} SOVERSION ${MIDI_VERSION_MAJOR})
endif()


if(IOS AND CMAKE_OSX_ARCHITECTURES MATCHES "arm")
    message(STATUS "Target arch: arm-ios")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -mfpu=neon -march=armv7 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loop")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    message(STATUS "Target arch: arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8 -ftree-vectorize -fpermissive -fomit-frame-pointer -funroll-loops")
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
    message(STATUS "Target arch: x86")
    if(MSVC OR MSVC_IDE)
        target_compile_options(midi PRIVATE /arch:AVX2 /arch:AVX /arch:FMA)
    else()
        target_compile_options(midi PRIVATE -mavx2 -mavx -mfma)
    endif()
endif()


get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(MIDI_LIBRARYS midi ${EXTRA_LIBS} PARENT_SCOPE )
    set(MIDI_INCLUDES ${MIDI_HEADER_FILES} PARENT_SCOPE )
    set(MIDI_INCLUDE_DIRS ${MIDI_INC_DIRS} PARENT_SCOPE )
endif()