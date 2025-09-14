cmake_minimum_required(VERSION 3.16)

# Qt Creator installation path
set(CMAKE_PREFIX_PATH "/Users/davec/Developer/Qt/Qt Creator.app/Contents/Frameworks")
list(APPEND CMAKE_PREFIX_PATH "/Users/davec/Developer/Qt/Qt Creator.app/Contents/Resources")
list(APPEND CMAKE_PREFIX_PATH "/Users/davec/Developer/Qt/6.9.2/macos")

# Prevent using system Qt
set(QT_NO_USE_FIND_PACKAGE_Qt6 TRUE)
set(QT_NO_USE_FIND_PACKAGE_Qt5 TRUE)

project(Qt_MCP_Plugin)

# Include centralized versioning
include(version.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

# Add a CMake option that enables building your plugin with tests.
# You don't want your released plugin binaries to contain tests,
# so make that default to 'NO'.
# Enable tests by passing -DWITH_TESTS=ON to CMake.
option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  # Look for QtTest
  find_package(Qt6 REQUIRED COMPONENTS Test)

  # Tell CMake functions like add_qtc_plugin about the QtTest component.
  set(IMPLICIT_DEPENDS Qt::Test)

  # Enable ctest for auto tests.
  enable_testing()
endif()

add_qtc_plugin(Qt_MCP_Plugin
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::ProjectExplorer
  DEPENDS
    Qt::Widgets
    Qt::Network
    QtCreator::ExtensionSystem
    QtCreator::Utils
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    qt_mcp_plugin.cpp
    qt_mcp_pluginconstants.h
    qt_mcp_plugintr.h
    mcpserver.cpp
    mcpserver.h
    mcpcommands.cpp
    mcpcommands.h
)

# Set plugin properties without version in filename
set_target_properties(Qt_MCP_Plugin PROPERTIES
    SOVERSION ${PLUGIN_VERSION_MAJOR}
)

# Generate the plugin JSON file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Qt_MCP_Plugin.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json
    @ONLY
)

# Use the standard Qt Creator plugins directory
set(QTCREATOR_PLUGINS_DIR "$ENV{HOME}/Library/Application Support/QtProject/qtcreator/plugins")
message(STATUS "Using standard Qt Creator plugins directory: ${QTCREATOR_PLUGINS_DIR}")

# Custom target to clean up previous plugin versions
add_custom_target(CleanOldPlugins
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_0.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_1.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_2.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_3.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_4.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_5.dylib"
        "${QTCREATOR_PLUGINS_DIR}/libQt_MCP_Plugin_v1_6.dylib"
        "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin.json"
        "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_0.json"
        "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_3.json"
        "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_4.json"
        "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_5.json"
        "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_6.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_1.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_2.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_3.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_4.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_5.json"
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_6.json"
    COMMENT "Cleaning up previous plugin versions"
)

# Custom target to install the plugin to the external plugins directory
add_custom_target(InstallPlugin
    COMMAND ${CMAKE_COMMAND} -E make_directory "${QTCREATOR_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_BINARY_DIR}/Qt Creator.app/Contents/PlugIns/qtcreator/libQt_MCP_Plugin.dylib"
        "${QTCREATOR_PLUGINS_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json"
        "${QTCREATOR_PLUGINS_DIR}/"
    DEPENDS Qt_MCP_Plugin
    COMMENT "Installing plugin to external Qt Creator plugins directory"
)

# Enable the Run button in Qt Creator
find_program(QtCreatorExecutable
  NAMES
    qtcreator "Qt Creator"
  PATHS
    "/Users/davec/Developer/Qt/Qt Creator.app/Contents/MacOS"
  NO_DEFAULT_PATH
)
if (QtCreatorExecutable)
  add_custom_target(RunQtCreator
    COMMAND ${QtCreatorExecutable} -pluginpath "${QTCREATOR_PLUGINS_DIR}"
    DEPENDS InstallPlugin
  )
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()
