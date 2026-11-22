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

class DrawingGuiExport HandlerCropView: public TechDrawGui::TechDrawHandler
{
public:
    explicit HandlerCropView(TechDraw::DrawViewPart* targetView = nullptr);
    ~HandlerCropView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Stage
    {
        PickFirstCorner,
        PickSecondCorner,
    };

    void createPreviewItems();
    void clearPreviewItems();
    void updatePreview(const QPointF& cursorScenePos);
    TechDraw::DrawViewPart* findViewPartAtScenePos(const QPointF& scenePos) const;

    Stage m_stage {Stage::PickFirstCorner};
    TechDraw::DrawViewPart* m_targetView {nullptr};
    QPointF m_corner1Scene;

    QGraphicsRectItem* m_cropRect {nullptr};
};

}  // namespace DrawingGui
