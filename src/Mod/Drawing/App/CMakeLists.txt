# SPDX-License-Identifier: LGPL-2.1-or-later

set(Drawing_LIBS
    FreeCADApp
    Part
    Spreadsheet
    TechDraw
)

set(Drawing_SRCS
    AppDrawing.cpp
    DraftingTools.cpp
    DraftingTools.h
    ProjectionMath.cpp
    ProjectionMath.h
    TitleBlockBinder.cpp
    TitleBlockBinder.h
)

add_library(Drawing SHARED ${Drawing_SRCS})

target_include_directories(
    Drawing
    PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Drawing ${Drawing_LIBS})

if(MSVC)
    target_compile_definitions(Drawing PRIVATE WIN32_LEAN_AND_MEAN)
endif()

target_compile_warn_error(Drawing)

SET_BIN_DIR(Drawing)
SET_PYTHON_PREFIX_SUFFIX(Drawing)

install(TARGETS Drawing DESTINATION ${CMAKE_INSTALL_LIBDIR})
