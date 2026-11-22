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

#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QPointF>

#include <Mod/Drawing/DrawingGlobal.h>
#include <Mod/TechDraw/Gui/TechDrawHandler.h>

namespace TechDraw
{
class DrawViewPart;
}

namespace DrawingGui
{

class DrawingGuiExport HandlerBreakView: public TechDrawGui::TechDrawHandler
{
public:
    explicit HandlerBreakView(TechDraw::DrawViewPart* baseView = nullptr);
    ~HandlerBreakView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Stage
    {
        PickFirstCut,
        PickSecondCut,
    };

    void createPreviewItems();
    void clearPreviewItems();
    void updatePreview(const QPointF& cursorScenePos);
    TechDraw::DrawViewPart* findViewPartAtScenePos(const QPointF& scenePos) const;

    Stage m_stage {Stage::PickFirstCut};
    TechDraw::DrawViewPart* m_baseView {nullptr};
    bool m_verticalBreakLine {true};
    QPointF m_cut1Scene;
    QPointF m_lastCursorScene;

    QGraphicsPathItem* m_zigZagLine1 {nullptr};
    QGraphicsPathItem* m_zigZagLine2 {nullptr};
    QGraphicsRectItem* m_removedSpanRect {nullptr};
};

}  // namespace DrawingGui
