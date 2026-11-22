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
#include <set>

#include <QBrush>
#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPen>

#include <App/Document.h>
#include <Gui/MainWindow.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/Geometry.h>
#include <Mod/TechDraw/Gui/QGIView.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerAuxiliaryView.h"

namespace DrawingGui
{

namespace
{

double pointToSegmentDistance(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const double vx = b.x() - a.x();
    const double vy = b.y() - a.y();
    const double len2 = vx * vx + vy * vy;
    if (len2 < 1e-6) {
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    const double t = std::clamp(((p.x() - a.x()) * vx + (p.y() - a.y()) * vy) / len2, 0.0, 1.0);
    const double projX = a.x() + t * vx;
    const double projY = a.y() + t * vy;
    return std::hypot(p.x() - projX, p.y() - projY);
}

}  // namespace

HandlerAuxiliaryView::HandlerAuxiliaryView(TechDraw::DrawViewPart* baseView)
    : m_baseView(baseView)
{}

HandlerAuxiliaryView::~HandlerAuxiliaryView()
{
    clearPreviewItems();
}

void HandlerAuxiliaryView::activated()
{
    if (!viewPage || !viewPage->getScene()) {
        quit();
        return;
    }

    createPreviewItems();
    setWidgetCursor(Qt::CrossCursor);

    if (auto* mw = Gui::getMainWindow()) {
        mw->showMessage(
            QObject::tr("Auxiliary View: Click a slanted or straight edge on any view • Esc to cancel"),
            6000
        );
    }

    const QPoint localCursor = viewPage->mapFromGlobal(QCursor::pos());
    updatePreview(viewPage->mapToScene(localCursor));
}

void HandlerAuxiliaryView::deactivated()
{
    clearPreviewItems();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerAuxiliaryView::createPreviewItems()
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return;
    }

    m_edgeHighlight = new QGraphicsLineItem();
    QPen edgePen(QColor(16, 185, 129), 3.0, Qt::SolidLine);
    edgePen.setCosmetic(true);
    m_edgeHighlight->setPen(edgePen);
    m_edgeHighlight->setZValue(ZVALUE::TRACKER + 3);
    m_edgeHighlight->hide();
    scene->addItem(m_edgeHighlight);

    m_alignGuide = new QGraphicsLineItem();
    QPen guidePen(QColor(14, 165, 233), 1.5, Qt::DotLine);
    guidePen.setCosmetic(true);
    m_alignGuide->setPen(guidePen);
    m_alignGuide->setZValue(ZVALUE::TRACKER);
    m_alignGuide->hide();
    scene->addItem(m_alignGuide);

    m_ghostRect = new QGraphicsRectItem();
    QPen rectPen(QColor(37, 99, 235), 2.0, Qt::DashLine);
    rectPen.setCosmetic(true);
    m_ghostRect->setPen(rectPen);
    m_ghostRect->setBrush(QBrush(QColor(59, 130, 246, 36)));
    m_ghostRect->setZValue(ZVALUE::TRACKER);
    m_ghostRect->hide();
    scene->addItem(m_ghostRect);

    m_ghostLabel = new QGraphicsSimpleTextItem();
    m_ghostLabel->setBrush(QBrush(QColor(37, 99, 235)));
    m_ghostLabel->setZValue(ZVALUE::TRACKER + 2);
    m_ghostLabel->hide();
    scene->addItem(m_ghostLabel);
}

void HandlerAuxiliaryView::clearPreviewItems()
{
    auto removeSafe = [](QGraphicsItem* item) {
        if (item) {
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            delete item;
        }
    };
    removeSafe(m_edgeHighlight);
    removeSafe(m_alignGuide);
    removeSafe(m_ghostRect);
    removeSafe(m_ghostLabel);
    m_edgeHighlight = nullptr;
    m_alignGuide = nullptr;
    m_ghostRect = nullptr;
    m_ghostLabel = nullptr;
}

TechDraw::DrawViewPart* HandlerAuxiliaryView::findViewPartAtScenePos(const QPointF& scenePos) const
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

bool HandlerAuxiliaryView::findClosestEdgeOnView(
    TechDraw::DrawViewPart* dvp,
    const QPointF& scenePos,
    QPointF& outEdgeStartScene,
    QPointF& outEdgeEndScene,
    Base::Vector3d& outTangent2DPage
) const
{
    if (!dvp) {
        return false;
    }

    const QPointF viewCenterScene(
        TechDrawGui::Rez::guiX(dvp->X.getValue()),
        TechDrawGui::Rez::guiX(-dvp->Y.getValue())
    );

    double bestDist = 1e12;
    bool found = false;

    for (const auto& geom : dvp->getEdgeGeometry()) {
        if (!geom) {
            continue;
        }
        const Base::Vector3d sp = geom->getStartPoint();
        const Base::Vector3d ep = geom->getEndPoint();
        const QPointF a(
            viewCenterScene.x() + TechDrawGui::Rez::guiX(sp.x),
            viewCenterScene.y() + TechDrawGui::Rez::guiX(sp.y)
        );
        const QPointF b(
            viewCenterScene.x() + TechDrawGui::Rez::guiX(ep.x),
            viewCenterScene.y() + TechDrawGui::Rez::guiX(ep.y)
        );
        if (std::hypot(b.x() - a.x(), b.y() - a.y()) < 1e-3) {
            continue;
        }
        const double d = pointToSegmentDistance(scenePos, a, b);
        if (d < bestDist) {
            bestDist = d;
            outEdgeStartScene = a;
            outEdgeEndScene = b;
            // Convert scene delta (+Y down) into page-space tangent (+Y up)
            Base::Vector3d tang(b.x() - a.x(), -(b.y() - a.y()), 0.0);
            if (tang.Length() > 1e-6) {
                tang.Normalize();
            }
            outTangent2DPage = tang;
            found = true;
        }
    }
    return found;
}

std::string HandlerAuxiliaryView::nextAuxiliarySymbol()
{
    std::set<std::string> used;
    if (TechDraw::DrawPage* page = getPage()) {
        for (App::DocumentObject* obj : page->Views.getValues()) {
            if (!obj) {
                continue;
            }
            const std::string lbl = obj->Label.getStrValue();
            if (lbl.rfind("Auxiliary ", 0) == 0 && lbl.size() > 10) {
                used.insert(lbl.substr(10));
            }
        }
    }
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string sym(1, c);
        if (!used.contains(sym)) {
            return sym;
        }
    }
    return "AA";
}

void HandlerAuxiliaryView::updatePreview(const QPointF& cursorScenePos)
{
    m_lastCursorScene = cursorScenePos;

    if (m_stage == Stage::PickSlantedEdge) {
        TechDraw::DrawViewPart* hovered = m_baseView ? m_baseView : findViewPartAtScenePos(cursorScenePos);
        QPointF a;
        QPointF b;
        Base::Vector3d tang;
        if (hovered && findClosestEdgeOnView(hovered, cursorScenePos, a, b, tang)) {
            if (m_edgeHighlight) {
                m_edgeHighlight->setLine(QLineF(a, b));
                m_edgeHighlight->show();
            }
        }
        else if (m_edgeHighlight) {
            m_edgeHighlight->hide();
        }
        return;
    }

    if (m_stage == Stage::PlaceAuxiliaryView && m_baseView) {
        TechDraw::DrawPage* page = getPage();
        const bool thirdAngle = page && page->ProjectionType.getValue() == 1;

        const Base::Vector3d parentPagePos(m_baseView->X.getValue(), m_baseView->Y.getValue(), 0.0);
        const Base::Vector3d cursorPagePos(
            TechDrawGui::Rez::appX(cursorScenePos.x()),
            TechDrawGui::Rez::appX(-cursorScenePos.y()),
            0.0
        );
        const Base::Vector3d offset = cursorPagePos - parentPagePos;
        const std::string sym = nextAuxiliarySymbol();

        const Drawing::ProjectedViewSpec spec = Drawing::ProjectionMath::computeAuxiliaryView(
            m_baseView->Direction.getValue(),
            m_baseView->XDirection.getValue(),
            m_edgeTangent2DPage,
            offset,
            thirdAngle,
            sym
        );

        const Base::Vector3d clamped = Drawing::ProjectionMath::clampPointToAlignment(
            parentPagePos,
            cursorPagePos,
            spec.alignment,
            spec.alignmentVector
        );

        const QPointF parentScene(
            TechDrawGui::Rez::guiX(parentPagePos.x),
            TechDrawGui::Rez::guiX(-parentPagePos.y)
        );
        const QPointF clampedScene(
            TechDrawGui::Rez::guiX(clamped.x),
            TechDrawGui::Rez::guiX(-clamped.y)
        );

        const double boxW = TechDrawGui::Rez::guiX(std::clamp(m_baseView->getBoxX(), 20.0, 220.0));
        const double boxH = TechDrawGui::Rez::guiX(std::clamp(m_baseView->getBoxY(), 20.0, 180.0));

        if (m_alignGuide) {
            m_alignGuide->setLine(QLineF(parentScene, clampedScene));
            m_alignGuide->show();
        }
        if (m_ghostRect) {
            m_ghostRect->setRect(
                clampedScene.x() - boxW * 0.5,
                clampedScene.y() - boxH * 0.5,
                boxW,
                boxH
            );
            m_ghostRect->show();
        }
        if (m_ghostLabel) {
            m_ghostLabel->setText(QString::fromStdString("VIEW " + sym));
            QFont f = m_ghostLabel->font();
            f.setPointSizeF(30.0);
            m_ghostLabel->setFont(f);
            m_ghostLabel->setPos(clampedScene.x() - boxW * 0.5, clampedScene.y() - boxH * 0.5 - 50.0);
            m_ghostLabel->show();
        }
    }
}

void HandlerAuxiliaryView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    updatePreview(viewPage->mapToScene(event->pos()));
    event->accept();
}

void HandlerAuxiliaryView::mouseReleaseEvent(QMouseEvent* event)
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

    if (m_stage == Stage::PickSlantedEdge) {
        TechDraw::DrawViewPart* target = m_baseView ? m_baseView : findViewPartAtScenePos(scenePos);
        if (!target
            || !findClosestEdgeOnView(
                target,
                scenePos,
                m_edgeStartScene,
                m_edgeEndScene,
                m_edgeTangent2DPage
            )) {
            event->accept();
            return;
        }
        m_baseView = target;
        m_stage = Stage::PlaceAuxiliaryView;
        if (auto* mw = Gui::getMainWindow()) {
            mw->showMessage(
                QObject::tr("Move cursor perpendicular to the selected edge and click to place Auxiliary View"),
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

    const bool thirdAngle = page->ProjectionType.getValue() == 1;
    const Base::Vector3d parentPagePos(m_baseView->X.getValue(), m_baseView->Y.getValue(), 0.0);
    const Base::Vector3d cursorPagePos(
        TechDrawGui::Rez::appX(scenePos.x()),
        TechDrawGui::Rez::appX(-scenePos.y()),
        0.0
    );
    const Base::Vector3d offset = cursorPagePos - parentPagePos;
    const std::string sym = nextAuxiliarySymbol();

    const Drawing::ProjectedViewSpec spec = Drawing::ProjectionMath::computeAuxiliaryView(
        m_baseView->Direction.getValue(),
        m_baseView->XDirection.getValue(),
        m_edgeTangent2DPage,
        offset,
        thirdAngle,
        sym
    );

    const Base::Vector3d placedPagePos = Drawing::ProjectionMath::clampPointToAlignment(
        parentPagePos,
        cursorPagePos,
        spec.alignment,
        spec.alignmentVector
    );

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert Auxiliary View");

    auto* auxView = doc->addObject<TechDraw::DrawViewPart>("AuxiliaryView");
    if (auxView) {
        auxView->Label.setValue(spec.label);
        auxView->Source.setValues(m_baseView->Source.getValues());
        auxView->XSource.setValues(m_baseView->XSource.getValues());
        auxView->Direction.setValue(spec.direction);
        auxView->XDirection.setValue(spec.xDirection);
        auxView->ScaleType.setValue(m_baseView->ScaleType.getValue());
        auxView->Scale.setValue(m_baseView->Scale.getValue());
        auxView->X.setValue(placedPagePos.x);
        auxView->Y.setValue(placedPagePos.y);
        auxView->Caption.setValue("VIEW " + sym);

        Drawing::ProjectionMath::attachViewAlignment(
            auxView,
            m_baseView,
            spec.alignment,
            spec.alignmentVector
        );
        page->addView(auxView, false);
    }

    doc->recompute();
    doc->commitTransaction();

    event->accept();
    quit();
}

void HandlerAuxiliaryView::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
