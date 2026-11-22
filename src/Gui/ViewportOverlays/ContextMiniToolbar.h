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

#include <QFrame>
#include <QList>
#include <QPoint>

#include <FCGlobal.h>

class QAction;
class QToolBar;

namespace Gui
{

/**
 * Floating SolidWorks-style Context Mini-Toolbar displayed near the cursor when
 * geometry or document objects are selected in a viewport.
 *
 * Non-focus-stealing so keyboard shortcuts and viewport interactions remain uninterrupted.
 */
class GuiExport ContextMiniToolbar: public QFrame
{
    Q_OBJECT

public:
    explicit ContextMiniToolbar(QWidget* parent = nullptr);
    ~ContextMiniToolbar() override = default;

    /// Populate from registered FreeCAD command names and show near globalAnchor.
    bool showForCommands(const std::vector<std::string>& commands, const QPoint& globalAnchor);

    /// Populate directly from QActions (useful for custom callers and tests).
    bool showForActions(const QList<QAction*>& actions, const QPoint& globalAnchor);

    void dismiss();
    int actionCount() const;

private:
    void positionNear(const QPoint& globalAnchor);

    QToolBar* m_toolBar {nullptr};
};

}  // namespace Gui
