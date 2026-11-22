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

#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>

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
#include <Mod/TechDraw/App/DrawViewSection.h>
#include <Mod/TechDraw/App/Geometry.h>
#include <Mod/TechDraw/Gui/QGIView.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerSectionView.h"

namespace DrawingGui
{

HandlerSectionView::HandlerSectionView(TechDraw::DrawViewPart* baseView)
    : m_baseView(baseView)
{}

HandlerSectionView::~HandlerSectionView()
{
    clearPreviewItems();
}

void HandlerSectionView::activated()
{
    if (!viewPage || !viewPage->getScene()) {
        quit();
        return;
    }

    createPreviewItems();
    setWidgetCursor(Qt::CrossCursor);

    if (auto* mw = Gui::getMainWindow()) {
        mw->showMessage(
            QObject::tr("Section View: Click cutting line point on a view • V=Vertical, H=Horizontal, A=Angular, F=Flip • Esc to cancel"),
            6000
        );
    }

    const QPoint localCursor = viewPage->mapFromGlobal(QCursor::pos());
    updatePreview(viewPage->mapToScene(localCursor));
}

void HandlerSectionView::deactivated()
{
    clearPreviewItems();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerSectionView::createPreviewItems()
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return;
    }

    m_cutLine = new QGraphicsLineItem();
    QPen cutPen(QColor(220, 38, 38), 2.5, Qt::DashDotLine);
    cutPen.setCosmetic(true);
    m_cutLine->setPen(cutPen);
    m_cutLine->setZValue(ZVALUE::TRACKER + 2);
    m_cutLine->hide();
    scene->addItem(m_cutLine);

    m_alignGuide = new QGraphicsLineItem();
    QPen guidePen(QColor(14, 165, 233), 1.5, Qt::DotLine);
    guidePen.setCosmetic(true);
    m_alignGuide->setPen(guidePen);
    m_alignGuide->setZValue(ZVALUE::TRACKER);
    m_alignGuide->hide();
    scene->addItem(m_alignGuide);

    m_snapRing = new QGraphicsEllipseItem(-18.0, -18.0, 36.0, 36.0);
    QPen snapPen(QColor(16, 185, 129), 2.0);
    snapPen.setCosmetic(true);
    m_snapRing->setPen(snapPen);
    m_snapRing->setBrush(Qt::NoBrush);
    m_snapRing->setZValue(ZVALUE::TRACKER + 3);
    m_snapRing->hide();
    scene->addItem(m_snapRing);

    m_ghostRect = new QGraphicsRectItem();
    QPen rectPen(QColor(37, 99, 235), 2.0, Qt::DashLine);
    rectPen.setCosmetic(true);
    m_ghostRect->setPen(rectPen);
    m_ghostRect->setBrush(QBrush(QColor(59, 130, 246, 36)));
    m_ghostRect->setZValue(ZVALUE::TRACKER);
    m_ghostRect->hide();
    scene->addItem(m_ghostRect);

    m_ghostLabel = new QGraphicsSimpleTextItem();
    m_ghostLabel->setBrush(QBrush(QColor(220, 38, 38)));
    m_ghostLabel->setZValue(ZVALUE::TRACKER + 3);
    m_ghostLabel->hide();
    scene->addItem(m_ghostLabel);
}

void HandlerSectionView::clearPreviewItems()
{
    auto removeSafe = [](QGraphicsItem* item) {
        if (item) {
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            delete item;
        }
    };
    removeSafe(m_cutLine);
    removeSafe(m_alignGuide);
    removeSafe(m_snapRing);
    removeSafe(m_ghostRect);
    removeSafe(m_ghostLabel);
    m_cutLine = nullptr;
    m_alignGuide = nullptr;
    m_snapRing = nullptr;
    m_ghostRect = nullptr;
    m_ghostLabel = nullptr;
}

TechDraw::DrawViewPart* HandlerSectionView::findViewPartAtScenePos(const QPointF& scenePos) const
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

QPointF HandlerSectionView::snapToViewGeometry(
    TechDraw::DrawViewPart* dvp,
    const QPointF& scenePos,
    bool& outSnapped
) const
{
    outSnapped = false;
    if (!dvp) {
        return scenePos;
    }

    const QPointF viewCenterScene(
        TechDrawGui::Rez::guiX(dvp->X.getValue()),
        TechDrawGui::Rez::guiX(-dvp->Y.getValue())
    );
    const double snapRadiusScene = TechDrawGui::Rez::guiX(4.5);

    double bestDist = snapRadiusScene;
    QPointF bestPt = scenePos;

    // Snap to view center first
    const double centerDist = std::hypot(
        scenePos.x() - viewCenterScene.x(),
        scenePos.y() - viewCenterScene.y()
    );
    if (centerDist < bestDist) {
        bestDist = centerDist;
        bestPt = viewCenterScene;
        outSnapped = true;
    }

    // Snap to projected vertices (including arc/circle center vertices!).
    for (const auto& vert : dvp->getVertexGeometry()) {
        if (!vert) {
            continue;
        }
        const Base::Vector3d pt = vert->point();
        // Vertex geometry in DrawViewPart is already scaled in view coordinates (Y inverted in scene).
        const QPointF vScene(
            viewCenterScene.x() + TechDrawGui::Rez::guiX(pt.x),
            viewCenterScene.y() + TechDrawGui::Rez::guiX(pt.y)
        );
        const double dist = std::hypot(scenePos.x() - vScene.x(), scenePos.y() - vScene.y());
        if (dist < bestDist) {
            bestDist = dist;
            bestPt = vScene;
            outSnapped = true;
        }
    }

    return bestPt;
}

Base::Vector3d HandlerSectionView::computeCutNormal2D() const
{
    const double flip = m_flipDirection ? -1.0 : 1.0;
    if (m_cutOrientation == CutOrientation::Vertical) {
        // Vertical cutting line -> normal points Left (-1, 0) or Right (+1, 0) in page coords (+Y up)
        return Base::Vector3d(1.0 * flip, 0.0, 0.0);
    }
    if (m_cutOrientation == CutOrientation::Horizontal) {
        // Horizontal cutting line -> normal points Up (0, +1) or Down (0, -1)
        return Base::Vector3d(0.0, 1.0 * flip, 0.0);
    }

    // Angular cutting line from m_originScene to m_anglePtScene (convert to page +Y up)
    const double dx = m_anglePtScene.x() - m_originScene.x();
    const double dy = -(m_anglePtScene.y() - m_originScene.y());
    const double len = std::hypot(dx, dy);
    if (len < 1e-6) {
        return Base::Vector3d(1.0 * flip, 0.0, 0.0);
    }
    // Perpendicular to cut line direction (dx, dy)
    return Base::Vector3d((-dy / len) * flip, (dx / len) * flip, 0.0);
}

std::string HandlerSectionView::nextSectionSymbol()
{
    std::set<std::string> used;
    if (TechDraw::DrawPage* page = getPage()) {
        for (App::DocumentObject* obj : page->Views.getValues()) {
            if (auto* sec = dynamic_cast<TechDraw::DrawViewSection*>(obj)) {
                used.insert(sec->SectionSymbol.getValue());
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

void HandlerSectionView::updatePreview(const QPointF& cursorScenePos)
{
    m_lastCursorScene = cursorScenePos;

    if (m_stage == Stage::PickOrigin) {
        TechDraw::DrawViewPart* hovered = m_baseView ? m_baseView : findViewPartAtScenePos(cursorScenePos);
        if (!hovered) {
            if (m_cutLine) {
                m_cutLine->hide();
            }
            if (m_snapRing) {
                m_snapRing->hide();
            }
            if (m_ghostLabel) {
                m_ghostLabel->hide();
            }
            return;
        }

        bool snapped = false;
        const QPointF pt = snapToViewGeometry(hovered, cursorScenePos, snapped);
        if (m_snapRing) {
            m_snapRing->setPos(pt);
            m_snapRing->setVisible(snapped);
        }

        const double span = TechDrawGui::Rez::guiX(
            std::max(30.0, std::max(hovered->getBoxX(), hovered->getBoxY()) * 0.65)
        );
        if (m_cutLine) {
            if (m_cutOrientation == CutOrientation::Vertical) {
                m_cutLine->setLine(pt.x(), pt.y() - span, pt.x(), pt.y() + span);
            }
            else if (m_cutOrientation == CutOrientation::Horizontal) {
                m_cutLine->setLine(pt.x() - span, pt.y(), pt.x() + span, pt.y());
            }
            else {
                m_cutLine->setLine(pt.x() - span * 0.7, pt.y() - span * 0.7, pt.x() + span * 0.7, pt.y() + span * 0.7);
            }
            m_cutLine->show();
        }
        return;
    }

    if (m_stage == Stage::PickAnglePoint && m_baseView) {
        bool snapped = false;
        const QPointF pt = snapToViewGeometry(m_baseView, cursorScenePos, snapped);
        m_anglePtScene = pt;
        if (m_snapRing) {
            m_snapRing->setPos(pt);
            m_snapRing->setVisible(snapped);
        }
        if (m_cutLine) {
            m_cutLine->setLine(QLineF(m_originScene, pt));
            m_cutLine->show();
        }
        return;
    }

    if (m_stage == Stage::PlaceSectionView && m_baseView) {
        if (m_snapRing) {
            m_snapRing->hide();
        }
        const Base::Vector3d normal2D = computeCutNormal2D();
        const Base::Vector3d parentPagePos(m_baseView->X.getValue(), m_baseView->Y.getValue(), 0.0);
        const Base::Vector3d cursorPagePos(
            TechDrawGui::Rez::appX(cursorScenePos.x()),
            TechDrawGui::Rez::appX(-cursorScenePos.y()),
            0.0
        );

        Drawing::AlignmentKind alignKind = Drawing::AlignmentKind::Vector;
        if (m_cutOrientation == CutOrientation::Vertical) {
            alignKind = Drawing::AlignmentKind::Horizontal;
        }
        else if (m_cutOrientation == CutOrientation::Horizontal) {
            alignKind = Drawing::AlignmentKind::Vertical;
        }

        const Base::Vector3d clamped = Drawing::ProjectionMath::clampPointToAlignment(
            parentPagePos,
            cursorPagePos,
            alignKind,
            normal2D
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
            const std::string sym = nextSectionSymbol();
            m_ghostLabel->setText(
                QStringLiteral("SECTION %1-%1").arg(QString::fromStdString(sym))
            );
            QFont f = m_ghostLabel->font();
            f.setPointSizeF(30.0);
            m_ghostLabel->setFont(f);
            m_ghostLabel->setPos(clampedScene.x() - boxW * 0.5, clampedScene.y() - boxH * 0.5 - 50.0);
            m_ghostLabel->show();
        }
    }
}

void HandlerSectionView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    updatePreview(viewPage->mapToScene(event->pos()));
    event->accept();
}

void HandlerSectionView::mouseReleaseEvent(QMouseEvent* event)
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

    if (m_stage == Stage::PickOrigin) {
        if (!m_baseView) {
            m_baseView = findViewPartAtScenePos(scenePos);
        }
        if (!m_baseView) {
            event->accept();
            return;
        }
        bool snapped = false;
        m_originScene = snapToViewGeometry(m_baseView, scenePos, snapped);
        m_stage = (m_cutOrientation == CutOrientation::Angular) ? Stage::PickAnglePoint
                                                                : Stage::PlaceSectionView;
        if (auto* mw = Gui::getMainWindow()) {
            mw->showMessage(
                m_stage == Stage::PickAnglePoint
                    ? QObject::tr("Click second point for angular cutting line")
                    : QObject::tr("Move cursor to place Section View • Press F to flip direction • Click to place"),
                4000
            );
        }
        updatePreview(scenePos);
        event->accept();
        return;
    }

    if (m_stage == Stage::PickAnglePoint) {
        bool snapped = false;
        m_anglePtScene = snapToViewGeometry(m_baseView, scenePos, snapped);
        if (std::hypot(m_anglePtScene.x() - m_originScene.x(), m_anglePtScene.y() - m_originScene.y())
            < 5.0) {
            event->accept();
            return;
        }
        m_stage = Stage::PlaceSectionView;
        updatePreview(scenePos);
        event->accept();
        return;
    }

    // Stage::PlaceSectionView — create DrawViewSection cleanly!
    TechDraw::DrawPage* page = getPage();
    if (!page || !page->getDocument() || !m_baseView) {
        quit();
        return;
    }

    const Base::Vector3d normal2D = computeCutNormal2D();
    const Base::Vector3d parentPagePos(m_baseView->X.getValue(), m_baseView->Y.getValue(), 0.0);
    const Base::Vector3d cursorPagePos(
        TechDrawGui::Rez::appX(scenePos.x()),
        TechDrawGui::Rez::appX(-scenePos.y()),
        0.0
    );

    Drawing::AlignmentKind alignKind = Drawing::AlignmentKind::Vector;
    if (m_cutOrientation == CutOrientation::Vertical) {
        alignKind = Drawing::AlignmentKind::Horizontal;
    }
    else if (m_cutOrientation == CutOrientation::Horizontal) {
        alignKind = Drawing::AlignmentKind::Vertical;
    }

    const Base::Vector3d placedPagePos = Drawing::ProjectionMath::clampPointToAlignment(
        parentPagePos,
        cursorPagePos,
        alignKind,
        normal2D
    );

    // Map 2D clicked cutting origin on base view into 3D model coordinates relative to base centroid.
    const double baseScale = std::max(1e-6, m_baseView->getScale());
    const double localX = (TechDrawGui::Rez::appX(m_originScene.x()) - m_baseView->X.getValue()) / baseScale;
    const double localY = (TechDrawGui::Rez::appX(-m_originScene.y()) - m_baseView->Y.getValue()) / baseScale;

    const Base::Vector3d centroid = m_baseView->getOriginalCentroid();
    const gp_Ax2 baseCS = m_baseView->getProjectionCS(centroid);
    const gp_Dir bx = baseCS.XDirection();
    const gp_Dir by = baseCS.YDirection();
    const Base::Vector3d sectionOrigin3D(
        centroid.x + bx.X() * localX + by.X() * localY,
        centroid.y + bx.Y() * localX + by.Y() * localY,
        centroid.z + bx.Z() * localX + by.Z() * localY
    );

    const std::string sym = nextSectionSymbol();
    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert Section View");

    auto* sec = doc->addObject<TechDraw::DrawViewSection>("SectionView");
    if (sec) {
        sec->Label.setValue("Section " + sym + "-" + sym);
        sec->SectionSymbol.setValue(sym);
        sec->BaseView.setValue(m_baseView);
        sec->Source.setValues(m_baseView->Source.getValues());
        sec->XSource.setValues(m_baseView->XSource.getValues());
        sec->ScaleType.setValue(m_baseView->ScaleType.getValue());
        sec->Scale.setValue(m_baseView->Scale.getValue());
        sec->SectionOrigin.setValue(sectionOrigin3D);
        sec->setCSFromBase(normal2D);
        sec->X.setValue(placedPagePos.x);
        sec->Y.setValue(placedPagePos.y);

        Drawing::ProjectionMath::attachViewAlignment(sec, m_baseView, alignKind, normal2D);
        page->addView(sec, false);
    }

    doc->recompute();
    doc->commitTransaction();

    event->accept();
    quit();
}

void HandlerSectionView::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        return;
    }
    if (event->key() == Qt::Key_V) {
        m_cutOrientation = CutOrientation::Vertical;
        updatePreview(m_lastCursorScene);
        event->accept();
    }
    else if (event->key() == Qt::Key_H) {
        m_cutOrientation = CutOrientation::Horizontal;
        updatePreview(m_lastCursorScene);
        event->accept();
    }
    else if (event->key() == Qt::Key_A) {
        m_cutOrientation = CutOrientation::Angular;
        updatePreview(m_lastCursorScene);
        event->accept();
    }
    else if (event->key() == Qt::Key_F) {
        m_flipDirection = !m_flipDirection;
        updatePreview(m_lastCursorScene);
        event->accept();
    }
    else if (event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
