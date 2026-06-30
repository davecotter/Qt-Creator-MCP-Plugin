# Qt Creator 6.11+ links QtCreator::Utils against Qt6::TaskTree, but the standalone
# Qt SDK install often omits the qttasktree module. Qt Creator ships Qt6TaskTree.dll
# in its bin directory. Provide the imported target for plugin builds.

if(TARGET Qt6::TaskTree)
    return()
endif()

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
    endforeach()
endfunction()

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

# Headers: qttasktree is often missing from the Qt SDK install but source may exist.
set(_QTC_TASKTREE_SRCDIR "")
foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(_prefix MATCHES "msvc2022_64")
        get_filename_component(_qt_ver_dir "${_prefix}" DIRECTORY)
        get_filename_component(_qt_root "${_qt_ver_dir}" DIRECTORY)
        foreach(_candidate IN ITEMS
            "${_qt_ver_dir}/Src/qttasktree/src/tasktree"
            "${_qt_root}/6.11.1/Src/qttasktree/src/tasktree"
            "${_qt_root}/6.11.0/Src/qttasktree/src/tasktree")
            if(EXISTS "${_candidate}/qtasktree.h")
                set(_QTC_TASKTREE_SRCDIR "${_candidate}")
                break()
            endif()
        endforeach()
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
file(GLOB _QTC_TASKTREE_HEADERS "${_QTC_TASKTREE_SRCDIR}/*.h")
foreach(_QTC_HDR IN LISTS _QTC_TASKTREE_HEADERS)
    get_filename_component(_QTC_HDR_NAME "${_QTC_HDR}" NAME)
    configure_file("${_QTC_HDR}" "${_QTC_TASKTREE_INCLUDE_MODULE}/${_QTC_HDR_NAME}" COPYONLY)
endforeach()
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
file(WRITE "${_QTC_TASKTREE_INCLUDE_MODULE}/QTaskTree"
    "#include \"qtasktree.h\"\n#include \"qtasktreerunner.h\"\n")

set_target_properties(Qt6TaskTree PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_QTC_TASKTREE_INCLUDE_ROOT}"
)
set(QTC_TASKTREE_INCLUDE_DIR "${_QTC_TASKTREE_INCLUDE_ROOT}" CACHE INTERNAL "QtTaskTree include shim")
message(STATUS "Using QtTaskTree headers from: ${_QTC_TASKTREE_SRCDIR}")
