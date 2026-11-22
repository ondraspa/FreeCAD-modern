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

#include <QPointer>
#include <QWidget>
#include <fastsignals/signal.h>

#include <FCGlobal.h>
#include "CommandRibbonRegistry.h"

class QHBoxLayout;
class QMenu;
class QScrollArea;
class QTabBar;
class QToolBar;
class QToolButton;

namespace Gui
{

class MDIView;
class ToolBarItem;
class ViewProviderDocumentObject;

/**
 * SolidWorks-inspired context-aware Command Ribbon bar hosted at the top of MainWindow.
 *
 * Displays task-grouped tabs for core CAD contexts (Part, Sketch, Assembly, Drawing)
 * and seamlessly adapts ToolBarItem definitions for any other workbench.
 */
class GuiExport CommandRibbonBar: public QWidget
{
    Q_OBJECT

public:
    explicit CommandRibbonBar(QWidget* parent = nullptr);
    ~CommandRibbonBar() override;

    /// Rebuild ribbon tabs for the given workbench and active edit/view state.
    void refreshContext(
        const std::string& workbenchName,
        bool inSketchEdit = false,
        bool isDrawingView = false
    );

    /// Supply the active workbench's ToolBarItem tree for fallback tab generation.
    void setFallbackToolBarItems(const ToolBarItem* rootItems, const std::string& workbenchName);

    /// Switch to a tab by its registered tab ID if present.
    bool selectTabById(const std::string& tabId);

    /// Collapse or expand the command strip (keeping the tab header visible).
    void setCollapsed(bool collapsed);
    bool isCollapsed() const;

    /// Current tab IDs shown in the tab strip (for introspection and testing).
    std::vector<std::string> visibleTabIds() const;
    std::string activeTabId() const;

    /// Populate the "+ More" workbenches menu.
    void rebuildMoreWorkbenchesMenu();

public Q_SLOTS:
    void onWorkbenchActivated(const QString& workbenchName);
    void onWindowActivated(Gui::MDIView* view);
    void retranslateUi();

Q_SIGNALS:
    void tabActivated(const QString& tabId);
    void collapsedChanged(bool collapsed);

protected:
    void changeEvent(QEvent* event) override;

private Q_SLOTS:
    void onTabBarCurrentChanged(int index);
    void onCollapseToggled(bool checked);

private:
    void setupUi();
    void setupQuickAccessBar();
    void connectApplicationSignals();
    void buildActiveTabContent();
    void clearStripLayout();
    QWidget* createGroupWidget(const RibbonGroupDef& group);
    std::vector<RibbonGroupDef> buildFallbackGroupsFromToolBars() const;
    static bool isSketchObjectType(const ViewProviderDocumentObject& vp);
    static bool isDrawingMdiView(const MDIView* view);

    QWidget* m_headerWidget {nullptr};
    QToolBar* m_quickAccessBar {nullptr};
    QTabBar* m_tabBar {nullptr};
    QToolButton* m_moreButton {nullptr};
    QMenu* m_moreMenu {nullptr};
    QToolButton* m_collapseButton {nullptr};

    QScrollArea* m_contentScroll {nullptr};
    QWidget* m_stripContainer {nullptr};
    QHBoxLayout* m_stripLayout {nullptr};

    std::string m_currentWorkbench;
    bool m_inSketchEdit {false};
    bool m_isDrawingView {false};
    bool m_collapsed {false};
    bool m_updatingTabs {false};

    std::vector<RibbonTabDef> m_visibleTabs;
    std::vector<RibbonGroupDef> m_fallbackGroups;
    std::string m_fallbackWorkbench;

    fastsignals::scoped_connection m_connInEdit;
    fastsignals::scoped_connection m_connResetEdit;
    fastsignals::scoped_connection m_connRefreshWorkbenches;
};

}  // namespace Gui
