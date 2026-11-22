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
#include <Gui/Application.h>
#include <Gui/MainWindow.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/QGIView.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ViewProviderViewPart.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerProjectedView.h"

namespace DrawingGui
{

HandlerProjectedView::HandlerProjectedView(TechDraw::DrawViewPart* parentView)
    : m_parentView(parentView)
{}

HandlerProjectedView::~HandlerProjectedView()
{
    clearPreviewItems();
}

void HandlerProjectedView::activated()
{
    if (!viewPage || !viewPage->getScene()) {
        quit();
        return;
    }

    createPreviewItems();
    setWidgetCursor(Qt::CrossCursor);

    if (auto* mw = Gui::getMainWindow()) {
        if (m_parentView) {
            mw->showMessage(
                QObject::tr("Move cursor in 8 directions from %1 and click to project • Right-click or Esc to finish")
                    .arg(QString::fromUtf8(m_parentView->Label.getValue())),
                5000
            );
        }
        else {
            mw->showMessage(
                QObject::tr("Click a drawing view to project from • Esc to cancel"),
                5000
            );
        }
    }

    if (m_parentView) {
        const QPoint localCursor = viewPage->mapFromGlobal(QCursor::pos());
        updatePreview(viewPage->mapToScene(localCursor));
    }
}

void HandlerProjectedView::deactivated()
{
    clearPreviewItems();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerProjectedView::createPreviewItems()
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return;
    }

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
    m_ghostLabel->setZValue(ZVALUE::TRACKER + 1);
    m_ghostLabel->hide();
    scene->addItem(m_ghostLabel);
}

void HandlerProjectedView::clearPreviewItems()
{
    auto removeItemSafe = [](QGraphicsItem* item) {
        if (item) {
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            delete item;
        }
    };
    removeItemSafe(m_alignGuide);
    removeItemSafe(m_ghostRect);
    removeItemSafe(m_ghostLabel);
    m_alignGuide = nullptr;
    m_ghostRect = nullptr;
    m_ghostLabel = nullptr;
}

TechDraw::DrawViewPart* HandlerProjectedView::findViewPartAtScenePos(const QPointF& scenePos) const
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
        const QRectF sceneRect = qgiv->mapRectToScene(qgiv->boundingRect());
        if (sceneRect.contains(scenePos)) {
            return dvp;
        }
    }
    return nullptr;
}

void HandlerProjectedView::updatePreview(const QPointF& cursorScenePos)
{
    if (!m_parentView || !m_alignGuide || !m_ghostRect || !m_ghostLabel) {
        return;
    }

    TechDraw::DrawPage* page = getPage();
    const bool thirdAngle = page && (page->ProjectionType.getValue() == 1);

    const Base::Vector3d parentPagePos(m_parentView->X.getValue(), m_parentView->Y.getValue(), 0.0);
    const Base::Vector3d cursorPagePos(
        TechDrawGui::Rez::appX(cursorScenePos.x()),
        TechDrawGui::Rez::appX(-cursorScenePos.y()),
        0.0
    );

    const double dx = cursorPagePos.x - parentPagePos.x;
    const double dy = cursorPagePos.y - parentPagePos.y;
    if (std::hypot(dx, dy) < 8.0) {
        m_alignGuide->hide();
        m_ghostRect->hide();
        m_ghostLabel->hide();
        return;
    }

    const auto dir8 = Drawing::ProjectionMath::classifyDirection8(dx, dy);
    const auto spec = Drawing::ProjectionMath::computeProjectedView(
        m_parentView->Direction.getValue(),
        m_parentView->getXDirection(),
        dir8,
        thirdAngle
    );

    const Base::Vector3d alignedPagePos = Drawing::ProjectionMath::clampPointToAlignment(
        parentPagePos,
        cursorPagePos,
        spec.alignment,
        spec.alignmentVector
    );

    const QPointF parentScene(
        TechDrawGui::Rez::guiX(parentPagePos.x),
        TechDrawGui::Rez::guiX(-parentPagePos.y)
    );
    const QPointF alignedScene(
        TechDrawGui::Rez::guiX(alignedPagePos.x),
        TechDrawGui::Rez::guiX(-alignedPagePos.y)
    );

    const double boxW = TechDrawGui::Rez::guiX(std::clamp(m_parentView->getBoxX(), 20.0, 220.0));
    const double boxH = TechDrawGui::Rez::guiX(std::clamp(m_parentView->getBoxY(), 20.0, 180.0));

    m_alignGuide->setLine(QLineF(parentScene, alignedScene));
    m_alignGuide->show();

    m_ghostRect->setRect(
        alignedScene.x() - boxW * 0.5,
        alignedScene.y() - boxH * 0.5,
        boxW,
        boxH
    );
    m_ghostRect->show();

    m_ghostLabel->setText(QString::fromStdString(spec.label));
    QFont font = m_ghostLabel->font();
    font.setPointSizeF(32.0);
    m_ghostLabel->setFont(font);
    m_ghostLabel->setPos(alignedScene.x() - boxW * 0.5, alignedScene.y() - boxH * 0.5 - 52.0);
    m_ghostLabel->show();
}

void HandlerProjectedView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    if (m_parentView) {
        updatePreview(viewPage->mapToScene(event->pos()));
    }
    event->accept();
}

void HandlerProjectedView::mouseReleaseEvent(QMouseEvent* event)
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

    // Tool-first selection of parent view if none was preselected.
    if (!m_parentView) {
        m_parentView = findViewPartAtScenePos(scenePos);
        if (m_parentView) {
            if (auto* mw = Gui::getMainWindow()) {
                mw->showMessage(
                    QObject::tr("Projecting from %1 — click to place projected view • Right-click/Esc to finish")
                        .arg(QString::fromUtf8(m_parentView->Label.getValue())),
                    4000
                );
            }
            updatePreview(scenePos);
        }
        event->accept();
        return;
    }

    TechDraw::DrawPage* page = getPage();
    if (!page || !page->getDocument()) {
        quit();
        return;
    }

    const Base::Vector3d parentPagePos(m_parentView->X.getValue(), m_parentView->Y.getValue(), 0.0);
    const Base::Vector3d cursorPagePos(
        TechDrawGui::Rez::appX(scenePos.x()),
        TechDrawGui::Rez::appX(-scenePos.y()),
        0.0
    );
    const double dx = cursorPagePos.x - parentPagePos.x;
    const double dy = cursorPagePos.y - parentPagePos.y;
    if (std::hypot(dx, dy) < 8.0) {
        event->accept();
        return;
    }

    const bool thirdAngle = (page->ProjectionType.getValue() == 1);
    const auto dir8 = Drawing::ProjectionMath::classifyDirection8(dx, dy);
    const auto spec = Drawing::ProjectionMath::computeProjectedView(
        m_parentView->Direction.getValue(),
        m_parentView->getXDirection(),
        dir8,
        thirdAngle
    );
    const Base::Vector3d alignedPagePos = Drawing::ProjectionMath::clampPointToAlignment(
        parentPagePos,
        cursorPagePos,
        spec.alignment,
        spec.alignmentVector
    );

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert Projected View");

    auto* childView = doc->addObject<TechDraw::DrawViewPart>("ProjView");
    if (childView) {
        childView->Label.setValue(spec.label);
        childView->Source.setValues(m_parentView->Source.getValues());
        childView->XSource.setValues(m_parentView->XSource.getValues());
        childView->Direction.setValue(spec.direction);
        childView->XDirection.setValue(spec.xDirection);
        childView->ScaleType.setValue(m_parentView->ScaleType.getValue());
        childView->Scale.setValue(m_parentView->Scale.getValue());
        childView->X.setValue(alignedPagePos.x);
        childView->Y.setValue(alignedPagePos.y);

        // Attach 100% .FCStd-compatible dynamic alignment properties!
        Drawing::ProjectionMath::attachViewAlignment(
            childView,
            m_parentView,
            spec.alignment,
            spec.alignmentVector
        );

        page->addView(childView, false);

        if (Gui::Application::Instance) {
            auto* parentVp = dynamic_cast<TechDrawGui::ViewProviderViewPart*>(
                Gui::Application::Instance->getViewProvider(m_parentView)
            );
            auto* childVp = dynamic_cast<TechDrawGui::ViewProviderViewPart*>(
                Gui::Application::Instance->getViewProvider(childView)
            );
            if (parentVp && childVp) {
                childVp->ArcCenterMarks.setValue(parentVp->ArcCenterMarks.getValue());
            }
        }
    }

    doc->recompute();
    doc->commitTransaction();

    // Stay active for effortless multi-view projection (Right-click or Esc exits).
    event->accept();
}

void HandlerProjectedView::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
