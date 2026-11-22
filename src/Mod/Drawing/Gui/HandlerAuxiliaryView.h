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

#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QPointF>

#include <Base/Vector3D.h>
#include <Mod/Drawing/DrawingGlobal.h>
#include <Mod/TechDraw/Gui/TechDrawHandler.h>

namespace TechDraw
{
class DrawViewPart;
}

namespace DrawingGui
{

class DrawingGuiExport HandlerAuxiliaryView: public TechDrawGui::TechDrawHandler
{
public:
    explicit HandlerAuxiliaryView(TechDraw::DrawViewPart* baseView = nullptr);
    ~HandlerAuxiliaryView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Stage
    {
        PickSlantedEdge,
        PlaceAuxiliaryView,
    };

    void createPreviewItems();
    void clearPreviewItems();
    void updatePreview(const QPointF& cursorScenePos);

    TechDraw::DrawViewPart* findViewPartAtScenePos(const QPointF& scenePos) const;
    bool findClosestEdgeOnView(
        TechDraw::DrawViewPart* dvp,
        const QPointF& scenePos,
        QPointF& outEdgeStartScene,
        QPointF& outEdgeEndScene,
        Base::Vector3d& outTangent2DPage
    ) const;
    std::string nextAuxiliarySymbol();

    Stage m_stage {Stage::PickSlantedEdge};
    TechDraw::DrawViewPart* m_baseView {nullptr};
    QPointF m_edgeStartScene;
    QPointF m_edgeEndScene;
    Base::Vector3d m_edgeTangent2DPage {1.0, 0.0, 0.0};
    QPointF m_lastCursorScene;

    QGraphicsLineItem* m_edgeHighlight {nullptr};
    QGraphicsLineItem* m_alignGuide {nullptr};
    QGraphicsRectItem* m_ghostRect {nullptr};
    QGraphicsSimpleTextItem* m_ghostLabel {nullptr};
};

}  // namespace DrawingGui
