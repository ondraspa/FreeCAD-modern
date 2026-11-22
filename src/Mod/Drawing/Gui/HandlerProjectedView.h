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

#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>

#include <Mod/Drawing/DrawingGlobal.h>
#include <Mod/TechDraw/Gui/TechDrawHandler.h>

namespace TechDraw
{
class DrawViewPart;
}  // namespace TechDraw

namespace DrawingGui
{

/**
 * Interactive 8-way Projected View handler.
 * Works tool-first (click a parent view first if none supplied) or immediately
 * after HandlerInsertView. Places standalone TechDraw::DrawViewPart views linked
 * via dynamic alignment properties (ParentView, AlignmentMode, AlignmentVector)
 * supporting unlimited projection chains and breakable alignment.
 */
class DrawingGuiExport HandlerProjectedView: public TechDrawGui::TechDrawHandler
{
public:
    explicit HandlerProjectedView(TechDraw::DrawViewPart* parentView = nullptr);
    ~HandlerProjectedView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void createPreviewItems();
    void clearPreviewItems();
    void updatePreview(const QPointF& cursorScenePos);
    TechDraw::DrawViewPart* findViewPartAtScenePos(const QPointF& scenePos) const;

    TechDraw::DrawViewPart* m_parentView {nullptr};
    QGraphicsLineItem* m_alignGuide {nullptr};
    QGraphicsRectItem* m_ghostRect {nullptr};
    QGraphicsSimpleTextItem* m_ghostLabel {nullptr};
};

}  // namespace DrawingGui
