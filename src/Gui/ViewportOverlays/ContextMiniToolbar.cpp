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

#include <QAction>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QScreen>
#include <QToolBar>

#include <Gui/Application.h>
#include <Gui/Command.h>

#include "ContextMiniToolbar.h"

namespace Gui
{

namespace
{

constexpr int kMiniBarIconPx = 20;
constexpr int kCursorOffsetX = 16;
constexpr int kCursorOffsetY = 14;

}  // namespace

ContextMiniToolbar::ContextMiniToolbar(QWidget* parent)
    : QFrame(
          parent,
          Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus
      )
{
    setObjectName(QStringLiteral("ContextMiniToolbar"));
    setFrameShape(QFrame::StyledPanel);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 3, 4, 3);
    layout->setSpacing(2);

    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("*contextMiniToolBar"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(kMiniBarIconPx, kMiniBarIconPx));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    connect(m_toolBar, &QToolBar::actionTriggered, this, &ContextMiniToolbar::dismiss);

    layout->addWidget(m_toolBar);
    hide();
}

bool ContextMiniToolbar::showForCommands(
    const std::vector<std::string>& commands,
    const QPoint& globalAnchor
)
{
    m_toolBar->clear();
    if (!Application::Instance || commands.empty()) {
        hide();
        return false;
    }

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
        if (Command* cmd = cmdMgr.getCommandByName(cmdName.c_str())) {
            if (cmd->isActive() && cmdMgr.addTo(cmdName.c_str(), m_toolBar)) {
                addedSinceSeparator = true;
            }
        }
    }

    auto actions = m_toolBar->actions();
    if (!actions.isEmpty() && actions.constLast()->isSeparator()) {
        m_toolBar->removeAction(actions.constLast());
    }

    if (m_toolBar->actions().isEmpty()) {
        hide();
        return false;
    }

    adjustSize();
    positionNear(globalAnchor);
    show();
    raise();
    return true;
}

bool ContextMiniToolbar::showForActions(const QList<QAction*>& actions, const QPoint& globalAnchor)
{
    m_toolBar->clear();
    for (QAction* action : actions) {
        if (action) {
            m_toolBar->addAction(action);
        }
    }
    if (m_toolBar->actions().isEmpty()) {
        hide();
        return false;
    }

    adjustSize();
    positionNear(globalAnchor);
    show();
    raise();
    return true;
}

void ContextMiniToolbar::dismiss()
{
    if (isVisible()) {
        hide();
    }
}

int ContextMiniToolbar::actionCount() const
{
    return static_cast<int>(m_toolBar->actions().size());
}

void ContextMiniToolbar::positionNear(const QPoint& globalAnchor)
{
    QPoint target(globalAnchor.x() + kCursorOffsetX, globalAnchor.y() - height() - kCursorOffsetY);

    if (const QScreen* screen = QGuiApplication::screenAt(globalAnchor)) {
        const QRect avail = screen->availableGeometry();
        if (target.y() < avail.top()) {
            target.setY(globalAnchor.y() + kCursorOffsetY);
        }
        const int clampedX = std::clamp(target.x(), avail.left() + 4, std::max(avail.left() + 4, avail.right() - width() - 4));
        const int clampedY = std::clamp(target.y(), avail.top() + 4, std::max(avail.top() + 4, avail.bottom() - height() - 4));
        target = QPoint(clampedX, clampedY);
    }

    move(target);
}

}  // namespace Gui
