set(GURUX_DLMS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gurux_dlms)

file(GLOB GURUX_DLMS_SOURCES ${GURUX_DLMS_DIR}/development/src/*.cpp)

add_library(gurux_dlms STATIC ${GURUX_DLMS_SOURCES})

target_include_directories(gurux_dlms PUBLIC
    ${GURUX_DLMS_DIR}/development/include
)


if(UNIX AND NOT APPLE)
    target_link_libraries(gurux_dlms PUBLIC pthread)
endif()

if(APPLE)
    target_compile_options(gurux_dlms PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gurux_apple_sockets.h
    )
endif()

if(WIN32)
    target_link_libraries(gurux_dlms PUBLIC ws2_32)
endif()
