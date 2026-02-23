cmake_minimum_required(VERSION 3.10)
project(shipobs_pi)

set(PACKAGE_NAME shipobs_pi)
set(PLUGIN_VERSION_MAJOR 0)
set(PLUGIN_VERSION_MINOR 1)

message(STATUS "*** Building ${PACKAGE_NAME} ***")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MACOSX_RPATH ON)

# Compiler flags
if(NOT WIN32 AND NOT APPLE)
    add_definitions(-Wall -O2 -fexceptions -fvisibility=hidden)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
        set(CMAKE_SHARED_LINKER_FLAGS
            "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
    endif()
endif()

if(MSVC)
    add_definitions(-D__MSVC__)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_DEPRECATE)
endif()

if(APPLE)
    add_definitions(-Wall -O2 -fexceptions -fvisibility=hidden)
endif()

# wxWidgets — try find_package first, fall back to manual paths from OpenCPN build
if(NOT QT_ANDROID)
    if(NOT DEFINED wxWidgets_USE_FILE)
        set(wxWidgets_USE_LIBS base core net xml html adv gl)
        set(BUILD_SHARED_LIBS TRUE)
        find_package(wxWidgets QUIET)
    endif()
    if(wxWidgets_FOUND)
        include(${wxWidgets_USE_FILE})
    else()
        message(STATUS "wxWidgets not found via find_package, using OpenCPN build paths")
        # Use wx headers extracted from dev packages
        set(WX_INCLUDE_DIRS
            /tmp/wx-dev/extracted/usr/include/wx-3.2
            /tmp/wx-dev/extracted/usr/lib/x86_64-linux-gnu/wx/include/gtk3-unicode-3.2
        )
        set(WX_LIBRARIES
            -L/tmp/wx-dev/lib -pthread
            -lwx_gtk3u_gl-3.2
            -lwx_baseu_net-3.2
            -lwx_baseu_xml-3.2
            -lwx_gtk3u_html-3.2
            -lwx_gtk3u_aui-3.2
            -lwx_gtk3u_core-3.2
            -lwx_baseu-3.2
        )
        add_definitions(-D_FILE_OFFSET_BITS=64 -DWXUSINGDLL -D__WXGTK__)
    endif()
endif()

# OpenGL
find_package(OpenGL REQUIRED)

# libcurl
find_package(CURL REQUIRED)

# OpenCPN headers
set(OPENCPN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/opencpn"
    CACHE PATH "Path to OpenCPN include directory")

# Generate info_html.h from INFO.md at build time
set(GENERATED_INFO_HEADER ${CMAKE_CURRENT_BINARY_DIR}/info_html.h)
add_custom_command(
    OUTPUT  ${GENERATED_INFO_HEADER}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_info_html.py
                    ${CMAKE_CURRENT_SOURCE_DIR}/INFO.md
                    ${GENERATED_INFO_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/INFO.md
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_info_html.py
    COMMENT "Generating info_html.h from INFO.md"
    VERBATIM
)

# Sources
set(SRC_PLUGIN
    src/shipobs_pi.h
    src/shipobs_pi.cpp
    src/observation.h
    src/server_client.h
    src/server_client.cpp
    src/ship_reports_plugin_dialog.h
    src/ship_reports_plugin_dialog.cpp
    src/render_overlay.h
    src/render_overlay.cpp
    src/station_popup.h
    src/station_popup.cpp
    src/station_info_frame.h
    src/station_info_frame.cpp
    src/settings_dialog.h
    src/settings_dialog.cpp
)

set(SRC_WXJSON
    src/wxJSON/jsonval.cpp
    src/wxJSON/jsonreader.cpp
    src/wxJSON/jsonwriter.cpp
    include/wx/json_defs.h
    include/wx/jsonreader.h
    include/wx/jsonval.h
    include/wx/jsonwriter.h
)

set(BUILD_SHARED_LIBS ON)
add_library(${PACKAGE_NAME} SHARED ${SRC_PLUGIN} ${SRC_WXJSON} ${GENERATED_INFO_HEADER})

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
    target_compile_options(${PACKAGE_NAME} PRIVATE -fvisibility=hidden)
endif()

target_include_directories(${PACKAGE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wx
    ${OPENCPN_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}   # for generated info_html.h
)

if(wxWidgets_FOUND)
    target_link_libraries(${PACKAGE_NAME}
        ${wxWidgets_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CURL_LIBRARIES}
    )
else()
    target_include_directories(${PACKAGE_NAME} PRIVATE ${WX_INCLUDE_DIRS})
    target_link_libraries(${PACKAGE_NAME}
        ${WX_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${CURL_LIBRARIES}
    )
endif()

# Install
if(UNIX AND NOT APPLE)
    install(TARGETS ${PACKAGE_NAME} LIBRARY DESTINATION lib/opencpn)
endif()

if(APPLE)
    install(TARGETS ${PACKAGE_NAME} LIBRARY DESTINATION lib/opencpn)
endif()

# Translations: compile .po → .mo and install alongside OpenCPN locale tree
find_package(Gettext)
if(GETTEXT_FOUND)
    file(GLOB PO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/po/*.po")
    set(MO_FILES)
    foreach(PO_FILE ${PO_FILES})
        get_filename_component(LANG ${PO_FILE} NAME_WE)
        set(MO_FILE "${CMAKE_CURRENT_BINARY_DIR}/${LANG}.mo")
        add_custom_command(
            OUTPUT  "${MO_FILE}"
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${MO_FILE}" "${PO_FILE}"
            DEPENDS "${PO_FILE}"
            COMMENT "Compiling translation: ${LANG}"
            VERBATIM
        )
        list(APPEND MO_FILES "${MO_FILE}")
        if(UNIX AND NOT APPLE)
            install(FILES "${MO_FILE}"
                DESTINATION "share/opencpn/locale/${LANG}/LC_MESSAGES"
                RENAME "opencpn-${PACKAGE_NAME}.mo"
            )
        endif()
        if(APPLE)
            install(FILES "${MO_FILE}"
                DESTINATION "share/opencpn/locale/${LANG}/LC_MESSAGES"
                RENAME "opencpn-${PACKAGE_NAME}.mo"
            )
        endif()
    endforeach()
    if(MO_FILES)
        add_custom_target(translations ALL DEPENDS ${MO_FILES})
    endif()

    # Developer target: regenerate .pot template from source
    find_program(XGETTEXT_EXECUTABLE xgettext)
    if(XGETTEXT_EXECUTABLE)
        add_custom_target(update_pot
            COMMAND ${XGETTEXT_EXECUTABLE}
                    --keyword=_
                    --language=C++
                    --add-comments=TRANSLATORS
                    --sort-output
                    --package-name=${PACKAGE_NAME}
                    --output=${CMAKE_CURRENT_SOURCE_DIR}/po/${PACKAGE_NAME}.pot
                    --files-from=${CMAKE_CURRENT_SOURCE_DIR}/po/POTFILES.in
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Updating po/${PACKAGE_NAME}.pot from source"
            VERBATIM
        )
    endif()
endif()
