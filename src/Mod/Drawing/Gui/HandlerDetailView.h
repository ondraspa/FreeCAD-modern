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

#include <QGraphicsEllipseItem>
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
 * Interactive tool-first Detail View handler (3 clicks: center -> radius -> place magnified view).
 * Shows a live dashed detail circle on the parent view and magnified ghost preview at 2x scale.
 */
class DrawingGuiExport HandlerDetailView: public TechDrawGui::TechDrawHandler
{
public:
    explicit HandlerDetailView(TechDraw::DrawViewPart* baseView = nullptr);
    ~HandlerDetailView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Stage
    {
        PickCenter,
        PickRadius,
        PlaceDetail,
    };

    void createPreviewItems();
    void clearPreviewItems();
    TechDraw::DrawViewPart* findViewPartAtScenePos(const QPointF& scenePos) const;
    std::string nextDetailReference();
    void updatePreview(const QPointF& cursorScenePos);

    TechDraw::DrawViewPart* m_baseView {nullptr};
    Stage m_stage {Stage::PickCenter};
    QPointF m_centerScene;
    double m_radiusScene {100.0};
    double m_scaleFactor {2.0};

    QGraphicsEllipseItem* m_detailCircle {nullptr};
    QGraphicsEllipseItem* m_ghostCircle {nullptr};
    QGraphicsSimpleTextItem* m_ghostLabel {nullptr};
};

}  // namespace DrawingGui
