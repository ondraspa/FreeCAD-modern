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

#include <QDialogButtonBox>
#include <QFrame>

#include <FCGlobal.h>

class QToolButton;

namespace Gui
{

namespace TaskView
{
class TaskDialog;
}

/**
 * SolidWorks-style Confirmation Corner anchored at the top-right of the active viewport.
 *
 * Displays Accept (OK / Close) and Cancel controls whenever a TaskDialog or Sketch edit
 * session is active, offset cleanly so it does not collide with the NaviCube.
 */
class GuiExport ConfirmationCorner: public QFrame
{
    Q_OBJECT

public:
    explicit ConfirmationCorner(QWidget* parent = nullptr);
    ~ConfirmationCorner() override = default;

    /// Update button visibility from an active TaskDialog (hides if dialog is nullptr).
    void setActiveDialog(TaskView::TaskDialog* dialog);

    /// Direct button configuration helper (also used by unit tests).
    void configureButtons(QDialogButtonBox::StandardButtons buttons);

    void updateGeometryInHost(QWidget* host, bool avoidTopRightNaviCube = false);
    void updateGeometryInRect(const QRect& hostRect, bool avoidTopRightNaviCube = false);

    bool isAcceptVisible() const;
    bool isRejectVisible() const;

Q_SIGNALS:
    void accepted();
    void rejected();

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();

    QToolButton* m_acceptButton {nullptr};
    QToolButton* m_rejectButton {nullptr};
};

}  // namespace Gui
