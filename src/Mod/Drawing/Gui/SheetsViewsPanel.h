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

#include <QWidget>
#include <fastsignals/signal.h>

#include <Mod/Drawing/DrawingGlobal.h>

class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace App
{
class Document;
class DocumentObject;
}  // namespace App

namespace DrawingGui
{

/**
 * Onshape-inspired Left Dock displaying all Drawing Sheets (TechDraw::DrawPage)
 * and their view hierarchy (nesting projected/section/detail child views under parents),
 * with quick Add Sheet, Sheet Properties, Update Drawing, and alignment context actions.
 */
class DrawingGuiExport SheetsViewsPanel: public QWidget
{
    Q_OBJECT

public:
    explicit SheetsViewsPanel(QWidget* parent = nullptr);
    ~SheetsViewsPanel() override;

public Q_SLOTS:
    void rebuildTree();
    void updateDirtyIndicator();

private Q_SLOTS:
    void onAddSheetClicked();
    void onPropertiesClicked();
    void onUpdateClicked();
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenuRequested(const QPoint& pos);

private:
    App::DocumentObject* objectForItem(QTreeWidgetItem* item) const;

    void scheduleRebuild();

    QPushButton* m_addSheetBtn {nullptr};
    QPushButton* m_propsBtn {nullptr};
    QPushButton* m_updateBtn {nullptr};
    QTreeWidget* m_tree {nullptr};
    bool m_rebuildScheduled {false};

    fastsignals::scoped_connection m_connNewObj;
    fastsignals::scoped_connection m_connDelObj;
    fastsignals::scoped_connection m_connChangedObj;
    fastsignals::scoped_connection m_connRelabelObj;
    fastsignals::scoped_connection m_connActiveDoc;
};

}  // namespace DrawingGui
