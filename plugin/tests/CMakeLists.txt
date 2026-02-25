cmake_minimum_required(VERSION 3.10)

# ---- url_builder tests (no wx, no curl) ------------------------------------
add_executable(test_url_builder test_url_builder.cpp)
target_include_directories(test_url_builder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_url_builder PRIVATE cxx_std_14)
add_test(NAME url_builder COMMAND test_url_builder)

# ---- obs_parser tests (wx + wxJSON, no curl) --------------------------------
add_executable(test_obs_parser
    test_obs_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/obs_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/wxJSON/jsonval.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/wxJSON/jsonreader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/wxJSON/jsonwriter.cpp
)
target_include_directories(test_obs_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/wx
    ${OPENCPN_INCLUDE_DIR}
)
target_compile_features(test_obs_parser PRIVATE cxx_std_14)
if(wxWidgets_FOUND)
    target_include_directories(test_obs_parser PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(test_obs_parser PRIVATE ${wxWidgets_DEFINITIONS})
    target_link_libraries(test_obs_parser ${wxWidgets_LIBRARIES})
else()
    target_include_directories(test_obs_parser PRIVATE ${WX_INCLUDE_DIRS})
    target_link_libraries(test_obs_parser ${WX_LIBRARIES})
endif()
add_test(NAME obs_parser COMMAND test_obs_parser)

# ---- gpx_builder tests (wx, no curl) ----------------------------------------
add_executable(test_gpx
    test_gpx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/gpx_builder.cpp
)
target_include_directories(test_gpx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENCPN_INCLUDE_DIR}
)
target_compile_features(test_gpx PRIVATE cxx_std_14)
if(wxWidgets_FOUND)
    target_include_directories(test_gpx PRIVATE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(test_gpx PRIVATE ${wxWidgets_DEFINITIONS})
    target_link_libraries(test_gpx ${wxWidgets_LIBRARIES})
else()
    target_include_directories(test_gpx PRIVATE ${WX_INCLUDE_DIRS})
    target_link_libraries(test_gpx ${WX_LIBRARIES})
endif()
add_test(NAME gpx_builder COMMAND test_gpx)
