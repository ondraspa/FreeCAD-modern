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
#include <QPainterPath>
#include <QPen>

#include <App/Document.h>
#include <Gui/MainWindow.h>
#include <Mod/Drawing/App/DraftingTools.h>
#include <Mod/TechDraw/App/DrawBrokenView.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/QGIView.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerBreakView.h"

namespace DrawingGui
{

namespace
{

QPainterPath buildZigZagPath(const QPointF& center, double halfSpan, bool vertical)
{
    QPainterPath path;
    const double amp = TechDrawGui::Rez::guiX(2.4);
    const double pitch = TechDrawGui::Rez::guiX(3.0);

    if (vertical) {
        const double x = center.x();
        const double y0 = center.y() - halfSpan;
        const double y1 = center.y() + halfSpan;
        const double ym = center.y();
        path.moveTo(x, y0);
        path.lineTo(x, ym - pitch);
        path.lineTo(x - amp, ym - pitch * 0.5);
        path.lineTo(x + amp, ym + pitch * 0.5);
        path.lineTo(x, ym + pitch);
        path.lineTo(x, y1);
    }
    else {
        const double y = center.y();
        const double x0 = center.x() - halfSpan;
        const double x1 = center.x() + halfSpan;
        const double xm = center.x();
        path.moveTo(x0, y);
        path.lineTo(xm - pitch, y);
        path.lineTo(xm - pitch * 0.5, y - amp);
        path.lineTo(xm + pitch * 0.5, y + amp);
        path.lineTo(xm + pitch, y);
        path.lineTo(x1, y);
    }
    return path;
}

}  // namespace

HandlerBreakView::HandlerBreakView(TechDraw::DrawViewPart* baseView)
    : m_baseView(baseView)
{}

HandlerBreakView::~HandlerBreakView()
{
    clearPreviewItems();
}

void HandlerBreakView::activated()
{
    if (!viewPage || !viewPage->getScene()) {
        quit();
        return;
    }

    createPreviewItems();
    setWidgetCursor(Qt::CrossCursor);

    if (auto* mw = Gui::getMainWindow()) {
        mw->showMessage(
            QObject::tr("Break View: Click first break position on a view • V=Vertical break, H=Horizontal break • Esc to cancel"),
            6000
        );
    }

    const QPoint localCursor = viewPage->mapFromGlobal(QCursor::pos());
    updatePreview(viewPage->mapToScene(localCursor));
}

void HandlerBreakView::deactivated()
{
    clearPreviewItems();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerBreakView::createPreviewItems()
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return;
    }

    QPen zigPen(QColor(220, 38, 38), 2.2, Qt::SolidLine);
    zigPen.setCosmetic(true);

    m_zigZagLine1 = new QGraphicsPathItem();
    m_zigZagLine1->setPen(zigPen);
    m_zigZagLine1->setZValue(ZVALUE::TRACKER + 3);
    m_zigZagLine1->hide();
    scene->addItem(m_zigZagLine1);

    m_zigZagLine2 = new QGraphicsPathItem();
    m_zigZagLine2->setPen(zigPen);
    m_zigZagLine2->setZValue(ZVALUE::TRACKER + 3);
    m_zigZagLine2->hide();
    scene->addItem(m_zigZagLine2);

    m_removedSpanRect = new QGraphicsRectItem();
    QPen spanPen(QColor(220, 38, 38), 1.5, Qt::DashLine);
    spanPen.setCosmetic(true);
    m_removedSpanRect->setPen(spanPen);
    m_removedSpanRect->setBrush(QBrush(QColor(239, 68, 68, 38)));
    m_removedSpanRect->setZValue(ZVALUE::TRACKER + 1);
    m_removedSpanRect->hide();
    scene->addItem(m_removedSpanRect);
}

void HandlerBreakView::clearPreviewItems()
{
    auto removeSafe = [](QGraphicsItem* item) {
        if (item) {
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            delete item;
        }
    };
    removeSafe(m_zigZagLine1);
    removeSafe(m_zigZagLine2);
    removeSafe(m_removedSpanRect);
    m_zigZagLine1 = nullptr;
    m_zigZagLine2 = nullptr;
    m_removedSpanRect = nullptr;
}

TechDraw::DrawViewPart* HandlerBreakView::findViewPartAtScenePos(const QPointF& scenePos) const
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

void HandlerBreakView::updatePreview(const QPointF& cursorScenePos)
{
    m_lastCursorScene = cursorScenePos;

    TechDraw::DrawViewPart* target = m_baseView ? m_baseView : findViewPartAtScenePos(cursorScenePos);
    if (!target) {
        if (m_zigZagLine1) {
            m_zigZagLine1->hide();
        }
        if (m_zigZagLine2) {
            m_zigZagLine2->hide();
        }
        if (m_removedSpanRect) {
            m_removedSpanRect->hide();
        }
        return;
    }

    const QPointF viewCenterScene(
        TechDrawGui::Rez::guiX(target->X.getValue()),
        TechDrawGui::Rez::guiX(-target->Y.getValue())
    );
    const double halfW = TechDrawGui::Rez::guiX(std::max(15.0, target->getBoxX() * 0.55));
    const double halfH = TechDrawGui::Rez::guiX(std::max(15.0, target->getBoxY() * 0.55));

    if (m_stage == Stage::PickFirstCut) {
        const QPointF c1 = m_verticalBreakLine ? QPointF(cursorScenePos.x(), viewCenterScene.y())
                                               : QPointF(viewCenterScene.x(), cursorScenePos.y());
        if (m_zigZagLine1) {
            m_zigZagLine1->setPath(
                buildZigZagPath(c1, m_verticalBreakLine ? halfH : halfW, m_verticalBreakLine)
            );
            m_zigZagLine1->show();
        }
        if (m_zigZagLine2) {
            m_zigZagLine2->hide();
        }
        if (m_removedSpanRect) {
            m_removedSpanRect->hide();
        }
        return;
    }

    // Stage::PickSecondCut
    const QPointF c1 = m_verticalBreakLine ? QPointF(m_cut1Scene.x(), viewCenterScene.y())
                                           : QPointF(viewCenterScene.x(), m_cut1Scene.y());
    const QPointF c2 = m_verticalBreakLine ? QPointF(cursorScenePos.x(), viewCenterScene.y())
                                           : QPointF(viewCenterScene.x(), cursorScenePos.y());
    if (m_zigZagLine1) {
        m_zigZagLine1->setPath(
            buildZigZagPath(c1, m_verticalBreakLine ? halfH : halfW, m_verticalBreakLine)
        );
        m_zigZagLine1->show();
    }
    if (m_zigZagLine2) {
        m_zigZagLine2->setPath(
            buildZigZagPath(c2, m_verticalBreakLine ? halfH : halfW, m_verticalBreakLine)
        );
        m_zigZagLine2->show();
    }
    if (m_removedSpanRect) {
        if (m_verticalBreakLine) {
            const double left = std::min(c1.x(), c2.x());
            const double w = std::abs(c2.x() - c1.x());
            m_removedSpanRect->setRect(left, viewCenterScene.y() - halfH, w, halfH * 2.0);
        }
        else {
            const double top = std::min(c1.y(), c2.y());
            const double h = std::abs(c2.y() - c1.y());
            m_removedSpanRect->setRect(viewCenterScene.x() - halfW, top, halfW * 2.0, h);
        }
        m_removedSpanRect->show();
    }
}

void HandlerBreakView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    updatePreview(viewPage->mapToScene(event->pos()));
    event->accept();
}

void HandlerBreakView::mouseReleaseEvent(QMouseEvent* event)
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

    if (m_stage == Stage::PickFirstCut) {
        if (!m_baseView) {
            m_baseView = findViewPartAtScenePos(scenePos);
        }
        if (!m_baseView) {
            event->accept();
            return;
        }
        m_cut1Scene = scenePos;
        m_stage = Stage::PickSecondCut;
        if (auto* mw = Gui::getMainWindow()) {
            mw->showMessage(
                QObject::tr("Click second break position to remove the intermediate span"),
                4000
            );
        }
        updatePreview(scenePos);
        event->accept();
        return;
    }

    TechDraw::DrawPage* page = getPage();
    if (!page || !page->getDocument() || !m_baseView) {
        quit();
        return;
    }

    const double scale = std::max(1e-6, m_baseView->getScale());
    const double viewX = m_baseView->X.getValue();
    const double viewY = m_baseView->Y.getValue();

    double cut1Mm = 0.0;
    double cut2Mm = 0.0;
    if (m_verticalBreakLine) {
        cut1Mm = (TechDrawGui::Rez::appX(m_cut1Scene.x()) - viewX) / scale;
        cut2Mm = (TechDrawGui::Rez::appX(scenePos.x()) - viewX) / scale;
    }
    else {
        cut1Mm = (TechDrawGui::Rez::appX(-m_cut1Scene.y()) - viewY) / scale;
        cut2Mm = (TechDrawGui::Rez::appX(-scenePos.y()) - viewY) / scale;
    }

    if (std::abs(cut2Mm - cut1Mm) < 1.0) {
        event->accept();
        return;
    }

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert 2D Break View");
    Drawing::DraftingTools::createBrokenViewFrom2DCut(
        page,
        m_baseView,
        cut1Mm,
        cut2Mm,
        m_verticalBreakLine,
        10.0
    );
    doc->recompute();
    doc->commitTransaction();

    event->accept();
    quit();
}

void HandlerBreakView::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        return;
    }
    if (event->key() == Qt::Key_V) {
        m_verticalBreakLine = true;
        updatePreview(m_lastCursorScene);
        event->accept();
    }
    else if (event->key() == Qt::Key_H) {
        m_verticalBreakLine = false;
        updatePreview(m_lastCursorScene);
        event->accept();
    }
    else if (event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
