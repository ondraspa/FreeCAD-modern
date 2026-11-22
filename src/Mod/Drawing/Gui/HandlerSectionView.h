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
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>

#include <Base/Vector3D.h>
#include <Mod/Drawing/DrawingGlobal.h>
#include <Mod/TechDraw/Gui/TechDrawHandler.h>

namespace TechDraw
{
class DrawViewPart;
}  // namespace TechDraw

namespace DrawingGui
{

/**
 * Interactive tool-first 2D Section View handler.
 * 1. Hover over any DrawViewPart — snaps to projected vertices and arc/circle centers.
 *    Press V (vertical cut, default), H (horizontal cut), or A (angular cut); F flips arrow direction.
 * 2. Click anchor point on the view (and second point if in Angular mode).
 * 3. Drag perpendicularly to place the aligned TechDraw::DrawViewSection and click to commit.
 */
class DrawingGuiExport HandlerSectionView: public TechDrawGui::TechDrawHandler
{
public:
    enum class CutOrientation
    {
        Vertical,
        Horizontal,
        Angular,
    };

    explicit HandlerSectionView(TechDraw::DrawViewPart* baseView = nullptr);
    ~HandlerSectionView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Stage
    {
        PickOrigin,
        PickAnglePoint,
        PlaceSectionView,
    };

    void createPreviewItems();
    void clearPreviewItems();
    TechDraw::DrawViewPart* findViewPartAtScenePos(const QPointF& scenePos) const;
    QPointF snapToViewGeometry(TechDraw::DrawViewPart* dvp, const QPointF& scenePos, bool& outSnapped) const;
    Base::Vector3d computeCutNormal2D() const;
    void updatePreview(const QPointF& cursorScenePos);
    std::string nextSectionSymbol();

    TechDraw::DrawViewPart* m_baseView {nullptr};
    Stage m_stage {Stage::PickOrigin};
    CutOrientation m_cutOrientation {CutOrientation::Vertical};
    bool m_flipDirection {false};

    QPointF m_originScene;
    QPointF m_anglePtScene;
    QPointF m_lastCursorScene;

    QGraphicsLineItem* m_cutLine {nullptr};
    QGraphicsLineItem* m_alignGuide {nullptr};
    QGraphicsEllipseItem* m_snapRing {nullptr};
    QGraphicsRectItem* m_ghostRect {nullptr};
    QGraphicsSimpleTextItem* m_ghostLabel {nullptr};
};

}  // namespace DrawingGui
