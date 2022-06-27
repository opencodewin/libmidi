function(make_appimage)
	set(optional)
	set(args EXE NAME DESKTOP ICON OUTPUT_NAME)
	set(list_args ASSETS)
	cmake_parse_arguments(
		PARSE_ARGV 0
		ARGS
		"${optional}"
		"${args}"
		"${list_args}"
	)

	if(${ARGS_UNPARSED_ARGUMENTS})
		message(WARNING "Unparsed arguments: ${ARGS_UNPARSED_ARGUMENTS}")
	endif()


    # download linuxdeploy if needed (TODO: non-x86 build machine?)
    SET(LINUX_DEPLOY_PATH "${CMAKE_BINARY_DIR}/linuxdeploy.AppImage" CACHE INTERNAL "")
    if (NOT EXISTS "${LINUX_DEPLOY_PATH}")
        file(DOWNLOAD https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage "${LINUX_DEPLOY_PATH}")
        execute_process(COMMAND chmod +x ${LINUX_DEPLOY_PATH})
    endif()

    # make the AppDir
    set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
    file(REMOVE_RECURSE "${APPDIR}")       # remove if leftover
    file(MAKE_DIRECTORY "${APPDIR}")

    # use linuxdeploy to generate the appimage
    message("ARGS_DESKTOP = '${ARGS_DESKTOP}'")
    file(COPY "${ARGS_DESKTOP}" DESTINATION "${CMAKE_BINARY_DIR}")
    get_filename_component(TEMP "${ARGS_DESKTOP}" NAME)
    set(DESKTOP_FILE "${CMAKE_BINARY_DIR}/immidi.desktop")
    file(RENAME "${CMAKE_BINARY_DIR}/${TEMP}" "${DESKTOP_FILE}")
    file(COPY "${ARGS_ICON}" DESTINATION "${CMAKE_BINARY_DIR}")
    get_filename_component(TEMP "${ARGS_ICON}" NAME)
    get_filename_component(ICON_EXT "${ARGS_ICON}" EXT)
    set(ICON_FILE "${CMAKE_BINARY_DIR}/immidi${ICON_EXT}")
    file(RENAME "${CMAKE_BINARY_DIR}/${TEMP}" "${ICON_FILE}")
    set(ENV{OUTPUT} "immidi-x86_64.AppImage")
    execute_process(COMMAND ${LINUX_DEPLOY_PATH} --appdir ${APPDIR} -e ${ARGS_EXE} -d ${DESKTOP_FILE} -i ${ICON_FILE} --output appimage)











#     # copy assets to appdir
#     file(COPY ${ARGS_ASSETS} DESTINATION "${APPDIR}")

#     # copy icon thumbnail
#     file(COPY ${ARGS_DIR_ICON} DESTINATION "${APPDIR}")
#     get_filename_component(THUMB_NAME "${ARGS_DIR_ICON}" NAME)
#     file(RENAME "${APPDIR}/${THUMB_NAME}" "${APPDIR}/.DirIcon")

#     # copy icon highres
#     file(COPY ${ARGS_ICON} DESTINATION "${APPDIR}")
#     get_filename_component(ICON_NAME "${ARGS_ICON}" NAME)
#     get_filename_component(ICON_EXT "${ARGS_ICON}" EXT)
#     file(RENAME "${APPDIR}/${ICON_NAME}" "${APPDIR}/${ARGS_NAME}${ICON_EXT}")

#     # Create the .desktop file
#     file(WRITE "${APPDIR}/${ARGS_NAME}.desktop" 
#     "[Desktop Entry]
# Type=Application
# Name=${ARGS_NAME}
# Icon=${ARGS_NAME}
# Categories=X-None;"    
#     )

#     # Invoke AppImageTool
#     execute_process(COMMAND ${AIT_PATH} ${APPDIR} ${ARGS_OUTPUT_NAME})
#     file(REMOVE_RECURSE "${APPDIR}")

endfunction()
