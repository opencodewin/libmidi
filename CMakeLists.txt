cmake_minimum_required(VERSION 3.8)
## start setting
PROJECT(midi_test)
set(CMAKE_CXX_STANDARD 14)
option(USE_STATIC          "Set to ON to build Static Library"  OFF)
option(USE_IMGUI           "Set to ON to build with imgui test"  ON)

set(PROJECT_VER_MAJOR 1)
set(PROJECT_VER_MINOR 0)
set(PROJECT_VER_PATCH 2)
set(PROJECT_VER "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations -Wno-writable-strings")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-writable-strings")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|x86_64|AMD64)")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mavx")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mavx")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.2 -msse4.1 -mssse3 -msse2 -msse")
endif()

if (APPLE)
set(FRAMEWORKS_PATH ${PROJECT_BINARY_DIR}/lib)
else(APPLE)
set(FRAMEWORKS_PATH ${PROJECT_BINARY_DIR})
endif(APPLE)

add_subdirectory(libmidi)
set_target_properties(
    midi
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
    RUNTIME_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
)
include_directories(
    ${MIDI_INCLUDE_DIRS}
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
add_subdirectory(imgui)
set_target_properties(
    imgui
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
    RUNTIME_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
)
if(IMGUI_VULKAN_SHADER)
set_target_properties(
    VkShader
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
    RUNTIME_OUTPUT_DIRECTORY "${FRAMEWORKS_PATH}"
)
endif(IMGUI_VULKAN_SHADER)

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
#if we need run on MacOS before 12.0, then uncomment following code, but not guarantee it's working
#set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") 

add_executable(
    immidi
    MACOSX_BUNDLE
    test/immidi.cpp
    ${IMGUI_APP_ENTRY_SRC}
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
set_target_properties(immidi PROPERTIES MACOSX_BUNDLE TRUE MACOSX_FRAMEWORK_IDENTIFIER com.code-win.immidi RESOURCE "${RESOURCE_FILES}")
elseif(WIN32)
add_executable(
    immidi
    test/immidi.cpp
    test/logo.rc
    ${IMGUI_APP_ENTRY_SRC}
)
target_link_libraries(
    immidi
    ${MIDI_LIBRARYS}
    ${IMGUI_LIBRARYS}
)
else()
add_executable(
    immidi
    test/immidi.cpp
    ${IMGUI_APP_ENTRY_SRC}
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
if (IMGUI_VULKAN)
    if (IMGUI_SDL2)
        set(CPACK_SYSTEM_NAME  "immidi_SDL2_Vulkan")
    elseif (IMGUI_GLFW)
        set(CPACK_SYSTEM_NAME  "immidi_GLFW_Vulkan")
    endif()
elseif (IMGUI_GL3)
    if (IMGUI_SDL2)
        set(CPACK_SYSTEM_NAME  "immidi_SDL2_OpenGL3")
    elseif (IMGUI_GLFW)
        set(CPACK_SYSTEM_NAME  "immidi_GLFW_OpenGL3")
    endif()
elseif (IMGUI_GL2)
    if (IMGUI_SDL2)
        set(CPACK_SYSTEM_NAME  "immidi_SDL2_OpenGL2")
    elseif (IMGUI_GLFW)
        set(CPACK_SYSTEM_NAME  "immidi_GLFW_OpenGL2")
    endif()
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "immidi installation.")
set(CPACK_PACKAGE_VENDOR "CodeWin")
set(CPACK_PACKAGE_VERSION ${PROJECT_VER})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VER_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VER_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VER_PATCH})

#message(STATUS "vulkan library:" ${Vulkan_LIBRARY})

if(APPLE)
    # Bundle target
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_BINARY_DRAGNDROP ON)
    # OSX cpack info
    install(TARGETS immidi
            CONFIGURATIONS Release RelWithDebInfo
            BUNDLE  DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION bin COMPONENT Runtime
    )

    set(frameworks_dir immidi.app/Contents/Frameworks/)
    set(resources_dir immidi.app/Contents/Resources/)
    # Install Resource File
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/midi_demo/"
            DESTINATION "${resources_dir}/midi_demo/" COMPONENT Runtime)
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/soundfont/"
            DESTINATION "${resources_dir}/soundfont/" COMPONENT Runtime)

    # package runtime fixup bundle
    set(APPS "\${CMAKE_INSTALL_PREFIX}/immidi.app")
    set(DIRS ${FRAMEWORKS_PATH})
    list (APPEND DIRS "/usr/local/lib/")
    list (APPEND DIRS "${PROJECT_BINARY_DIR}")
    INSTALL(CODE "
        include(BundleUtilities)
        fixup_bundle(\"${APPS}\"   \"\"  \"${DIRS}\")
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

elseif(UNIX)
    set(CPACK_SYSTEM_NAME  "immidi")
    if (IMGUI_VULKAN)
        if (IMGUI_SDL2)
            set(CPACK_SYSTEM_NAME  "immidi_SDL2_Vulkan")
        elseif (IMGUI_GLFW)
            set(CPACK_SYSTEM_NAME  "immidi_GLFW_Vulkan")
        endif()
    elseif (IMGUI_GL3)
        if (IMGUI_SDL2)
            set(CPACK_SYSTEM_NAME  "immidi_SDL2_OpenGL3")
        elseif (IMGUI_GLFW)
            set(CPACK_SYSTEM_NAME  "immidi_GLFW_OpenGL3")
        endif()
    elseif (IMGUI_GL2)
        if (IMGUI_SDL2)
            set(CPACK_SYSTEM_NAME  "immidi_SDL2_OpenGL2")
        elseif (IMGUI_GLFW)
            set(CPACK_SYSTEM_NAME  "immidi_GLFW_OpenGL2")
        endif()
    endif()

    install(CODE "
        include(../cmake/appimage.cmake)
        make_appimage(
            PROJECT_DIR \"${PROJECT_SOURCE_DIR}\"
            EXE \"${CMAKE_BINARY_DIR}/immidi\"
            NAME \"IMMIDI\"
            OUTPUT_NAME \"${CPACK_SYSTEM_NAME}\"
        )
    " COMPONENT Runtime)
elseif(WIN32)
    macro(prepareNSIS_Link linkName appName params)
        #prepare start menu links
        LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA "  CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${linkName}.lnk' '$INSTDIR\\\\bin\\\\${appName}.exe' '${params}'")
        LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA "  Delete '$SMPROGRAMS\\\\$START_MENU\\\\${linkName}.lnk'")

        #prepare desktop links
        LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA  "  CreateShortCut '$DESKTOP\\\\${linkName}.lnk' '$INSTDIR\\\\bin\\\\${appName}.exe' '${params}'")
        LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA  "  Delete '$DESKTOP\\\\${linkName}.lnk'")

        #replace new line
        string (REPLACE ";" "\n" CPACK_NSIS_CREATE_ICONS_EXTRA "${CPACK_NSIS_CREATE_ICONS_EXTRA}")
        string (REPLACE ";" "\n" CPACK_NSIS_DELETE_ICONS_EXTRA "${CPACK_NSIS_DELETE_ICONS_EXTRA}")
    endmacro()

    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")
    install(TARGETS immidi
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
            COMPONENT applications)
    
    # Install the SDL (all those installed with mingw64 )
    install(FILES   "C:/msys64/mingw64/bin/SDL2.dll"
                    "C:/msys64/mingw64/bin/SDL2_image.dll"
            DESTINATION bin COMPONENT binaries)

    # Install the GLFW/GLEW (all those installed with mingw64 )
    install(FILES   "C:/msys64/mingw64/bin/glfw3.dll"
                    "C:/msys64/mingw64/bin/glew32.dll"
            DESTINATION bin COMPONENT binaries)
    
    # Install the mingw library (all those installed with mingw64 )
    install(FILES   "C:/msys64/mingw64/bin/libgcc_s_seh-1.dll"
                    "C:/msys64/mingw64/bin/libwinpthread-1.dll"
                    "C:/msys64/mingw64/bin/libstdc++-6.dll"
                    "C:/msys64/mingw64/bin/zlib1.dll"
            DESTINATION bin COMPONENT binaries)
    
    # Install the msys (all those installed with msys )
    install(FILES   "C:/msys64/usr/bin/msys-ncursesw6.dll"
                    "C:/msys64/usr/bin/msys-2.0.dll"
            DESTINATION bin COMPONENT binaries)

    # Install the built library
    file(GLOB BUILT_LIBS "${CMAKE_BINARY_DIR}/*.dll")
    install(FILES   ${BUILT_LIBS}
            DESTINATION bin COMPONENT binaries)

    # Install Resource File
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/midi_demo/"
            DESTINATION midi_demo COMPONENT Demo)
    install(DIRECTORY  "${PROJECT_SOURCE_DIR}/soundfont/"
            DESTINATION soundfont COMPONENT SoundFont)

    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/test/immidi.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/test/immidi.ico")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_NAME immidi)
    set(CPACK_NSIS_DISPLAY_NAME immidi)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY immidi)
    
    prepareNSIS_Link("immidi" "immidi" "")

endif(APPLE)

# Package full name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SYSTEM_NAME}")

# To Create a package, run "cpack"
include(CPack)
