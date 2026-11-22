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

#include <QHBoxLayout>
#include <QToolBar>

#include <Gui/Application.h>
#include <Gui/Command.h>

#include "HeadsUpViewBar.h"

namespace Gui
{

namespace
{

constexpr int kHeadsUpIconPx = 18;
constexpr int kHeadsUpTopMargin = 8;

const std::vector<std::string>& default3DCommands()
{
    static const std::vector<std::string> commands = {
        "Std_ViewFitAll",
        "Std_ViewBoxZoom",
        "Separator",
        "Std_ViewGroup",
        "Std_DrawStyle",
        "Std_ToggleClipPlane",
        "Separator",
        "Std_OrthographicCamera",
        "Std_PerspectiveCamera",
        "Std_Measure",
    };
    return commands;
}

const std::vector<std::string>& defaultDrawingCommands()
{
    static const std::vector<std::string> commands = {
        "Std_ViewFitAll",
        "Separator",
        "TechDraw_ToggleFrame",
        "TechDraw_RedrawPage",
    };
    return commands;
}

}  // namespace

HeadsUpViewBar::HeadsUpViewBar(QWidget* parent)
    : QFrame(parent)
    , m_drawingCommands(defaultDrawingCommands())
{
    setObjectName(QStringLiteral("HeadsUpViewBar"));
    setFrameShape(QFrame::StyledPanel);
    setAttribute(Qt::WA_StyledBackground, true);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(4);

    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("*headsUpToolBar"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(kHeadsUpIconPx, kHeadsUpIconPx));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_layout->addWidget(m_toolBar);
    hide();
}

void HeadsUpViewBar::setMode(Mode mode)
{
    if (m_mode == mode && isVisible() == (mode != Mode::Hidden)) {
        return;
    }
    m_mode = mode;
    rebuildActions();

    if (m_mode == Mode::Hidden || (m_toolBar->actions().isEmpty() && !m_badgeWidget)) {
        hide();
        return;
    }

    adjustSize();
    show();
    raise();
}

HeadsUpViewBar::Mode HeadsUpViewBar::mode() const
{
    return m_mode;
}

void HeadsUpViewBar::setDrawingCommands(const std::vector<std::string>& commands)
{
    m_drawingCommands = commands;
    if (m_mode == Mode::Drawing2D) {
        rebuildActions();
        adjustSize();
    }
}

void HeadsUpViewBar::setBadgeWidget(QWidget* badge)
{
    if (m_badgeWidget == badge) {
        return;
    }
    if (m_badgeWidget) {
        m_layout->removeWidget(m_badgeWidget);
        m_badgeWidget->setParent(nullptr);
    }
    m_badgeWidget = badge;
    if (m_badgeWidget) {
        m_badgeWidget->setParent(this);
        m_layout->addWidget(m_badgeWidget);
    }
    adjustSize();
}

void HeadsUpViewBar::updateGeometryInHost(QWidget* host)
{
    if (!host) {
        return;
    }
    updateGeometryInRect(host->rect());
}

void HeadsUpViewBar::updateGeometryInRect(const QRect& hostRect)
{
    if (isHidden() || hostRect.isEmpty()) {
        return;
    }
    m_toolBar->adjustSize();
    if (m_layout) {
        m_layout->activate();
    }
    resize(sizeHint());
    const int x = hostRect.left() + std::max(8, (hostRect.width() - width()) / 2);
    move(x, hostRect.top() + kHeadsUpTopMargin);
    raise();
}

int HeadsUpViewBar::actionCount() const
{
    return static_cast<int>(m_toolBar->actions().size());
}

void HeadsUpViewBar::rebuildActions()
{
    m_toolBar->clear();
    if (m_mode == Mode::Hidden || !Application::Instance) {
        return;
    }

    const auto& commands = (m_mode == Mode::Drawing2D) ? m_drawingCommands : default3DCommands();
    CommandManager& cmdMgr = Application::Instance->commandManager();

    bool addedSinceSeparator = false;
    for (const auto& cmdName : commands) {
        if (cmdName == "Separator") {
            if (addedSinceSeparator) {
                m_toolBar->addSeparator();
                addedSinceSeparator = false;
            }
            continue;
        }
        if (cmdMgr.addTo(cmdName.c_str(), m_toolBar)) {
            addedSinceSeparator = true;
        }
    }

    // Remove trailing separator if the last group had no available commands.
    auto actions = m_toolBar->actions();
    if (!actions.isEmpty() && actions.constLast()->isSeparator()) {
        m_toolBar->removeAction(actions.constLast());
    }
}

}  // namespace Gui
