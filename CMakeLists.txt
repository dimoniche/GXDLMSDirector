cmake_minimum_required(VERSION 3.16)

project(GXDLMSDirector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialPort Network Concurrent Xml)

include(cmake/GuruxDLMS.cmake)

set(SOURCES
    src/main.cpp
    src/core/MediaConnection.cpp
    src/core/VariantConverter.cpp
    src/core/ManufacturerSettings.cpp
    src/core/ProjectSerializer.cpp
    src/core/HdlcAddressScanner.cpp
    src/core/PlcDiscoverer.cpp
    src/core/MacroSerializer.cpp
    src/core/MacroRunner.cpp
    src/core/ConformanceHelper.cpp
    src/core/DataConcentratorManager.cpp
    src/core/ObjectAttributeNames.cpp
    src/core/CosemObjectHelper.cpp
    src/core/GXDLMSProject.cpp
    src/core/RecentFilesManager.cpp
    src/core/ValuesSerializer.cpp
    src/core/TraceFormatter.cpp
    src/core/AssociationViewParser.cpp
    src/core/GXDLMSCommunicator.cpp
    src/core/GXDLMSDevice.cpp
    src/core/GXDLMSDeviceGurux.cpp
    src/ui/MainWindow.cpp
    src/ui/DevicePropertiesDialog.cpp
    src/ui/ProfileGenericDialog.cpp
    src/ui/DlmsTranslatorDialog.cpp
    src/ui/HdlcAddressScannerDialog.cpp
    src/ui/MacroEditorDialog.cpp
    src/ui/ConformanceTestDialog.cpp
    src/ui/PlcDiscoverDialog.cpp
    src/ui/CosemObjectDialog.cpp
    src/ui/ByteArrayEditorDialog.cpp
    src/ui/PropertyTableDelegate.cpp
    src/ui/FindObjectDialog.cpp
)

set(HEADERS
    src/core/DeviceState.h
    src/core/MediaConnection.h
    src/core/GXDLMSCommunicator.h
    src/core/GXDLMSDevice.h
    src/core/ReadResult.h
    src/core/VariantConverter.h
    src/core/ManufacturerSettings.h
    src/core/ProjectSerializer.h
    src/core/ProfileGenericResult.h
    src/core/HdlcAddressScanner.h
    src/core/PlcDiscoverer.h
    src/core/MacroStep.h
    src/core/MacroSerializer.h
    src/core/MacroRunner.h
    src/core/ConformanceHelper.h
    src/core/DataConcentratorManager.h
    src/core/ObjectAttributeNames.h
    src/core/CosemObjectHelper.h
    src/core/GXDLMSProject.h
    src/core/RecentFilesManager.h
    src/core/ValuesSerializer.h
    src/core/TraceFormatter.h
    src/core/AssociationViewParser.h
    src/ui/MainWindow.h
    src/ui/DevicePropertiesDialog.h
    src/ui/ProfileGenericDialog.h
    src/ui/DlmsTranslatorDialog.h
    src/ui/HdlcAddressScannerDialog.h
    src/ui/MacroEditorDialog.h
    src/ui/ConformanceTestDialog.h
    src/ui/PlcDiscoverDialog.h
    src/ui/CosemObjectDialog.h
    src/ui/ByteArrayEditorDialog.h
    src/ui/PropertyTableDelegate.h
    src/ui/FindObjectDialog.h
)

set(UI_FILES
    src/ui/MainWindow.ui
    src/ui/DevicePropertiesDialog.ui
    src/ui/ProfileGenericDialog.ui
    src/ui/DlmsTranslatorDialog.ui
    src/ui/HdlcAddressScannerDialog.ui
    src/ui/MacroEditorDialog.ui
    src/ui/ConformanceTestDialog.ui
    src/ui/PlcDiscoverDialog.ui
    src/ui/CosemObjectDialog.ui
    src/ui/FindObjectDialog.ui
)

add_executable(GXDLMSDirector ${SOURCES} ${HEADERS} ${UI_FILES})

target_link_libraries(GXDLMSDirector PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
    Qt6::Concurrent
    Qt6::Xml
    gurux_dlms
)

target_include_directories(GXDLMSDirector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(APPLE)
    set_target_properties(GXDLMSDirector PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER fi.gurux.GXDLMSDirector
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
    install(TARGETS GXDLMSDirector BUNDLE DESTINATION .)
else()
    install(TARGETS GXDLMSDirector RUNTIME DESTINATION bin)
endif()
