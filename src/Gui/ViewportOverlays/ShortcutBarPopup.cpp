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
#include <QKeyEvent>
#include <QLabel>
#include <QScreen>
#include <QToolBar>
#include <QVBoxLayout>

#include <Gui/Application.h>
#include <Gui/Command.h>

#include "ShortcutBarPopup.h"

namespace Gui
{

namespace
{

constexpr int kShortcutIconPx = 22;
constexpr int kItemsPerRow = 4;

}  // namespace

ShortcutBarPopup::ShortcutBarPopup(QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("ShortcutBarPopup"));
    setFrameShape(QFrame::StyledPanel);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 5, 6, 6);
    rootLayout->setSpacing(4);

    m_headerLabel = new QLabel(this);
    m_headerLabel->setObjectName(QStringLiteral("shortcutBarHeader"));
    rootLayout->addWidget(m_headerLabel);

    m_rowsLayout = new QVBoxLayout();
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    rootLayout->addLayout(m_rowsLayout);
}

void ShortcutBarPopup::clearRows()
{
    m_actionCount = 0;
    while (QLayoutItem* item = m_rowsLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

bool ShortcutBarPopup::showForContext(
    const QString& contextLabel,
    const std::vector<std::string>& commands,
    const QPoint& globalPos
)
{
    clearRows();
    if (!Application::Instance || commands.empty()) {
        hide();
        return false;
    }

    CommandManager& cmdMgr = Application::Instance->commandManager();

    // Filter to currently registered commands first so rows pack evenly.
    std::vector<std::string> available;
    available.reserve(commands.size());
    for (const auto& cmdName : commands) {
        if (cmdMgr.getCommandByName(cmdName.c_str())) {
            available.push_back(cmdName);
        }
    }

    if (available.empty()) {
        hide();
        return false;
    }

    m_headerLabel->setText(contextLabel);

    QToolBar* currentRow = nullptr;
    int countInRow = 0;

    for (const auto& cmdName : available) {
        if (!currentRow || countInRow >= kItemsPerRow) {
            currentRow = new QToolBar(this);
            currentRow->setObjectName(QStringLiteral("*shortcutBarRow"));
            currentRow->setMovable(false);
            currentRow->setFloatable(false);
            currentRow->setIconSize(QSize(kShortcutIconPx, kShortcutIconPx));
            currentRow->setToolButtonStyle(Qt::ToolButtonIconOnly);
            connect(currentRow, &QToolBar::actionTriggered, this, &ShortcutBarPopup::hide);
            m_rowsLayout->addWidget(currentRow);
            countInRow = 0;
        }
        if (cmdMgr.addTo(cmdName.c_str(), currentRow)) {
            ++countInRow;
            ++m_actionCount;
        }
    }

    if (m_actionCount == 0) {
        hide();
        return false;
    }

    adjustSize();
    positionAt(globalPos);
    show();
    setFocus(Qt::PopupFocusReason);
    return true;
}

bool ShortcutBarPopup::showForActions(
    const QString& contextLabel,
    const QList<QAction*>& actions,
    const QPoint& globalPos
)
{
    clearRows();
    if (actions.isEmpty()) {
        hide();
        return false;
    }

    m_headerLabel->setText(contextLabel);

    QToolBar* currentRow = nullptr;
    int countInRow = 0;

    for (QAction* action : actions) {
        if (!action) {
            continue;
        }
        if (!currentRow || countInRow >= kItemsPerRow) {
            currentRow = new QToolBar(this);
            currentRow->setObjectName(QStringLiteral("*shortcutBarRow"));
            currentRow->setMovable(false);
            currentRow->setFloatable(false);
            currentRow->setIconSize(QSize(kShortcutIconPx, kShortcutIconPx));
            currentRow->setToolButtonStyle(Qt::ToolButtonIconOnly);
            connect(currentRow, &QToolBar::actionTriggered, this, &ShortcutBarPopup::hide);
            m_rowsLayout->addWidget(currentRow);
            countInRow = 0;
        }
        currentRow->addAction(action);
        ++countInRow;
        ++m_actionCount;
    }

    if (m_actionCount == 0) {
        hide();
        return false;
    }

    adjustSize();
    positionAt(globalPos);
    show();
    setFocus(Qt::PopupFocusReason);
    return true;
}

int ShortcutBarPopup::actionCount() const
{
    return m_actionCount;
}

QString ShortcutBarPopup::headerText() const
{
    return m_headerLabel->text();
}

void ShortcutBarPopup::keyPressEvent(QKeyEvent* event)
{
    if (event && (event->key() == Qt::Key_Escape || event->key() == Qt::Key_S)) {
        hide();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void ShortcutBarPopup::positionAt(const QPoint& globalPos)
{
    QPoint target(globalPos.x() - width() / 2, globalPos.y() - height() / 2);

    if (const QScreen* screen = QGuiApplication::screenAt(globalPos)) {
        const QRect avail = screen->availableGeometry();
        const int clampedX = std::clamp(
            target.x(),
            avail.left() + 4,
            std::max(avail.left() + 4, avail.right() - width() - 4)
        );
        const int clampedY = std::clamp(
            target.y(),
            avail.top() + 4,
            std::max(avail.top() + 4, avail.bottom() - height() - 4)
        );
        target = QPoint(clampedX, clampedY);
    }

    move(target);
}

}  // namespace Gui
