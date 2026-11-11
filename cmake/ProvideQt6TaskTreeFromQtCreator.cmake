# Qt Creator links QtCreator::Utils against Qt6::TaskTree, but the standalone
# Qt SDK install often omits the qttasktree module. Provide an imported
# Qt6::TaskTree from the Qt Creator installation (DLL on Windows, framework on macOS).

if(TARGET Qt6::TaskTree)
    return()
endif()

# ---------------------------------------------------------------------------
# Locate Qt Creator install root / binary location
# ---------------------------------------------------------------------------

function(_qtc_find_qtcreator_bin out_var)
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_prefix}/bin/qtcreator.exe")
            set(${out_var} "${_prefix}/bin" PARENT_SCOPE)
            return()
        endif()
        if(EXISTS "${_prefix}/../bin/qtcreator.exe")
            get_filename_component(_bin "${_prefix}/../bin" ABSOLUTE)
            set(${out_var} "${_bin}" PARENT_SCOPE)
            return()
        endif()
        # macOS: CMAKE_PREFIX_PATH is often "Qt Creator.sdk" beside the .app
        if(EXISTS "${_prefix}/../Qt Creator.app/Contents/MacOS/Qt Creator")
            get_filename_component(_bin "${_prefix}/../Qt Creator.app/Contents/MacOS" ABSOLUTE)
            set(${out_var} "${_bin}" PARENT_SCOPE)
            return()
        endif()
        if(EXISTS "${_prefix}/../../MacOS/Qt Creator")
            get_filename_component(_bin "${_prefix}/../../MacOS" ABSOLUTE)
            set(${out_var} "${_bin}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

function(_qtc_find_qtcreator_app_contents out_var)
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_prefix}/../Qt Creator.app/Contents/Frameworks")
            get_filename_component(_contents "${_prefix}/../Qt Creator.app/Contents" ABSOLUTE)
            set(${out_var} "${_contents}" PARENT_SCOPE)
            return()
        endif()
        if(EXISTS "${_prefix}/../../Frameworks")
            get_filename_component(_contents "${_prefix}/../.." ABSOLUTE)
            set(${out_var} "${_contents}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# ---------------------------------------------------------------------------
# Imported library: Windows DLL or macOS framework binary
# ---------------------------------------------------------------------------

if(WIN32)
    _qtc_find_qtcreator_bin(_QTC_QTCREATOR_BIN)
    if(NOT _QTC_QTCREATOR_BIN)
        message(FATAL_ERROR
            "Could not locate Qt Creator bin directory on CMAKE_PREFIX_PATH. "
            "Expected qtcreator.exe under Tools/QtCreator/bin.")
    endif()

    set(_QTC_TASKTREE_DLL "${_QTC_QTCREATOR_BIN}/Qt6TaskTree.dll")
    if(NOT EXISTS "${_QTC_TASKTREE_DLL}")
        message(FATAL_ERROR "Qt6TaskTree.dll not found at: ${_QTC_TASKTREE_DLL}")
    endif()

    set(_QTC_TASKTREE_IMPLIB "${CMAKE_BINARY_DIR}/Qt6TaskTree_import.lib")

    if(NOT EXISTS "${_QTC_TASKTREE_IMPLIB}")
        set(_QTC_DUMPBIN "")
        set(_QTC_LIB "")
        if(DEFINED ENV{VCToolsInstallDir})
            set(_QTC_VC_BIN "$ENV{VCToolsInstallDir}/bin/Hostx64/x64")
            if(EXISTS "${_QTC_VC_BIN}/dumpbin.exe")
                set(_QTC_DUMPBIN "${_QTC_VC_BIN}/dumpbin.exe")
                set(_QTC_LIB "${_QTC_VC_BIN}/lib.exe")
            endif()
        endif()
        if(NOT _QTC_DUMPBIN)
            file(GLOB _QTC_DUMPBIN_CANDIDATES
                "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe")
            list(SORT _QTC_DUMPBIN_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
            if(_QTC_DUMPBIN_CANDIDATES)
                list(GET _QTC_DUMPBIN_CANDIDATES 0 _QTC_DUMPBIN)
                get_filename_component(_QTC_VC_BIN "${_QTC_DUMPBIN}" DIRECTORY)
                set(_QTC_LIB "${_QTC_VC_BIN}/lib.exe")
            endif()
        endif()

        if(NOT _QTC_DUMPBIN OR NOT EXISTS "${_QTC_LIB}")
            message(FATAL_ERROR
                "dumpbin/lib not found. Install Visual Studio C++ build tools to link Qt6TaskTree.")
        endif()

        set(_QTC_TASKTREE_DEF "${CMAKE_BINARY_DIR}/Qt6TaskTree_import.def")
        execute_process(
            COMMAND "${_QTC_DUMPBIN}" /exports "${_QTC_TASKTREE_DLL}"
            OUTPUT_VARIABLE _QTC_DUMPBIN_OUT
            ERROR_VARIABLE _QTC_DUMPBIN_ERR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(REPLACE "\n" ";" _QTC_DUMP_LINES "${_QTC_DUMPBIN_OUT}")
        set(_QTC_EXPORTS "")
        foreach(_QTC_LINE IN LISTS _QTC_DUMP_LINES)
            if(_QTC_LINE MATCHES "^[ \t]+[0-9]+[ \t]+[0-9A-Fa-f]+[ \t]+[0-9A-Fa-f]+[ \t]+([^ \t]+)")
                list(APPEND _QTC_EXPORTS "${CMAKE_MATCH_1}")
            endif()
        endforeach()

        if(NOT _QTC_EXPORTS)
            message(FATAL_ERROR
                "Failed to parse exports from Qt6TaskTree.dll using dumpbin.\n${_QTC_DUMPBIN_ERR}")
        endif()

        file(WRITE "${_QTC_TASKTREE_DEF}" "EXPORTS\n")
        foreach(_QTC_SYM IN LISTS _QTC_EXPORTS)
            file(APPEND "${_QTC_TASKTREE_DEF}" "${_QTC_SYM}\n")
        endforeach()

        execute_process(
            COMMAND "${_QTC_LIB}" /def:${_QTC_TASKTREE_DEF} /machine:x64 /name:Qt6TaskTree.dll
                /out:${_QTC_TASKTREE_IMPLIB}
            RESULT_VARIABLE _QTC_LIB_RESULT
            OUTPUT_VARIABLE _QTC_LIB_OUT
            ERROR_VARIABLE _QTC_LIB_ERR
        )
        if(_QTC_LIB_RESULT OR NOT EXISTS "${_QTC_TASKTREE_IMPLIB}")
            message(FATAL_ERROR
                "Failed to generate import library for Qt6TaskTree.dll:\n${_QTC_LIB_OUT}\n${_QTC_LIB_ERR}")
        endif()
    endif()

    add_library(Qt6TaskTree SHARED IMPORTED GLOBAL)
    add_library(Qt6::TaskTree ALIAS Qt6TaskTree)
    set_target_properties(Qt6TaskTree PROPERTIES
        IMPORTED_LOCATION "${_QTC_TASKTREE_DLL}"
        IMPORTED_IMPLIB "${_QTC_TASKTREE_IMPLIB}"
    )
    message(STATUS "Using Qt6::TaskTree from Qt Creator: ${_QTC_TASKTREE_DLL}")

elseif(APPLE)
    _qtc_find_qtcreator_app_contents(_QTC_APP_CONTENTS)
    if(NOT _QTC_APP_CONTENTS)
        message(FATAL_ERROR
            "Could not locate Qt Creator.app Contents on CMAKE_PREFIX_PATH. "
            "Expected Frameworks/QtTaskTree.framework beside Qt Creator.sdk.")
    endif()

    set(_QTC_TASKTREE_FW "${_QTC_APP_CONTENTS}/Frameworks/QtTaskTree.framework")
    set(_QTC_TASKTREE_LIB "${_QTC_TASKTREE_FW}/QtTaskTree")
    if(NOT EXISTS "${_QTC_TASKTREE_LIB}")
        message(FATAL_ERROR "QtTaskTree.framework not found at: ${_QTC_TASKTREE_FW}")
    endif()

    add_library(Qt6TaskTree SHARED IMPORTED GLOBAL)
    add_library(Qt6::TaskTree ALIAS Qt6TaskTree)
    # Do not set FRAMEWORK TRUE: clang would prefer the framework's (empty)
    # Headers over our INTERFACE_INCLUDE_DIRECTORIES shim.
    set_target_properties(Qt6TaskTree PROPERTIES
        IMPORTED_LOCATION "${_QTC_TASKTREE_LIB}"
    )
    message(STATUS "Using Qt6::TaskTree from Qt Creator: ${_QTC_TASKTREE_FW}")

else()
    message(FATAL_ERROR
        "Qt6::TaskTree is missing from the Qt SDK and no shim is available on this platform.")
endif()

# ---------------------------------------------------------------------------
# Headers: prefer Qt SDK Src (correct QtTaskTree:: namespace)
# ---------------------------------------------------------------------------

set(_QTC_TASKTREE_SRCDIR "")
set(_QTC_TASKTREE_HEADER_CANDIDATES "")

foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
    # Windows: .../6.11.x/msvc2022_64
    # macOS SDK: .../6.11.x/macos
    if(_prefix MATCHES "msvc2022_64" OR _prefix MATCHES "/macos$" OR _prefix MATCHES "\\\\macos$")
        get_filename_component(_qt_ver_dir "${_prefix}" DIRECTORY)
        get_filename_component(_qt_root "${_qt_ver_dir}" DIRECTORY)
        list(APPEND _QTC_TASKTREE_HEADER_CANDIDATES
            "${_qt_ver_dir}/Src/qttasktree/src/tasktree"
            "${_prefix}/../Src/qttasktree/src/tasktree"
            "${_qt_root}/6.11.2/Src/qttasktree/src/tasktree"
            "${_qt_root}/6.11.1/Src/qttasktree/src/tasktree"
            "${_qt_root}/6.11.0/Src/qttasktree/src/tasktree")
    endif()
    # Qt Creator.sdk / Qt root
    get_filename_component(_qt_root "${_prefix}/.." ABSOLUTE)
    list(APPEND _QTC_TASKTREE_HEADER_CANDIDATES
        "${_qt_root}/6.11.2/Src/qttasktree/src/tasktree"
        "${_qt_root}/6.11.1/Src/qttasktree/src/tasktree"
        "${_qt_root}/6.11.0/Src/qttasktree/src/tasktree")
endforeach()

# Also search via Qt6_DIR if set (.../6.11.2/macos/lib/cmake/Qt6)
if(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}")
    get_filename_component(_qt6_cmake "${Qt6_DIR}" DIRECTORY)
    get_filename_component(_qt6_lib "${_qt6_cmake}" DIRECTORY)
    get_filename_component(_qt6_macos "${_qt6_lib}" DIRECTORY)
    get_filename_component(_qt6_ver "${_qt6_macos}" DIRECTORY)
    get_filename_component(_qt6_root "${_qt6_ver}" DIRECTORY)
    list(APPEND _QTC_TASKTREE_HEADER_CANDIDATES
        "${_qt6_ver}/Src/qttasktree/src/tasktree"
        "${_qt6_root}/6.11.2/Src/qttasktree/src/tasktree"
        "${_qt6_root}/6.11.1/Src/qttasktree/src/tasktree"
        "${_qt6_root}/6.11.0/Src/qttasktree/src/tasktree")
endif()

# Last resort: Creator-bundled headers (may have namespace issues)
if(APPLE AND _QTC_APP_CONTENTS)
    list(APPEND _QTC_TASKTREE_HEADER_CANDIDATES
        "${_QTC_APP_CONTENTS}/Resources/Headers/qtcreator/src/libs/QtTaskTree/QtTaskTree")
endif()

foreach(_candidate IN LISTS _QTC_TASKTREE_HEADER_CANDIDATES)
    if(EXISTS "${_candidate}/qtasktree.h")
        set(_QTC_TASKTREE_SRCDIR "${_candidate}")
        break()
    endif()
endforeach()

if(NOT _QTC_TASKTREE_SRCDIR)
    message(FATAL_ERROR
        "Qt TaskTree headers not found. Install the qttasktree module for your Qt SDK, "
        "or install Qt source containing 6.11.x/Src/qttasktree.")
endif()

set(_QTC_TASKTREE_INCLUDE_ROOT "${CMAKE_BINARY_DIR}/qttasktree_include")
set(_QTC_TASKTREE_INCLUDE_MODULE "${_QTC_TASKTREE_INCLUDE_ROOT}/QtTaskTree")
file(MAKE_DIRECTORY "${_QTC_TASKTREE_INCLUDE_MODULE}")

# Copy *.h from preferred source (Src has correct namespace)
file(GLOB _QTC_TASKTREE_HEADERS "${_QTC_TASKTREE_SRCDIR}/*.h")
foreach(_QTC_HDR IN LISTS _QTC_TASKTREE_HEADERS)
    get_filename_component(_QTC_HDR_NAME "${_QTC_HDR}" NAME)
    configure_file("${_QTC_HDR}" "${_QTC_TASKTREE_INCLUDE_MODULE}/${_QTC_HDR_NAME}" COPYONLY)
endforeach()

# Extension-less Qt-style wrappers from Creator (QSingleTaskTreeRunner, ...)
set(_QTC_WRAPPER_SRC "")
if(APPLE AND _QTC_APP_CONTENTS)
    set(_cand "${_QTC_APP_CONTENTS}/Resources/Headers/qtcreator/src/libs/QtTaskTree/QtTaskTree")
    if(EXISTS "${_cand}")
        set(_QTC_WRAPPER_SRC "${_cand}")
    endif()
endif()
if(_QTC_WRAPPER_SRC)
    file(GLOB _QTC_TASKTREE_WRAPPERS LIST_DIRECTORIES false "${_QTC_WRAPPER_SRC}/Q*")
    foreach(_QTC_WRAP IN LISTS _QTC_TASKTREE_WRAPPERS)
        if(IS_DIRECTORY "${_QTC_WRAP}")
            continue()
        endif()
        get_filename_component(_QTC_WRAP_NAME "${_QTC_WRAP}" NAME)
        if(_QTC_WRAP_NAME MATCHES "\\.")
            continue()
        endif()
        configure_file("${_QTC_WRAP}" "${_QTC_TASKTREE_INCLUDE_MODULE}/${_QTC_WRAP_NAME}" COPYONLY)
    endforeach()
endif()

file(WRITE "${_QTC_TASKTREE_INCLUDE_MODULE}/qttasktreeexports.h"
    "#ifndef QTASKTREE_QTTASKTREEEXPORTS_H\n"
    "#define QTASKTREE_QTTASKTREEEXPORTS_H\n"
    "#include <QtCore/qglobal.h>\n"
    "#ifndef Q_TASKTREE_EXPORT\n"
    "#  define Q_TASKTREE_EXPORT Q_DECL_IMPORT\n"
    "#endif\n"
    "#ifndef QT_TASKTREE_EXPORT\n"
    "#  define QT_TASKTREE_EXPORT Q_DECL_IMPORT\n"
    "#endif\n"
    "#endif\n")

# Ensure a few essential wrappers exist even without Creator header pack
macro(_qtc_ensure_wrapper _name _include)
    if(NOT EXISTS "${_QTC_TASKTREE_INCLUDE_MODULE}/${_name}")
        file(WRITE "${_QTC_TASKTREE_INCLUDE_MODULE}/${_name}" "#include <QtTaskTree/${_include}>\n")
    endif()
endmacro()
_qtc_ensure_wrapper(QTaskTree qtasktree.h)
_qtc_ensure_wrapper(QSingleTaskTreeRunner qtasktreerunner.h)
_qtc_ensure_wrapper(QSequentialTaskTreeRunner qtasktreerunner.h)
_qtc_ensure_wrapper(QParallelTaskTreeRunner qtasktreerunner.h)
_qtc_ensure_wrapper(QMappedTaskTreeRunner qtasktreerunner.h)
_qtc_ensure_wrapper(QAbstractTaskTreeRunner qtasktreerunner.h)

set_target_properties(Qt6TaskTree PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_QTC_TASKTREE_INCLUDE_ROOT}"
)
set(QTC_TASKTREE_INCLUDE_DIR "${_QTC_TASKTREE_INCLUDE_ROOT}" CACHE INTERNAL "QtTaskTree include shim")
message(STATUS "Using QtTaskTree headers from: ${_QTC_TASKTREE_SRCDIR}")
