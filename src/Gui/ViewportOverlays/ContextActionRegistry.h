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

#pragma once

#include <string>
#include <vector>

#include <FCGlobal.h>

namespace Gui
{

/**
 * Rule mapping a FreeCAD SelectionFilter string (and optional workbench) to
 * commands shown in the floating ContextMiniToolbar.
 */
struct ContextActionRule
{
    std::string id;
    std::string selectionFilter;
    std::string workbenchFilter;  // empty matches all workbenches
    int priority {0};             // higher priority evaluated first
    std::vector<std::string> commands;
};

/**
 * Default command set for the S-Key ShortcutBarPopup in a given CAD context.
 */
struct ShortcutBarContext
{
    std::string contextId;  // e.g. "Part", "Sketch", "Assembly", "Drawing", "Default"
    std::vector<std::string> commands;
};

/**
 * Registry shared by ContextMiniToolbar and ShortcutBarPopup.
 *
 * Keeps core Gui decoupled from workbench-specific types while allowing any
 * workbench (including PartDesign, Assembly, and Drawing) to contribute rules.
 */
class GuiExport ContextActionRegistry
{
public:
    static ContextActionRegistry& instance();

    void registerContextRule(const ContextActionRule& rule);
    void unregisterContextRule(const std::string& ruleId);

    void registerShortcutBar(const ShortcutBarContext& context);
    void unregisterShortcutBar(const std::string& contextId);

    void clear();
    void ensureDefaultsRegistered();
    void registerDefaultRules();

    /// Evaluates registered rules against the current Gui::Selection() state.
    std::vector<std::string> resolveMiniToolbarCommands(const std::string& activeWorkbench) const;

    /// Returns shortcut bar commands for a context, honoring user preference overrides.
    std::vector<std::string> resolveShortcutBarCommands(const std::string& contextId) const;

    /// Maps active workbench + edit/view flags to a canonical shortcut bar context ID.
    static std::string shortcutContextForState(
        const std::string& activeWorkbench,
        bool inSketchEdit = false,
        bool isDrawingView = false
    );

private:
    ContextActionRegistry() = default;

    bool m_defaultsRegistered {false};
    std::vector<ContextActionRule> m_rules;
    std::vector<ShortcutBarContext> m_shortcutBars;
};

}  // namespace Gui
