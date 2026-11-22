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

#include <algorithm>
#include <cmath>

#include <QBrush>
#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPen>

#include <App/Document.h>
#include <Gui/MainWindow.h>
#include <Mod/Drawing/App/DraftingTools.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewClip.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/QGIView.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerCropView.h"

namespace DrawingGui
{

HandlerCropView::HandlerCropView(TechDraw::DrawViewPart* targetView)
    : m_targetView(targetView)
{}

HandlerCropView::~HandlerCropView()
{
    clearPreviewItems();
}

void HandlerCropView::activated()
{
    if (!viewPage || !viewPage->getScene()) {
        quit();
        return;
    }

    createPreviewItems();
    setWidgetCursor(Qt::CrossCursor);

    if (auto* mw = Gui::getMainWindow()) {
        mw->showMessage(
            QObject::tr("Crop View: Click two opposite corners on a view to crop it in place • Esc to cancel"),
            6000
        );
    }
}

void HandlerCropView::deactivated()
{
    clearPreviewItems();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerCropView::createPreviewItems()
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return;
    }

    m_cropRect = new QGraphicsRectItem();
    QPen rectPen(QColor(16, 185, 129), 2.0, Qt::DashLine);
    rectPen.setCosmetic(true);
    m_cropRect->setPen(rectPen);
    m_cropRect->setBrush(QBrush(QColor(16, 185, 129, 32)));
    m_cropRect->setZValue(ZVALUE::TRACKER + 2);
    m_cropRect->hide();
    scene->addItem(m_cropRect);
}

void HandlerCropView::clearPreviewItems()
{
    if (m_cropRect) {
        if (m_cropRect->scene()) {
            m_cropRect->scene()->removeItem(m_cropRect);
        }
        delete m_cropRect;
        m_cropRect = nullptr;
    }
}

TechDraw::DrawViewPart* HandlerCropView::findViewPartAtScenePos(const QPointF& scenePos) const
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return nullptr;
    }
    for (TechDrawGui::QGIView* qgiv : scene->getViews()) {
        if (!qgiv) {
            continue;
        }
        auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(qgiv->getViewObject());
        if (!dvp) {
            continue;
        }
        if (qgiv->mapRectToScene(qgiv->boundingRect()).contains(scenePos)) {
            return dvp;
        }
    }
    return nullptr;
}

void HandlerCropView::updatePreview(const QPointF& cursorScenePos)
{
    if (m_stage != Stage::PickSecondCorner || !m_cropRect) {
        return;
    }
    const double left = std::min(m_corner1Scene.x(), cursorScenePos.x());
    const double top = std::min(m_corner1Scene.y(), cursorScenePos.y());
    const double w = std::abs(cursorScenePos.x() - m_corner1Scene.x());
    const double h = std::abs(cursorScenePos.y() - m_corner1Scene.y());
    m_cropRect->setRect(left, top, w, h);
    m_cropRect->show();
}

void HandlerCropView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    updatePreview(viewPage->mapToScene(event->pos()));
    event->accept();
}

void HandlerCropView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }
    if (event->button() == Qt::RightButton) {
        quit();
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton || !viewPage) {
        TechDrawHandler::mouseReleaseEvent(event);
        return;
    }

    const QPointF scenePos = viewPage->mapToScene(event->pos());

    if (m_stage == Stage::PickFirstCorner) {
        if (!m_targetView) {
            m_targetView = findViewPartAtScenePos(scenePos);
        }
        if (!m_targetView) {
            event->accept();
            return;
        }
        m_corner1Scene = scenePos;
        m_stage = Stage::PickSecondCorner;
        if (auto* mw = Gui::getMainWindow()) {
            mw->showMessage(
                QObject::tr("Move cursor and click opposite corner to apply in-place Crop View"),
                4000
            );
        }
        updatePreview(scenePos);
        event->accept();
        return;
    }

    TechDraw::DrawPage* page = getPage();
    if (!page || !page->getDocument() || !m_targetView) {
        quit();
        return;
    }

    const double x1 = TechDrawGui::Rez::appX(m_corner1Scene.x());
    const double y1 = TechDrawGui::Rez::appX(-m_corner1Scene.y());
    const double x2 = TechDrawGui::Rez::appX(scenePos.x());
    const double y2 = TechDrawGui::Rez::appX(-scenePos.y());

    const double wMm = std::abs(x2 - x1);
    const double hMm = std::abs(y2 - y1);
    if (wMm < 3.0 || hMm < 3.0) {
        event->accept();
        return;
    }

    const double centerPageX = (x1 + x2) * 0.5;
    const double centerPageY = (y1 + y2) * 0.5;

    App::Document* doc = page->getDocument();
    doc->openTransaction("Crop View In Place");
    Drawing::DraftingTools::cropViewInPlace(
        page,
        m_targetView,
        centerPageX,
        centerPageY,
        wMm,
        hMm,
        false
    );
    doc->recompute();
    doc->commitTransaction();

    event->accept();
    quit();
}

void HandlerCropView::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
