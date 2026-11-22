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

enum class RibbonButtonSize
{
    Large,
    Compact
};

struct RibbonCommandEntry
{
    std::string commandName;
    RibbonButtonSize size {RibbonButtonSize::Large};
};

struct RibbonGroupDef
{
    std::string id;
    std::string title;
    std::vector<RibbonCommandEntry> commands;
};

struct RibbonTabDef
{
    std::string id;
    std::string title;
    std::string workbenchName;
    std::vector<RibbonGroupDef> groups;
};

struct RibbonContextDef
{
    std::string id;
    std::string matchWorkbench;
    std::vector<std::string> tabIds;
    std::string defaultTabId;
};

/**
 * Central registry for context-aware CommandRibbonBar tabs and workbench mappings.
 *
 * Modules contribute or override tabs and contexts by string command IDs without
 * coupling core Gui to workbench-specific headers or feature classes.
 */
class GuiExport CommandRibbonRegistry
{
public:
    static CommandRibbonRegistry& instance();

    void registerTab(const RibbonTabDef& tab);
    void unregisterTab(const std::string& tabId);

    void registerContext(const RibbonContextDef& context);
    void unregisterContext(const std::string& contextId);

    void clear();
    void ensureDefaultsRegistered();
    void registerDefaultContexts();

    bool hasContext(const std::string& workbenchName) const;
    const RibbonTabDef* findTab(const std::string& tabId) const;

    std::vector<RibbonTabDef> resolveTabsForContext(
        const std::string& workbenchName,
        bool inSketchEdit = false,
        bool isDrawingView = false
    ) const;

    std::string resolveActiveTabId(
        const std::string& workbenchName,
        bool inSketchEdit = false,
        bool isDrawingView = false
    ) const;

private:
    CommandRibbonRegistry() = default;

    const RibbonContextDef* findContextForWorkbench(const std::string& workbenchName) const;

    bool m_defaultsRegistered {false};
    std::vector<RibbonTabDef> m_tabs;
    std::vector<RibbonContextDef> m_contexts;
};

}  // namespace Gui
