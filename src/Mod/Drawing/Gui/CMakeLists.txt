# SPDX-License-Identifier: LGPL-2.1-or-later

set(DrawingGui_LIBS
    Drawing
    TechDraw
    TechDrawGui
    Part
    FreeCADGui
)

qt_add_resources(DrawingGui_RCC_SRCS
    Resources/Drawing.qrc
    OPTIONS ${FREECAD_RCC_OPTIONS}
)

set(DrawingGui_SRCS
    ${DrawingGui_RCC_SRCS}
    AppDrawingGui.cpp
    CommandDrawing.cpp
    DimensionPalettePopup.cpp
    DimensionPalettePopup.h
    DrawingUpdateController.cpp
    DrawingUpdateController.h
    HandlerAuxiliaryView.cpp
    HandlerAuxiliaryView.h
    HandlerBreakView.cpp
    HandlerBreakView.h
    HandlerCropView.cpp
    HandlerCropView.h
    HandlerDetailView.cpp
    HandlerDetailView.h
    HandlerInsertView.cpp
    HandlerInsertView.h
    HandlerProjectedView.cpp
    HandlerProjectedView.h
    HandlerSectionView.cpp
    HandlerSectionView.h
    PreCompiled.h
    SheetsViewsPanel.cpp
    SheetsViewsPanel.h
    TaskGdtDialog.cpp
    TaskGdtDialog.h
    TaskSheetSetup.cpp
    TaskSheetSetup.h
    ViewPaletteDock.cpp
    ViewPaletteDock.h
    Workbench.cpp
    Workbench.h
    WorkbenchManipulator.cpp
    WorkbenchManipulator.h
)

set(DrawingGuiIcon_SVG
    Resources/icons/DrawingWorkbench.svg
)

add_library(DrawingGui SHARED
    ${DrawingGui_SRCS}
    ${DrawingGuiIcon_SVG}
)

if(FREECAD_USE_PCH)
    target_precompile_headers(DrawingGui PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:"${CMAKE_CURRENT_LIST_DIR}/PreCompiled.h">
    )
endif()

target_include_directories(
    DrawingGui
    PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Mod/TechDraw/Gui
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(DrawingGui ${DrawingGui_LIBS})

if(MSVC)
    target_compile_definitions(DrawingGui PRIVATE WIN32_LEAN_AND_MEAN)
endif()

target_compile_warn_error(DrawingGui)

fc_copy_sources(DrawingGui "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/Mod/Drawing/" ${DrawingGuiIcon_SVG})
install(FILES ${DrawingGuiIcon_SVG} DESTINATION "${CMAKE_INSTALL_DATADIR}/Mod/Drawing/Resources/icons")

SET_BIN_DIR(DrawingGui)
SET_PYTHON_PREFIX_SUFFIX(DrawingGui)

install(TARGETS DrawingGui DESTINATION ${CMAKE_INSTALL_LIBDIR})
