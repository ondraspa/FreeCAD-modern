// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   Copyright (c) 2026 FreeCAD Contributors                               *
 *                                                                         *
 *   This file is part of FreeCAD.                                         *
 *                                                                         *
 *   FreeCAD is free software: you can redistribute it and/or modify it    *
 *   under the terms of the GNU Lesser General Public License as           *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful, but        *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with FreeCAD. If not, see                               *
 *   <https://www.gnu.org/licenses/>.                                      *
 *                                                                         *
 ***************************************************************************/

#include <QDockWidget>

#include <Gui/CommandRibbon/CommandRibbonRegistry.h>
#include <Gui/DockWindowManager.h>
#include <Gui/MainWindow.h>
#include <Gui/MenuManager.h>
#include <Gui/ToolBarManager.h>
#include <Gui/ViewportOverlays/ContextActionRegistry.h>
#include <Gui/ViewportOverlays/HeadsUpViewBar.h>
#include <Gui/ViewportOverlays/ViewportOverlayController.h>

#include "DrawingUpdateController.h"
#include "SheetsViewsPanel.h"
#include "ViewPaletteDock.h"
#include "Workbench.h"

namespace DrawingGui
{

TYPESYSTEM_SOURCE(DrawingGui::Workbench, Gui::StdWorkbench)

Workbench::Workbench() = default;

Workbench::~Workbench() = default;

void Workbench::registerModernUiIntegrations()
{
    auto& ribbon = Gui::CommandRibbonRegistry::instance();
    ribbon.ensureDefaultsRegistered();

    ribbon.registerTab(Gui::RibbonTabDef {
        .id = "drawing_view_layout",
        .title = "View Layout",
        .workbenchName = "DrawingWorkbench",
        .groups = {
            Gui::RibbonGroupDef {
                .id = "drawing_sheet_ops",
                .title = "Sheet",
                .commands = {
                    {"Drawing_NewSheet", Gui::RibbonButtonSize::Large},
                    {"Drawing_UpdateDrawing", Gui::RibbonButtonSize::Large},
                    {"Drawing_SheetProperties", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_ToggleFrame", Gui::RibbonButtonSize::Compact},
                },
            },
            Gui::RibbonGroupDef {
                .id = "drawing_views",
                .title = "Views",
                .commands = {
                    {"Drawing_InsertView", Gui::RibbonButtonSize::Large},
                    {"Drawing_ProjectedView", Gui::RibbonButtonSize::Large},
                    {"Drawing_AuxiliaryView", Gui::RibbonButtonSize::Large},
                    {"Drawing_SectionView", Gui::RibbonButtonSize::Large},
                    {"Drawing_DetailView", Gui::RibbonButtonSize::Large},
                    {"Drawing_BreakView", Gui::RibbonButtonSize::Compact},
                    {"Drawing_CropView", Gui::RibbonButtonSize::Compact},
                },
            },
            Gui::RibbonGroupDef {
                .id = "drawing_align",
                .title = "Alignment & Display",
                .commands = {
                    {"Drawing_ToggleShadedView", Gui::RibbonButtonSize::Compact},
                    {"Drawing_BreakAlignment", Gui::RibbonButtonSize::Compact},
                    {"Drawing_RestoreAlignment", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_ExtensionLockUnlockView", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_ShowAll", Gui::RibbonButtonSize::Compact},
                },
            },
        },
    });

    ribbon.registerTab(Gui::RibbonTabDef {
        .id = "drawing_annotation",
        .title = "Annotation",
        .workbenchName = "DrawingWorkbench",
        .groups = {
            Gui::RibbonGroupDef {
                .id = "drawing_dims",
                .title = "Dimensions",
                .commands = {
                    {"Drawing_SmartDimension", Gui::RibbonButtonSize::Large},
                    {"Drawing_DimensionPalette", Gui::RibbonButtonSize::Large},
                    {"Drawing_HoleCallout", Gui::RibbonButtonSize::Large},
                    {"TechDraw_HorizontalDimension", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_VerticalDimension", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_DiameterDimension", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_RadiusDimension", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_AngleDimension", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_DimensionRepair", Gui::RibbonButtonSize::Compact},
                },
            },
            Gui::RibbonGroupDef {
                .id = "drawing_centerlines",
                .title = "Centerlines",
                .commands = {
                    {"Drawing_AutoCentermarks", Gui::RibbonButtonSize::Large},
                    {"TechDraw_CenterLineGroup", Gui::RibbonButtonSize::Large},
                    {"TechDraw_ExtensionCircleCenterLines", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_ExtensionHoleCircle", Gui::RibbonButtonSize::Compact},
                },
            },
            Gui::RibbonGroupDef {
                .id = "drawing_callouts",
                .title = "GD&T, Balloons & Notes",
                .commands = {
                    {"Drawing_GdtFrame", Gui::RibbonButtonSize::Large},
                    {"Drawing_DatumSymbol", Gui::RibbonButtonSize::Large},
                    {"Drawing_AutoBalloon", Gui::RibbonButtonSize::Large},
                    {"TechDraw_Balloon", Gui::RibbonButtonSize::Compact},
                    {"Drawing_MagneticLine", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_RichTextAnnotation", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_LeaderLine", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_SurfaceFinishSymbols", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_WeldSymbol", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_Hatch", Gui::RibbonButtonSize::Compact},
                },
            },
        },
    });

    ribbon.registerTab(Gui::RibbonTabDef {
        .id = "drawing_cosmetic",
        .title = "Sketch / Cosmetic",
        .workbenchName = "DrawingWorkbench",
        .groups = {
            Gui::RibbonGroupDef {
                .id = "drawing_cosmetic_geom",
                .title = "Cosmetic Geometry",
                .commands = {
                    {"TechDraw_2PointCosmeticLine", Gui::RibbonButtonSize::Large},
                    {"TechDraw_CosmeticCircle", Gui::RibbonButtonSize::Large},
                    {"TechDraw_CosmeticVertex", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_Midpoints", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_Quadrants", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_CosmeticEraser", Gui::RibbonButtonSize::Compact},
                },
            },
            Gui::RibbonGroupDef {
                .id = "drawing_line_style",
                .title = "Line Style",
                .commands = {
                    {"TechDraw_DecorateLine", Gui::RibbonButtonSize::Large},
                    {"TechDraw_ExtensionExtendLine", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_ExtensionShortenLine", Gui::RibbonButtonSize::Compact},
                },
            },
        },
    });

    ribbon.registerTab(Gui::RibbonTabDef {
        .id = "drawing_export_sheet",
        .title = "Tables & Export",
        .workbenchName = "DrawingWorkbench",
        .groups = {
            Gui::RibbonGroupDef {
                .id = "drawing_tables",
                .title = "Tables & BOM",
                .commands = {
                    {"Drawing_BomTable", Gui::RibbonButtonSize::Large},
                    {"Drawing_AutoBalloon", Gui::RibbonButtonSize::Large},
                    {"Drawing_MagneticLine", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_SpreadsheetView", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_Symbol", Gui::RibbonButtonSize::Compact},
                    {"TechDraw_Image", Gui::RibbonButtonSize::Compact},
                },
            },
            Gui::RibbonGroupDef {
                .id = "drawing_export",
                .title = "Export",
                .commands = {
                    {"Drawing_ExportPDF", Gui::RibbonButtonSize::Large},
                    {"Drawing_ExportAllPDF", Gui::RibbonButtonSize::Large},
                    {"Drawing_ExportDXF", Gui::RibbonButtonSize::Large},
                    {"Drawing_ExportSVG", Gui::RibbonButtonSize::Large},
                    {"TechDraw_PrintAll", Gui::RibbonButtonSize::Compact},
                },
            },
        },
    });

    ribbon.registerContext(Gui::RibbonContextDef {
        .id = "drawing_modern",
        .matchWorkbench = "DrawingWorkbench",
        .tabIds = {
            "drawing_view_layout",
            "drawing_annotation",
            "drawing_cosmetic",
            "drawing_export_sheet",
        },
        .defaultTabId = "drawing_view_layout",
    });

    auto& ctxReg = Gui::ContextActionRegistry::instance();
    ctxReg.ensureDefaultsRegistered();

    ctxReg.registerContextRule(Gui::ContextActionRule {
        .id = "drawing_view_part",
        .selectionFilter = "SELECT TechDraw::DrawViewPart COUNT 1",
        .workbenchFilter = "",
        .priority = 40,
        .commands = {
            "Drawing_ProjectedView",
            "Drawing_AuxiliaryView",
            "Drawing_SectionView",
            "Drawing_DetailView",
            "Drawing_BreakView",
            "Drawing_CropView",
            "Drawing_SmartDimension",
            "Drawing_HoleCallout",
            "Drawing_AutoBalloon",
            "Drawing_ToggleShadedView",
            "Drawing_BreakAlignment",
            "Drawing_RestoreAlignment",
        },
    });

    ctxReg.registerContextRule(Gui::ContextActionRule {
        .id = "drawing_dimension",
        .selectionFilter = "SELECT TechDraw::DrawViewDimension COUNT 1",
        .workbenchFilter = "",
        .priority = 45,
        .commands = {
            "Drawing_DimensionPalette",
            "TechDraw_ExtensionIncreaseDecimal",
            "TechDraw_ExtensionDecreaseDecimal",
            "TechDraw_DimensionRepair",
        },
    });

    ctxReg.registerShortcutBar(Gui::ShortcutBarContext {
        .contextId = "Drawing",
        .commands = {
            "Drawing_InsertView",
            "Drawing_ProjectedView",
            "Drawing_SectionView",
            "Drawing_DetailView",
            "Drawing_SmartDimension",
            "Drawing_HoleCallout",
            "Drawing_GdtFrame",
            "Drawing_AutoBalloon",
        },
    });

    if (auto* mw = Gui::getMainWindow()) {
        if (auto* overlays = mw->viewportOverlayController()) {
            if (auto* headsUp = overlays->headsUpViewBar()) {
                headsUp->setDrawingCommands({
                    "Std_ViewFitAll",
                    "Separator",
                    "TechDraw_ToggleFrame",
                    "Drawing_ToggleShadedView",
                    "Drawing_UpdateDrawing",
                });
            }
        }
    }
}

void Workbench::activated()
{
    Gui::StdWorkbench::activated();
    DrawingUpdateController::instance()->install();

    auto* dockMgr = Gui::DockWindowManager::instance();
    if (!m_sheetsPanel) {
        m_sheetsPanel = new SheetsViewsPanel(Gui::getMainWindow());
    }
    if (!dockMgr->getDockWindow("DrawingSheetsViewsPanel")) {
        dockMgr->addDockWindow("DrawingSheetsViewsPanel", m_sheetsPanel, Qt::LeftDockWidgetArea);
    }
    if (QDockWidget* dw = dockMgr->getDockContainer("DrawingSheetsViewsPanel")) {
        dw->setWindowTitle(QObject::tr("Sheets & Views"));
        dw->show();
    }
    m_sheetsPanel->rebuildTree();

    if (!m_viewPalette) {
        m_viewPalette = new ViewPaletteDock(Gui::getMainWindow());
    }
    if (!dockMgr->getDockWindow("DrawingViewPaletteDock")) {
        dockMgr->addDockWindow("DrawingViewPaletteDock", m_viewPalette, Qt::RightDockWidgetArea);
    }
    if (QDockWidget* dw = dockMgr->getDockContainer("DrawingViewPaletteDock")) {
        dw->setWindowTitle(QObject::tr("View Palette"));
        dw->show();
    }
    m_viewPalette->refreshSources();
}

void Workbench::deactivated()
{
    auto* dockMgr = Gui::DockWindowManager::instance();
    if (QDockWidget* dw = dockMgr->getDockContainer("DrawingSheetsViewsPanel")) {
        dw->hide();
    }
    if (QDockWidget* dw = dockMgr->getDockContainer("DrawingViewPaletteDock")) {
        dw->hide();
    }
    Gui::StdWorkbench::deactivated();
}

Gui::MenuItem* Workbench::setupMenuBar() const
{
    Gui::MenuItem* root = StdWorkbench::setupMenuBar();
    Gui::MenuItem* item = root->findItem("&Windows");

    auto* drawMenu = new Gui::MenuItem;
    drawMenu->setCommand("&Drawing");
    *drawMenu << "Drawing_NewSheet"
              << "Drawing_SheetProperties"
              << "Drawing_UpdateDrawing"
              << "Separator"
              << "Drawing_InsertView"
              << "Drawing_ProjectedView"
              << "Drawing_AuxiliaryView"
              << "Drawing_SectionView"
              << "Drawing_DetailView"
              << "Drawing_BreakView"
              << "Drawing_CropView"
              << "Drawing_ToggleShadedView"
              << "Separator"
              << "Drawing_SmartDimension"
              << "Drawing_DimensionPalette"
              << "Drawing_HoleCallout"
              << "Drawing_GdtFrame"
              << "Drawing_DatumSymbol"
              << "Drawing_AutoCentermarks"
              << "TechDraw_CenterLineGroup"
              << "TechDraw_RichTextAnnotation"
              << "Drawing_AutoBalloon"
              << "Drawing_MagneticLine"
              << "TechDraw_Balloon"
              << "Drawing_BomTable"
              << "Separator"
              << "Drawing_BreakAlignment"
              << "Drawing_RestoreAlignment"
              << "Separator"
              << "Drawing_ExportPDF"
              << "Drawing_ExportAllPDF"
              << "Drawing_ExportDXF"
              << "Drawing_ExportSVG";

    root->insertItem(item, drawMenu);
    return root;
}

Gui::ToolBarItem* Workbench::setupToolBars() const
{
    Gui::ToolBarItem* root = StdWorkbench::setupToolBars();

    // Onshape-clean unified single drafting toolbar (for Classic Toolbar mode)
    auto* drafting = new Gui::ToolBarItem(root);
    drafting->setCommand("Drawing");
    *drafting << "Drawing_NewSheet"
              << "Drawing_UpdateDrawing"
              << "Separator"
              << "Drawing_InsertView"
              << "Drawing_ProjectedView"
              << "Drawing_AuxiliaryView"
              << "Drawing_SectionView"
              << "Drawing_DetailView"
              << "Drawing_BreakView"
              << "Drawing_CropView"
              << "Separator"
              << "Drawing_SmartDimension"
              << "Drawing_HoleCallout"
              << "Drawing_GdtFrame"
              << "Drawing_DatumSymbol"
              << "Drawing_AutoCentermarks"
              << "Drawing_AutoBalloon"
              << "Drawing_BomTable"
              << "Separator"
              << "Drawing_ExportPDF"
              << "Drawing_ExportAllPDF"
              << "Drawing_ExportDXF";

    return root;
}

Gui::ToolBarItem* Workbench::setupCommandBars() const
{
    auto* root = new Gui::ToolBarItem;
    return root;
}

}  // namespace DrawingGui
