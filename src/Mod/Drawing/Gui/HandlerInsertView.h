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
#include <QGraphicsSimpleTextItem>

#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/Drawing/DrawingGlobal.h>
#include <Mod/TechDraw/Gui/TechDrawHandler.h>

namespace App
{
class DocumentObject;
}  // namespace App

namespace DrawingGui
{

/**
 * Tool-first Model View placement handler.
 * Shows a live dashed bounding-box ghost preview following the cursor;
 * left-click places a standalone TechDraw::DrawViewPart and optionally
 * transitions directly into HandlerProjectedView (SolidWorks "Auto-start projected view").
 */
class DrawingGuiExport HandlerInsertView: public TechDrawGui::TechDrawHandler
{
public:
    HandlerInsertView(
        App::DocumentObject* source,
        Drawing::StandardOrientation orientation = Drawing::StandardOrientation::Front,
        bool autoStartProjected = true,
        bool enableCentermarks = true
    );
    ~HandlerInsertView() override;

    void activated() override;
    void deactivated() override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void computePreviewSizeScene();
    void updatePreviewAt(const QPointF& scenePos);
    void clearPreview();
    void cycleOrientation();

    App::DocumentObject* m_source {nullptr};
    Drawing::StandardOrientation m_orientation {Drawing::StandardOrientation::Front};
    bool m_autoStartProjected {true};
    bool m_enableCentermarks {true};

    double m_previewWidthScene {400.0};
    double m_previewHeightScene {300.0};
    QPointF m_lastScenePos;

    QGraphicsRectItem* m_ghostRect {nullptr};
    QGraphicsSimpleTextItem* m_ghostLabel {nullptr};
};

}  // namespace DrawingGui
