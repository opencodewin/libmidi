cmake_minimum_required(VERSION 3.8)
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

set(FRAMEWORKS_PATH ${PROJECT_BINARY_DIR}/lib)

add_subdirectory(libmidi)
set_target_properties(
    midi
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
    RUNTIME_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
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
    LIBRARY_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
    RUNTIME_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
)

include_directories(
    ${IMGUI_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
)

if(APPLE)
set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})
set(MACOSX_BUNDLE_ICON immidi.icns)
set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/test/${MACOSX_BUNDLE_ICON})
# set where in the bundle to put the icns file
set_source_files_properties(${MACOSX_BUNDLE_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(
    immidi
    MACOSX_BUNDLE
    test/immidi.cpp
    ${IMGUI_APP_ENTRY_SRC}
    ${PROJECT_BINARY_DIR}/Config.h
    ${MACOSX_BUNDLE_ICON_FILE}
)
target_link_libraries(
    immidi
    ${MIDI_LIBRARYS}
    ${IMGUI_LIBRARYS}
)
# set the Info.plist file
set(MACOSX_BUNDLE_PLIST_FILE ${CMAKE_SOURCE_DIR}/test/Info.plist)
set_target_properties(immidi PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_PLIST_FILE})

else(APPLE)
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
endif(APPLE)

endif(USE_IMGUI)

ADD_EXECUTABLE(
    bin2c
    test/bin2c.c
)

TARGET_LINK_LIBRARIES(
    bin2c
    ${EXTRA_LIBS}
)

### DEFINE THE PACKAGING (OS specific)

if(APPLE)
    # Bundle target
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_BINARY_DRAGNDROP ON)
    # OSX cpack info
    set(CPACK_SYSTEM_NAME  "IMMIDI")
    install(TARGETS immidi
            CONFIGURATIONS Release RelWithDebInfo
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

    set(frameworks_dir immidi.app/Contents/Frameworks/)
    # Install the SDL (all those installed with brew )
    install(FILES   "/usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib"
                    "/usr/local/opt/sdl2_image/lib/libSDL2_image-2.0.0.dylib"
            DESTINATION "${frameworks_dir}" COMPONENT Runtime)

    # Install the glew/glfw (all those installed with brew )
    install(FILES   "/usr/local/opt/glew/lib/libGLEW.2.2.dylib"
                    "/usr/local/opt/glew/lib/libGLEW.2.2.0.dylib"
                    "/usr/local/opt/glfw/lib/libglfw.3.dylib"
                    "/usr/local/opt/glfw/lib/libglfw.3.3.dylib"
            DESTINATION "${frameworks_dir}" COMPONENT Runtime)

    # Install the vulkan library (all those installed by Vulkan SDK )
    install(FILES   "/usr/local/lib/libvulkan.1.dylib"
                    "/usr/local/lib/libvulkan.1.3.211.dylib"
            DESTINATION "${frameworks_dir}" COMPONENT Runtime)


    # Install the built library
    install(FILES   "${PROJECT_BINARY_DIR}/lib/libimgui.dylib"
                    "${PROJECT_BINARY_DIR}/lib/libimgui.1.dylib"
                    "${PROJECT_BINARY_DIR}/lib/libimgui.1.88.00.dylib"
                    "${PROJECT_BINARY_DIR}/lib/libmidi.dylib"
                    "${PROJECT_BINARY_DIR}/lib/libmidi.2.dylib"
                    "${PROJECT_BINARY_DIR}/lib/libmidi.2.15.2.dylib"
            DESTINATION "${frameworks_dir}" COMPONENT Runtime)

    # package runtime fixup bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/immidi.app")
    set(DIRS "\${CMAKE_INSTALL_PREFIX}/${frameworks_dir}")
    INSTALL(CODE "
        include(BundleUtilities)
        fixup_bundle(\"${APPS}\"   \"\"   \"${DIRS}\")
    " COMPONENT Runtime)

    set(APPLE_CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/test/entitlements.plist")
    set(APPLE_CODESIGN_IDENTITY "" CACHE STRING "")
    string(LENGTH "${APPLE_CODESIGN_IDENTITY}" APPLE_CODESIGN_IDENTITY_LENGHT)
    if( ${APPLE_CODESIGN_IDENTITY_LENGHT} LESS 40 )
        message(STATUS "Not signing bundle. Specify APPLE_CODESIGN_IDENTITY to cmake before running cpack to sign")
    else()
        install(CODE "
                execute_process(COMMAND
                codesign -vvv --deep --force
                --entitlements \"${APPLE_CODESIGN_ENTITLEMENTS}\"
                --sign \"${APPLE_CODESIGN_IDENTITY}\"
                \"${APPS}\" )
                "
                COMPONENT Runtime
        )
    endif()


else(APPLE)
endif(APPLE)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SYSTEM_NAME}")

# To Create a package, run "cpack"
include(CPack)
