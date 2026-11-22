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

#include <algorithm>

#include "CommandRibbonRegistry.h"

namespace Gui
{

CommandRibbonRegistry& CommandRibbonRegistry::instance()
{
    static CommandRibbonRegistry registry;
    return registry;
}

void CommandRibbonRegistry::registerTab(const RibbonTabDef& tab)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [&tab](const RibbonTabDef& existing) {
        return existing.id == tab.id;
    });
    if (it != m_tabs.end()) {
        *it = tab;
    }
    else {
        m_tabs.push_back(tab);
    }
}

void CommandRibbonRegistry::unregisterTab(const std::string& tabId)
{
    std::erase_if(m_tabs, [&tabId](const RibbonTabDef& tab) { return tab.id == tabId; });
}

void CommandRibbonRegistry::registerContext(const RibbonContextDef& context)
{
    auto it = std::find_if(
        m_contexts.begin(),
        m_contexts.end(),
        [&context](const RibbonContextDef& existing) { return existing.id == context.id; }
    );
    if (it != m_contexts.end()) {
        *it = context;
    }
    else {
        m_contexts.push_back(context);
    }
}

void CommandRibbonRegistry::unregisterContext(const std::string& contextId)
{
    std::erase_if(m_contexts, [&contextId](const RibbonContextDef& ctx) {
        return ctx.id == contextId;
    });
}

void CommandRibbonRegistry::clear()
{
    m_tabs.clear();
    m_contexts.clear();
    m_defaultsRegistered = false;
}

void CommandRibbonRegistry::ensureDefaultsRegistered()
{
    if (!m_defaultsRegistered) {
        registerDefaultContexts();
    }
}

void CommandRibbonRegistry::registerDefaultContexts()
{
    m_defaultsRegistered = true;

    using enum RibbonButtonSize;

    // PartDesign Features tab
    registerTab(RibbonTabDef {
        .id = "part_features",
        .title = "Features",
        .workbenchName = "PartDesignWorkbench",
        .groups = {
            RibbonGroupDef {
                .id = "part_create",
                .title = "Create",
                .commands = {
                    {"PartDesign_NewSketch", Large},
                    {"PartDesign_Pad", Large},
                    {"PartDesign_Revolution", Large},
                    {"PartDesign_AdditiveLoft", Compact},
                    {"PartDesign_AdditivePipe", Compact},
                    {"PartDesign_AdditiveHelix", Compact},
                },
            },
            RibbonGroupDef {
                .id = "part_cut",
                .title = "Cut",
                .commands = {
                    {"PartDesign_Pocket", Large},
                    {"PartDesign_Hole", Large},
                    {"PartDesign_Groove", Large},
                    {"PartDesign_SubtractiveLoft", Compact},
                    {"PartDesign_SubtractivePipe", Compact},
                    {"PartDesign_SubtractiveHelix", Compact},
                },
            },
            RibbonGroupDef {
                .id = "part_dressup",
                .title = "Modify",
                .commands = {
                    {"PartDesign_Fillet", Large},
                    {"PartDesign_Chamfer", Large},
                    {"PartDesign_Draft", Compact},
                    {"PartDesign_Thickness", Compact},
                    {"PartDesign_Boolean", Compact},
                },
            },
            RibbonGroupDef {
                .id = "part_pattern",
                .title = "Pattern",
                .commands = {
                    {"PartDesign_Mirrored", Large},
                    {"PartDesign_LinearPattern", Compact},
                    {"PartDesign_PolarPattern", Compact},
                    {"PartDesign_MultiTransform", Compact},
                },
            },
            RibbonGroupDef {
                .id = "part_body",
                .title = "Body",
                .commands = {
                    {"PartDesign_Body", Large},
                    {"PartDesign_SubShapeBinder", Compact},
                    {"PartDesign_Clone", Compact},
                },
            },
        },
    });

    // Sketcher tab
    registerTab(RibbonTabDef {
        .id = "sketch_tools",
        .title = "Sketch",
        .workbenchName = "SketcherWorkbench",
        .groups = {
            RibbonGroupDef {
                .id = "sketch_session",
                .title = "Sketch",
                .commands = {
                    {"PartDesign_NewSketch", Large},
                    {"Sketcher_LeaveSketch", Large},
                },
            },
            RibbonGroupDef {
                .id = "sketch_draw",
                .title = "Draw",
                .commands = {
                    {"Sketcher_CompLine", Large},
                    {"Sketcher_CompCreateRectangles", Large},
                    {"Sketcher_CompCreateConic", Large},
                    {"Sketcher_CompCreateArc", Large},
                    {"Sketcher_CreatePoint", Compact},
                    {"Sketcher_ToggleConstruction", Compact},
                },
            },
            RibbonGroupDef {
                .id = "sketch_modify",
                .title = "Modify",
                .commands = {
                    {"Sketcher_Trimming", Large},
                    {"Sketcher_Extend", Compact},
                    {"Sketcher_Offset", Compact},
                    {"Sketcher_Symmetry", Compact},
                    {"Sketcher_Translate", Compact},
                },
            },
            RibbonGroupDef {
                .id = "sketch_constraints",
                .title = "Constraints",
                .commands = {
                    {"Sketcher_Dimension", Large},
                    {"Sketcher_ConstrainCoincidentUnified", Large},
                    {"Sketcher_ConstrainHorVer", Compact},
                    {"Sketcher_ConstrainParallel", Compact},
                    {"Sketcher_ConstrainPerpendicular", Compact},
                    {"Sketcher_ConstrainTangent", Compact},
                    {"Sketcher_ConstrainEqual", Compact},
                    {"Sketcher_ConstrainSymmetric", Compact},
                },
            },
        },
    });

    // Reference Geometry / Datums tab
    registerTab(RibbonTabDef {
        .id = "reference_geometry",
        .title = "Reference",
        .workbenchName = "PartDesignWorkbench",
        .groups = {
            RibbonGroupDef {
                .id = "ref_datums",
                .title = "Datums",
                .commands = {
                    {"Part_DatumPlane", Large},
                    {"Part_DatumLine", Large},
                    {"Part_DatumPoint", Large},
                    {"Part_CoordinateSystem", Large},
                },
            },
            RibbonGroupDef {
                .id = "ref_primitives",
                .title = "Binders & Primitives",
                .commands = {
                    {"PartDesign_ShapeBinder", Large},
                    {"PartDesign_SubShapeBinder", Large},
                    {"PartDesign_CompPrimitiveAdditive", Compact},
                    {"PartDesign_CompPrimitiveSubtractive", Compact},
                },
            },
        },
    });

    // Assembly tab
    registerTab(RibbonTabDef {
        .id = "assembly_tools",
        .title = "Assembly",
        .workbenchName = "AssemblyWorkbench",
        .groups = {
            RibbonGroupDef {
                .id = "asm_components",
                .title = "Components",
                .commands = {
                    {"Assembly_CreateAssembly", Large},
                    {"Assembly_Insert", Large},
                    {"Assembly_ToggleGrounded", Large},
                    {"Assembly_SolveAssembly", Compact},
                },
            },
            RibbonGroupDef {
                .id = "asm_joints",
                .title = "Joints",
                .commands = {
                    {"Assembly_CreateJointFixed", Large},
                    {"Assembly_CreateJointRevolute", Large},
                    {"Assembly_CreateJointCylindrical", Compact},
                    {"Assembly_CreateJointSlider", Compact},
                    {"Assembly_CreateJointBall", Compact},
                    {"Assembly_CreateJointDistance", Compact},
                    {"Assembly_CreateJointParallel", Compact},
                    {"Assembly_CreateJointPerpendicular", Compact},
                    {"Assembly_CreateJointAngle", Compact},
                },
            },
            RibbonGroupDef {
                .id = "asm_docs",
                .title = "Documentation",
                .commands = {
                    {"Assembly_CreateView", Large},
                    {"Assembly_CreateBom", Large},
                    {"Assembly_CreateSimulation", Compact},
                },
            },
        },
    });

    // Drawing View Layout tab (overridable by Drawing workbench module)
    registerTab(RibbonTabDef {
        .id = "drawing_views",
        .title = "View Layout",
        .workbenchName = "TechDrawWorkbench",
        .groups = {
            RibbonGroupDef {
                .id = "drw_sheet",
                .title = "Sheet",
                .commands = {
                    {"TechDraw_NewPage", Large},
                    {"TechDraw_RedrawPage", Large},
                    {"TechDraw_FillTemplateFields", Compact},
                },
            },
            RibbonGroupDef {
                .id = "drw_views",
                .title = "Views",
                .commands = {
                    {"TechDraw_View", Large},
                    {"TechDraw_ProjectionGroup", Large},
                    {"TechDraw_SectionGroup", Large},
                    {"TechDraw_DetailView", Large},
                    {"TechDraw_BrokenView", Compact},
                    {"TechDraw_ActiveView", Compact},
                },
            },
            RibbonGroupDef {
                .id = "drw_export",
                .title = "Export",
                .commands = {
                    {"TechDraw_ExportPagePDF", Large},
                    {"TechDraw_ExportPageDXF", Compact},
                    {"TechDraw_ExportPageSVG", Compact},
                    {"TechDraw_PrintAll", Compact},
                },
            },
        },
    });

    // Drawing Annotation tab
    registerTab(RibbonTabDef {
        .id = "drawing_annotations",
        .title = "Annotation",
        .workbenchName = "TechDrawWorkbench",
        .groups = {
            RibbonGroupDef {
                .id = "drw_dims",
                .title = "Dimensions",
                .commands = {
                    {"TechDraw_Dimension", Large},
                    {"TechDraw_Balloon", Large},
                    {"TechDraw_DimensionRepair", Compact},
                },
            },
            RibbonGroupDef {
                .id = "drw_symbols",
                .title = "Notes & Symbols",
                .commands = {
                    {"TechDraw_RichTextAnnotation", Large},
                    {"TechDraw_LeaderLine", Large},
                    {"TechDraw_CenterLineGroup", Compact},
                    {"TechDraw_WeldSymbol", Compact},
                    {"TechDraw_SurfaceFinishSymbols", Compact},
                    {"TechDraw_HoleShaftFit", Compact},
                },
            },
            RibbonGroupDef {
                .id = "drw_tables",
                .title = "Tables & Hatch",
                .commands = {
                    {"TechDraw_SpreadsheetView", Large},
                    {"TechDraw_Hatch", Compact},
                    {"TechDraw_GeometricHatch", Compact},
                    {"TechDraw_ToggleFrame", Compact},
                },
            },
        },
    });

    // Evaluate / Inspect tab (available across contexts)
    registerTab(RibbonTabDef {
        .id = "evaluate_tools",
        .title = "Evaluate",
        .workbenchName = "",
        .groups = {
            RibbonGroupDef {
                .id = "eval_measure",
                .title = "Inspect",
                .commands = {
                    {"Std_Measure", Large},
                    {"Std_ToggleClipPlane", Large},
                    {"Std_ViewFitAll", Compact},
                    {"Std_DrawStyle", Compact},
                },
            },
            RibbonGroupDef {
                .id = "eval_visibility",
                .title = "Display",
                .commands = {
                    {"Std_ToggleVisibility", Large},
                    {"Std_ViewIsometric", Compact},
                    {"Std_OrthographicCamera", Compact},
                    {"Std_PerspectiveCamera", Compact},
                },
            },
        },
    });

    // Register primary CAD contexts
    registerContext(RibbonContextDef {
        .id = "part_context",
        .matchWorkbench = "PartDesignWorkbench",
        .tabIds = {"part_features", "sketch_tools", "reference_geometry", "evaluate_tools"},
        .defaultTabId = "part_features",
    });

    registerContext(RibbonContextDef {
        .id = "sketch_context",
        .matchWorkbench = "SketcherWorkbench",
        .tabIds = {"sketch_tools", "part_features", "evaluate_tools"},
        .defaultTabId = "sketch_tools",
    });

    registerContext(RibbonContextDef {
        .id = "assembly_context",
        .matchWorkbench = "AssemblyWorkbench",
        .tabIds = {"assembly_tools", "reference_geometry", "evaluate_tools"},
        .defaultTabId = "assembly_tools",
    });

    registerContext(RibbonContextDef {
        .id = "techdraw_context",
        .matchWorkbench = "TechDrawWorkbench",
        .tabIds = {"drawing_views", "drawing_annotations", "evaluate_tools"},
        .defaultTabId = "drawing_views",
    });
}

const RibbonContextDef* CommandRibbonRegistry::findContextForWorkbench(
    const std::string& workbenchName
) const
{
    auto it = std::find_if(
        m_contexts.begin(),
        m_contexts.end(),
        [&workbenchName](const RibbonContextDef& ctx) { return ctx.matchWorkbench == workbenchName; }
    );
    return it != m_contexts.end() ? &(*it) : nullptr;
}

bool CommandRibbonRegistry::hasContext(const std::string& workbenchName) const
{
    return findContextForWorkbench(workbenchName) != nullptr;
}

const RibbonTabDef* CommandRibbonRegistry::findTab(const std::string& tabId) const
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [&tabId](const RibbonTabDef& tab) {
        return tab.id == tabId;
    });
    return it != m_tabs.end() ? &(*it) : nullptr;
}

std::vector<RibbonTabDef> CommandRibbonRegistry::resolveTabsForContext(
    const std::string& workbenchName,
    bool inSketchEdit,
    bool isDrawingView
) const
{
    const RibbonContextDef* ctx = nullptr;

    if (isDrawingView) {
        ctx = findContextForWorkbench("DrawingWorkbench");
        if (!ctx) {
            ctx = findContextForWorkbench("TechDrawWorkbench");
        }
    }
    if (!ctx) {
        ctx = findContextForWorkbench(workbenchName);
    }

    std::vector<RibbonTabDef> resolved;
    if (!ctx) {
        if (inSketchEdit) {
            if (const auto* sketchTab = findTab("sketch_tools")) {
                resolved.push_back(*sketchTab);
            }
        }
        return resolved;
    }

    for (const auto& tabId : ctx->tabIds) {
        if (const auto* tab = findTab(tabId)) {
            resolved.push_back(*tab);
        }
    }

    if (inSketchEdit) {
        bool hasSketch = std::any_of(
            resolved.begin(),
            resolved.end(),
            [](const RibbonTabDef& tab) { return tab.id == "sketch_tools"; }
        );
        if (!hasSketch) {
            if (const auto* sketchTab = findTab("sketch_tools")) {
                resolved.insert(resolved.begin(), *sketchTab);
            }
        }
    }

    return resolved;
}

std::string CommandRibbonRegistry::resolveActiveTabId(
    const std::string& workbenchName,
    bool inSketchEdit,
    bool isDrawingView
) const
{
    if (inSketchEdit && findTab("sketch_tools")) {
        return "sketch_tools";
    }

    if (isDrawingView) {
        if (const auto* drawingCtx = findContextForWorkbench("DrawingWorkbench")) {
            return drawingCtx->defaultTabId;
        }
        if (const auto* tdCtx = findContextForWorkbench("TechDrawWorkbench")) {
            return tdCtx->defaultTabId;
        }
    }

    if (const auto* ctx = findContextForWorkbench(workbenchName)) {
        return ctx->defaultTabId;
    }

    return {};
}

}  // namespace Gui
