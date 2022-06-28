function(make_appimage)
	set(optional)
	set(args PROJECT_DIR EXE NAME OUTPUT_NAME)
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

    # copy sound font and midi-demo files
    file(COPY "${ARGS_PROJECT_DIR}/soundfont" DESTINATION "${APPDIR}/usr")
    file(COPY "${ARGS_PROJECT_DIR}/midi_demo" DESTINATION "${APPDIR}/usr")

    # prepare desktop and icon file
    set(DESKTOP_SRC "${ARGS_PROJECT_DIR}/test/linuxdeploy.desktop")
    file(COPY "${DESKTOP_SRC}" DESTINATION "${CMAKE_BINARY_DIR}")
    get_filename_component(TEMP "${DESKTOP_SRC}" NAME)
    set(DESKTOP_FILE "${CMAKE_BINARY_DIR}/immidi.desktop")
    file(RENAME "${CMAKE_BINARY_DIR}/${TEMP}" "${DESKTOP_FILE}")
    set(ICON_SRC "${ARGS_PROJECT_DIR}/test/immidi.png")
    file(COPY "${ICON_SRC}" DESTINATION "${CMAKE_BINARY_DIR}")
    get_filename_component(TEMP "${ICON_SRC}" NAME)
    get_filename_component(ICON_EXT "${ICON_SRC}" EXT)
    set(ICON_FILE "${CMAKE_BINARY_DIR}/immidi${ICON_EXT}")
    file(RENAME "${CMAKE_BINARY_DIR}/${TEMP}" "${ICON_FILE}")

    # use linuxdeploy to generate the appimage
    set(ENV{OUTPUT} "${ARGS_OUTPUT_NAME}-x86_64.AppImage")
    execute_process(COMMAND ${LINUX_DEPLOY_PATH} --appdir ${APPDIR} -e ${ARGS_EXE} -d ${DESKTOP_FILE} -i ${ICON_FILE} --output appimage)

endfunction()
