# Shared packaging helpers (version string, install layout).

function(gxdlms_read_project_version out_var)
    if(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
        file(READ "${CMAKE_SOURCE_DIR}/VERSION" _version)
        string(STRIP "${_version}" _version)
        set("${out_var}" "${_version}" PARENT_SCOPE)
        return()
    endif()
    set("${out_var}" "${PROJECT_VERSION}" PARENT_SCOPE)
endfunction()

if(NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/GXDLMSDirector.desktop"
            DESTINATION share/applications)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/icons/GXDLMSDirector.png"
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME GXDLMSDirector.png)
endif()
