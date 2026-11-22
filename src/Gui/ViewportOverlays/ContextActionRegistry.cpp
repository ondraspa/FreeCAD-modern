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
#include <sstream>
#include <unordered_set>

#include <App/Application.h>
#include <Base/Parameter.h>
#include <Gui/Selection/SelectionFilter.h>

#include "ContextActionRegistry.h"

namespace Gui
{

namespace
{

std::vector<std::string> splitCommaList(const std::string& csv)
{
    std::vector<std::string> out;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const auto start = token.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        const auto end = token.find_last_not_of(" \t\r\n");
        out.push_back(token.substr(start, end - start + 1));
    }
    return out;
}

}  // namespace

ContextActionRegistry& ContextActionRegistry::instance()
{
    static ContextActionRegistry registry;
    return registry;
}

void ContextActionRegistry::registerContextRule(const ContextActionRule& rule)
{
    auto it = std::find_if(m_rules.begin(), m_rules.end(), [&rule](const ContextActionRule& existing) {
        return existing.id == rule.id;
    });
    if (it != m_rules.end()) {
        *it = rule;
    }
    else {
        m_rules.push_back(rule);
    }
}

void ContextActionRegistry::unregisterContextRule(const std::string& ruleId)
{
    std::erase_if(m_rules, [&ruleId](const ContextActionRule& rule) { return rule.id == ruleId; });
}

void ContextActionRegistry::registerShortcutBar(const ShortcutBarContext& context)
{
    auto it = std::find_if(
        m_shortcutBars.begin(),
        m_shortcutBars.end(),
        [&context](const ShortcutBarContext& existing) {
            return existing.contextId == context.contextId;
        }
    );
    if (it != m_shortcutBars.end()) {
        *it = context;
    }
    else {
        m_shortcutBars.push_back(context);
    }
}

void ContextActionRegistry::unregisterShortcutBar(const std::string& contextId)
{
    std::erase_if(m_shortcutBars, [&contextId](const ShortcutBarContext& ctx) {
        return ctx.contextId == contextId;
    });
}

void ContextActionRegistry::clear()
{
    m_rules.clear();
    m_shortcutBars.clear();
    m_defaultsRegistered = false;
}

void ContextActionRegistry::ensureDefaultsRegistered()
{
    if (!m_defaultsRegistered) {
        registerDefaultRules();
    }
}

void ContextActionRegistry::registerDefaultRules()
{
    m_defaultsRegistered = true;

    // Context mini-toolbar rules (higher priority checked first)
    registerContextRule(ContextActionRule {
        .id = "core_single_face",
        .selectionFilter = "SELECT Part::Feature SUBELEMENT Face COUNT 1",
        .workbenchFilter = "",
        .priority = 30,
        .commands = {
            "PartDesign_NewSketch",
            "PartDesign_Fillet",
            "PartDesign_Chamfer",
            "PartDesign_Draft",
            "PartDesign_Thickness",
            "Std_ToggleVisibility",
        },
    });

    registerContextRule(ContextActionRule {
        .id = "core_edges",
        .selectionFilter = "SELECT Part::Feature SUBELEMENT Edge COUNT 1..",
        .workbenchFilter = "",
        .priority = 25,
        .commands = {
            "PartDesign_Fillet",
            "PartDesign_Chamfer",
            "Part_DatumPlane",
            "Std_Measure",
        },
    });

    registerContextRule(ContextActionRule {
        .id = "core_vertices",
        .selectionFilter = "SELECT Part::Feature SUBELEMENT Vertex COUNT 1..",
        .workbenchFilter = "",
        .priority = 20,
        .commands = {
            "Part_DatumPoint",
            "Part_DatumLine",
            "Std_Measure",
        },
    });

    registerContextRule(ContextActionRule {
        .id = "core_sketch_object",
        .selectionFilter = "SELECT Sketcher::SketchObject COUNT 1",
        .workbenchFilter = "",
        .priority = 35,
        .commands = {
            "Std_Edit",
            "PartDesign_Pad",
            "PartDesign_Pocket",
            "PartDesign_Revolution",
            "PartDesign_Groove",
            "Std_ToggleVisibility",
        },
    });

    registerContextRule(ContextActionRule {
        .id = "core_plane_object",
        .selectionFilter = "SELECT App::Plane COUNT 1",
        .workbenchFilter = "",
        .priority = 22,
        .commands = {
            "PartDesign_NewSketch",
            "Part_DatumPlane",
            "Std_ToggleVisibility",
        },
    });

    registerContextRule(ContextActionRule {
        .id = "core_body_object",
        .selectionFilter = "SELECT PartDesign::Body COUNT 1",
        .workbenchFilter = "",
        .priority = 15,
        .commands = {
            "PartDesign_NewSketch",
            "Std_TransformManip",
            "Std_ViewFitSelection",
            "Std_ToggleVisibility",
        },
    });

    registerContextRule(ContextActionRule {
        .id = "core_techdraw_view",
        .selectionFilter = "SELECT TechDraw::DrawViewPart COUNT 1",
        .workbenchFilter = "",
        .priority = 30,
        .commands = {
            "TechDraw_Dimension",
            "TechDraw_SectionView",
            "TechDraw_DetailView",
            "TechDraw_Balloon",
            "TechDraw_ToggleFrame",
        },
    });

    // S-Key Shortcut Bar defaults per context
    registerShortcutBar(ShortcutBarContext {
        .contextId = "Part",
        .commands = {
            "PartDesign_NewSketch",
            "PartDesign_Pad",
            "PartDesign_Pocket",
            "PartDesign_Hole",
            "PartDesign_Fillet",
            "PartDesign_Chamfer",
            "PartDesign_Mirrored",
            "Std_Measure",
        },
    });

    registerShortcutBar(ShortcutBarContext {
        .contextId = "Sketch",
        .commands = {
            "Sketcher_CompLine",
            "Sketcher_CompCreateRectangles",
            "Sketcher_CompCreateConic",
            "Sketcher_CompCreateArc",
            "Sketcher_Dimension",
            "Sketcher_Trimming",
            "Sketcher_ConstrainCoincidentUnified",
            "Sketcher_LeaveSketch",
        },
    });

    registerShortcutBar(ShortcutBarContext {
        .contextId = "Assembly",
        .commands = {
            "Assembly_Insert",
            "Assembly_ToggleGrounded",
            "Assembly_CreateJointFixed",
            "Assembly_CreateJointRevolute",
            "Assembly_CreateJointCylindrical",
            "Assembly_CreateJointSlider",
            "Assembly_CreateBom",
            "Std_Measure",
        },
    });

    registerShortcutBar(ShortcutBarContext {
        .contextId = "Drawing",
        .commands = {
            "TechDraw_View",
            "TechDraw_ProjectionGroup",
            "TechDraw_SectionView",
            "TechDraw_DetailView",
            "TechDraw_Dimension",
            "TechDraw_Balloon",
            "TechDraw_RichTextAnnotation",
            "TechDraw_CenterLineGroup",
        },
    });

    registerShortcutBar(ShortcutBarContext {
        .contextId = "Default",
        .commands = {
            "PartDesign_NewSketch",
            "Std_ViewFitAll",
            "Std_ViewIsometric",
            "Std_ToggleClipPlane",
            "Std_Measure",
            "Std_ToggleVisibility",
        },
    });
}

std::vector<std::string> ContextActionRegistry::resolveMiniToolbarCommands(
    const std::string& activeWorkbench
) const
{
    std::vector<const ContextActionRule*> candidates;
    candidates.reserve(m_rules.size());

    for (const auto& rule : m_rules) {
        if (!rule.workbenchFilter.empty() && rule.workbenchFilter != activeWorkbench) {
            continue;
        }
        candidates.push_back(&rule);
    }

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const ContextActionRule* lhs, const ContextActionRule* rhs) {
            return lhs->priority > rhs->priority;
        }
    );

    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    for (const auto* rule : candidates) {
        if (rule->selectionFilter.empty()) {
            continue;
        }
        SelectionFilter filter(rule->selectionFilter);
        if (!filter.isValid() || !filter.match()) {
            continue;
        }
        for (const auto& cmd : rule->commands) {
            if (seen.insert(cmd).second) {
                result.push_back(cmd);
            }
        }
        // Stop at the highest-priority matching rule to keep the mini-toolbar concise.
        if (!result.empty()) {
            break;
        }
    }

    return result;
}

std::vector<std::string> ContextActionRegistry::resolveShortcutBarCommands(
    const std::string& contextId
) const
{
    // Allow user customization via BaseApp/Preferences/MainWindow/ShortcutBar/<contextId>
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow/ShortcutBar"
    );
    if (hGrp.isValid()) {
        const std::string customCsv = hGrp->GetASCII(contextId.c_str(), "");
        if (!customCsv.empty()) {
            return splitCommaList(customCsv);
        }
    }

    auto findContext = [this](const std::string& id) -> const ShortcutBarContext* {
        auto it = std::find_if(
            m_shortcutBars.begin(),
            m_shortcutBars.end(),
            [&id](const ShortcutBarContext& ctx) { return ctx.contextId == id; }
        );
        return it != m_shortcutBars.end() ? &(*it) : nullptr;
    };

    if (const auto* ctx = findContext(contextId)) {
        return ctx->commands;
    }
    if (const auto* fallback = findContext("Default")) {
        return fallback->commands;
    }
    return {};
}

std::string ContextActionRegistry::shortcutContextForState(
    const std::string& activeWorkbench,
    bool inSketchEdit,
    bool isDrawingView
)
{
    if (inSketchEdit || activeWorkbench == "SketcherWorkbench") {
        return "Sketch";
    }
    if (isDrawingView || activeWorkbench == "DrawingWorkbench"
        || activeWorkbench == "TechDrawWorkbench") {
        return "Drawing";
    }
    if (activeWorkbench == "AssemblyWorkbench") {
        return "Assembly";
    }
    if (activeWorkbench == "PartDesignWorkbench" || activeWorkbench == "PartWorkbench") {
        return "Part";
    }
    return "Default";
}

}  // namespace Gui
