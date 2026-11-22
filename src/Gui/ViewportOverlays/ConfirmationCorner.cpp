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

#include <QEvent>
#include <QHBoxLayout>
#include <QToolButton>

#include <Gui/TaskView/TaskDialog.h>

#include "ConfirmationCorner.h"

namespace Gui
{

namespace
{

constexpr int kCornerMargin = 12;
constexpr int kNaviCubeClearanceY = 148;

}  // namespace

ConfirmationCorner::ConfirmationCorner(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ConfirmationCorner"));
    setFrameShape(QFrame::StyledPanel);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(6);

    m_acceptButton = new QToolButton(this);
    m_acceptButton->setObjectName(QStringLiteral("confirmationCornerAccept"));
    m_acceptButton->setText(QStringLiteral("✓"));
    m_acceptButton->setMinimumSize(32, 32);
    connect(m_acceptButton, &QToolButton::clicked, this, &ConfirmationCorner::accepted);
    layout->addWidget(m_acceptButton);

    m_rejectButton = new QToolButton(this);
    m_rejectButton->setObjectName(QStringLiteral("confirmationCornerReject"));
    m_rejectButton->setText(QStringLiteral("✕"));
    m_rejectButton->setMinimumSize(32, 32);
    connect(m_rejectButton, &QToolButton::clicked, this, &ConfirmationCorner::rejected);
    layout->addWidget(m_rejectButton);

    retranslateUi();
    hide();
}

void ConfirmationCorner::setActiveDialog(TaskView::TaskDialog* dialog)
{
    if (!dialog) {
        hide();
        return;
    }
    configureButtons(dialog->getStandardButtons());
}

void ConfirmationCorner::configureButtons(QDialogButtonBox::StandardButtons buttons)
{
    const bool showAccept = buttons.testFlag(QDialogButtonBox::Ok)
        || buttons.testFlag(QDialogButtonBox::Close);
    const bool showReject = buttons.testFlag(QDialogButtonBox::Cancel);

    m_acceptButton->setVisible(showAccept);
    m_rejectButton->setVisible(showReject);

    if (!showAccept && !showReject) {
        hide();
        return;
    }

    adjustSize();
    show();
    raise();
}

void ConfirmationCorner::updateGeometryInHost(QWidget* host, bool avoidTopRightNaviCube)
{
    if (!host) {
        return;
    }
    updateGeometryInRect(host->rect(), avoidTopRightNaviCube);
}

void ConfirmationCorner::updateGeometryInRect(const QRect& hostRect, bool avoidTopRightNaviCube)
{
    if (isHidden() || hostRect.isEmpty()) {
        return;
    }
    adjustSize();
    const int x = hostRect.left() + std::max(kCornerMargin, hostRect.width() - width() - kCornerMargin);
    const int y = hostRect.top() + (avoidTopRightNaviCube ? kNaviCubeClearanceY : kCornerMargin);
    move(x, y);
    raise();
}

bool ConfirmationCorner::isAcceptVisible() const
{
    return !m_acceptButton->isHidden();
}

bool ConfirmationCorner::isRejectVisible() const
{
    return !m_rejectButton->isHidden();
}

void ConfirmationCorner::retranslateUi()
{
    m_acceptButton->setToolTip(tr("Accept and finish editing"));
    m_rejectButton->setToolTip(tr("Cancel and discard changes"));
}

void ConfirmationCorner::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QFrame::changeEvent(event);
}

}  // namespace Gui
