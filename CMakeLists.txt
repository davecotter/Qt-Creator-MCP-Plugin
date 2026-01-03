cmake_minimum_required(VERSION 3.16)

# IMPORTANT: This plugin ONLY builds in Release mode
# Debug builds are not supported and will cause runtime issues
# Always use: cmake --build . --config Release

# Library path configuration will be set after target definition

# Qt Creator installation path - platform specific
if(WIN32)
    # Windows paths - user should set these via CMAKE_PREFIX_PATH
    # Example: cmake -DCMAKE_PREFIX_PATH="C:/Qt/Qt Creator/6.9.2/msvc2022_64"
elseif(APPLE)
    # macOS paths - user should set these via CMAKE_PREFIX_PATH  
    # Example: cmake -DCMAKE_PREFIX_PATH="/Users/username/Qt/Qt Creator.app/Contents/Resources"
else()
    # Linux paths - user should set these via CMAKE_PREFIX_PATH
    # Example: cmake -DCMAKE_PREFIX_PATH="/opt/qtcreator"
endif()

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
    issuesmanager.cpp
    issuesmanager.h
    httpparser.cpp
    httpparser.h
    httpresponse.cpp
    httpresponse.h
    macos_menu_icon.h
    mcp.png
    mcp.qrc
)

# Add macOS-specific Objective-C++ source for native menu icons
if(APPLE)
    target_sources(Qt_MCP_Plugin PRIVATE macos_menu_icon.mm)
    # Link against AppKit framework for NSMenu/NSMenuItem access
    target_link_libraries(Qt_MCP_Plugin PRIVATE "-framework AppKit")
endif()

# Set plugin properties without version in filename
set_target_properties(Qt_MCP_Plugin PROPERTIES
    SOVERSION ${PLUGIN_VERSION_MAJOR}
)

# Configure library paths to use Qt Creator's bin directory
# This allows the plugin to link directly to Qt libraries without copying DLLs
if(WIN32)
    # Set RPATH to Qt Creator bin directory for Windows
    set_target_properties(Qt_MCP_Plugin PROPERTIES
        INSTALL_RPATH "../bin"
        BUILD_WITH_INSTALL_RPATH TRUE
        # Force linking against Qt Creator's Qt libraries
        LINK_FLAGS "/LIBPATH:${CMAKE_PREFIX_PATH}/bin"
    )
endif()

# Generate the plugin JSON file with version substitution
# Configure the JSON file with version replacement
# Read the template and replace version placeholder
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/Qt_MCP_Plugin.json.in JSON_CONTENT)
string(REPLACE "@PLUGIN_VERSION@" "${PLUGIN_VERSION}" JSON_CONTENT "${JSON_CONTENT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json "${JSON_CONTENT}")

# Generate version.h from version.cmake variables
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

# Generate the MCP discovery file with version substitution
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Qt_MCP_Plugin_discovery.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json
    @ONLY
)

# Create a custom target to ensure the JSON files are always generated
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/Qt_MCP_Plugin.json.in
        ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json
    COMMAND ${CMAKE_COMMAND} -E env sed -i '' 's/@PLUGIN_VERSION@/${PLUGIN_VERSION}/g' ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/Qt_MCP_Plugin_discovery.json.in
        ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json
    COMMAND ${CMAKE_COMMAND} -E env sed -i '' 's/@PLUGIN_VERSION@/${PLUGIN_VERSION}/g' ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json
    DEPENDS Qt_MCP_Plugin.json.in Qt_MCP_Plugin_discovery.json.in
    COMMENT "Generating plugin JSON and discovery files with version ${PLUGIN_VERSION}"
)

# Use the standard Qt Creator plugins directory - platform specific
if(WIN32)
    # Windows: Use AppData\Local\QtProject\qtcreator\plugins
    set(QTCREATOR_PLUGINS_DIR "$ENV{LOCALAPPDATA}/QtProject/qtcreator/plugins")
elseif(APPLE)
    # macOS: Use ~/Library/Application Support/QtProject/qtcreator/plugins
    set(QTCREATOR_PLUGINS_DIR "$ENV{HOME}/Library/Application Support/QtProject/qtcreator/plugins")
else()
    # Linux: Use ~/.local/share/data/QtProject/qtcreator/plugins
    set(QTCREATOR_PLUGINS_DIR "$ENV{HOME}/.local/share/data/QtProject/qtcreator/plugins")
endif()
message(STATUS "Using standard Qt Creator plugins directory: ${QTCREATOR_PLUGINS_DIR}")

# Custom target to clean up previous plugin versions - platform specific
if(WIN32)
    add_custom_target(CleanOldPlugins
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_0.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_1.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_2.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_3.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_4.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_5.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_6.dll"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_discovery.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_0.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_3.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_4.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_5.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_6.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_1.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_2.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_3.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_4.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_5.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_6.json"
        COMMENT "Cleaning up previous plugin versions (Windows)"
    )
else()
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
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_discovery.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_0.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_3.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_4.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_5.json"
            "${QTCREATOR_PLUGINS_DIR}/Qt_MCP_Plugin_v1_6.json"
            "/Users/davec/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator/libQt_MCP_Plugin.dylib"
            "/Users/davec/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator/Qt_MCP_Plugin.json"
            "/Users/davec/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator/Qt_MCP_Plugin_discovery.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_1.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_2.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_3.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_4.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_5.json"
            "${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_v1_6.json"
        COMMENT "Cleaning up previous plugin versions (macOS/Linux)"
    )
endif()

# Custom target to install the plugin using the complete lifecycle script
add_custom_target(InstallPlugin
    COMMAND ${CMAKE_COMMAND} -E echo "🚀 Running complete plugin lifecycle with build.py..."
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR} python3 ${CMAKE_CURRENT_SOURCE_DIR}/build.py
    
    DEPENDS Qt_MCP_Plugin ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin.json ${CMAKE_CURRENT_BINARY_DIR}/Qt_MCP_Plugin_discovery.json
    COMMENT "Installing plugin using complete lifecycle management (build.py)"
)

# Enable the Run button in Qt Creator - platform specific
if(WIN32)
    find_program(QtCreatorExecutable
      NAMES
        qtcreator.exe "Qt Creator.exe"
      PATHS
        "C:/Qt/Qt Creator/6.9.2/msvc2022_64/bin"
        "C:/Program Files/Qt/Qt Creator/6.9.2/msvc2022_64/bin"
        "C:/Qt/Qt Creator/6.9.2/mingw_64/bin"
        "C:/Program Files/Qt/Qt Creator/6.9.2/mingw_64/bin"
      NO_DEFAULT_PATH
    )
elseif(APPLE)
    find_program(QtCreatorExecutable
      NAMES
        qtcreator "Qt Creator"
      PATHS
        "/Users/davec/Developer/Qt/Qt Creator.app/Contents/MacOS"
        "/Applications/Qt Creator.app/Contents/MacOS"
      NO_DEFAULT_PATH
    )
else()
    find_program(QtCreatorExecutable
      NAMES
        qtcreator
      PATHS
        "/opt/qtcreator/bin"
        "/usr/local/qtcreator/bin"
        "/usr/bin"
      NO_DEFAULT_PATH
    )
endif()

if (QtCreatorExecutable)
  if(WIN32)
    add_custom_target(RunQtCreator
      COMMAND ${CMAKE_COMMAND} -E echo "Killing existing Qt Creator instances..."
      COMMAND taskkill /F /IM "Qt Creator.exe" 2>NUL || echo "No Qt Creator instances to kill"
      COMMAND ${CMAKE_COMMAND} -E echo "Starting Qt Creator with MCP Plugin..."
      COMMAND ${QtCreatorExecutable} -pluginpath "${QTCREATOR_PLUGINS_DIR}"
      DEPENDS InstallPlugin
    )
  else()
    add_custom_target(RunQtCreator
      COMMAND ${CMAKE_COMMAND} -E echo "Killing existing Qt Creator instances..."
      COMMAND pkill -f "Qt Creator" || echo "No Qt Creator instances to kill"
      COMMAND ${CMAKE_COMMAND} -E echo "Cleaning up previous plugin versions..."
      COMMAND ${CMAKE_COMMAND} --build . --target CleanOldPlugins
      COMMAND ${CMAKE_COMMAND} -E echo "Building and installing latest plugin..."
      COMMAND ${CMAKE_COMMAND} --build . --target InstallPlugin
      COMMAND ${CMAKE_COMMAND} -E echo "Waiting for processes to terminate..."
      COMMAND sleep 2
      COMMAND ${CMAKE_COMMAND} -E echo "Starting Qt Creator with MCP Plugin..."
      COMMAND ${QtCreatorExecutable} -pluginpath "${QTCREATOR_PLUGINS_DIR}"
      DEPENDS InstallPlugin
    )
  endif()
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()
