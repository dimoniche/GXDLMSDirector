find_package(Qt6 REQUIRED COMPONENTS Test)

function(add_gxdlms_test test_name test_source)
    add_executable(${test_name} ${test_source} ${ARGN})
    target_link_libraries(${test_name} PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::SerialPort
        Qt6::Concurrent
        Qt6::Xml
        gurux_dlms
    )
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ProjectSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GXDLMSProject.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GXDLMSDevice.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GXDLMSDeviceGurux.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GXDLMSCommunicator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MediaConnection.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AssociationViewParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConformanceHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/TraceFormatter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/VariantConverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ObjectAttributeNames.cpp
)

add_gxdlms_test(test_project_serializer test_project_serializer.cpp ${CORE_SOURCES})

add_gxdlms_test(test_variant_converter test_variant_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/VariantConverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ObjectAttributeNames.cpp
)

add_gxdlms_test(test_association_view_parser test_association_view_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AssociationViewParser.cpp
)

add_gxdlms_test(test_macro_serializer test_macro_serializer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MacroSerializer.cpp
)

add_gxdlms_test(test_trace_formatter test_trace_formatter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/TraceFormatter.cpp
)
