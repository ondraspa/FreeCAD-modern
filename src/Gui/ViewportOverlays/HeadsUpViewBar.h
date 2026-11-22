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
#include <QPointer>

#include <FCGlobal.h>

class QHBoxLayout;
class QToolBar;

namespace Gui
{

/**
 * Translucent SolidWorks-style Heads-Up View Toolbar anchored at top-center of the active viewport.
 *
 * Adapts automatically between 3D viewports and 2D Drawing pages, and allows modules to attach
 * an optional status badge widget (e.g., Drawing's "Update Drawing" indicator).
 */
class GuiExport HeadsUpViewBar: public QFrame
{
    Q_OBJECT

public:
    enum class Mode
    {
        Hidden,
        View3D,
        Drawing2D
    };

    explicit HeadsUpViewBar(QWidget* parent = nullptr);
    ~HeadsUpViewBar() override = default;

    void setMode(Mode mode);
    Mode mode() const;

    /// Override the command list used in Drawing2D mode (e.g., when Drawing workbench loads).
    void setDrawingCommands(const std::vector<std::string>& commands);

    /// Attach or clear an optional right-side badge widget (e.g., explicit Update Drawing button).
    void setBadgeWidget(QWidget* badge);

    /// Reposition centered along the top edge of the host viewport or mapped rect.
    void updateGeometryInHost(QWidget* host);
    void updateGeometryInRect(const QRect& hostRect);

    int actionCount() const;

private:
    void rebuildActions();

    QHBoxLayout* m_layout {nullptr};
    QToolBar* m_toolBar {nullptr};
    QPointer<QWidget> m_badgeWidget;
    Mode m_mode {Mode::Hidden};
    std::vector<std::string> m_drawingCommands;
};

}  // namespace Gui
