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
#include <QString>

#include <FCGlobal.h>

class QAction;
class QLabel;
class QVBoxLayout;

namespace Gui
{

/**
 * SolidWorks-style S-Key Shortcut Bar popup spawned at the cursor.
 *
 * Renders a compact multi-row grid of context-relevant commands (Part, Sketch,
 * Assembly, Drawing) and dismisses immediately on command invocation, Escape,
 * or clicking outside.
 */
class GuiExport ShortcutBarPopup: public QFrame
{
    Q_OBJECT

public:
    explicit ShortcutBarPopup(QWidget* parent = nullptr);
    ~ShortcutBarPopup() override = default;

    bool showForContext(
        const QString& contextLabel,
        const std::vector<std::string>& commands,
        const QPoint& globalPos
    );

    bool showForActions(
        const QString& contextLabel,
        const QList<QAction*>& actions,
        const QPoint& globalPos
    );

    int actionCount() const;
    QString headerText() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void clearRows();
    void positionAt(const QPoint& globalPos);

    QLabel* m_headerLabel {nullptr};
    QVBoxLayout* m_rowsLayout {nullptr};
    int m_actionCount {0};
};

}  // namespace Gui
