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
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewDetail.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/QGIView.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerDetailView.h"

namespace DrawingGui
{

HandlerDetailView::HandlerDetailView(TechDraw::DrawViewPart* baseView)
    : m_baseView(baseView)
{}

HandlerDetailView::~HandlerDetailView()
{
    clearPreviewItems();
}

void HandlerDetailView::activated()
{
    if (!viewPage || !viewPage->getScene()) {
        quit();
        return;
    }

    createPreviewItems();
    setWidgetCursor(Qt::CrossCursor);

    if (auto* mw = Gui::getMainWindow()) {
        mw->showMessage(
            QObject::tr("Detail View: Click center on a view, drag radius, then click to place magnified view • Esc to cancel"),
            6000
        );
    }
}

void HandlerDetailView::deactivated()
{
    clearPreviewItems();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerDetailView::createPreviewItems()
{
    auto* scene = viewPage ? viewPage->getScene() : nullptr;
    if (!scene) {
        return;
    }

    m_detailCircle = new QGraphicsEllipseItem();
    QPen pen(QColor(220, 38, 38), 2.0, Qt::DashLine);
    pen.setCosmetic(true);
    m_detailCircle->setPen(pen);
    m_detailCircle->setBrush(QBrush(QColor(220, 38, 38, 22)));
    m_detailCircle->setZValue(ZVALUE::TRACKER + 2);
    m_detailCircle->hide();
    scene->addItem(m_detailCircle);

    m_ghostCircle = new QGraphicsEllipseItem();
    QPen ghostPen(QColor(37, 99, 235), 2.0, Qt::DashLine);
    ghostPen.setCosmetic(true);
    m_ghostCircle->setPen(ghostPen);
    m_ghostCircle->setBrush(QBrush(QColor(59, 130, 246, 36)));
    m_ghostCircle->setZValue(ZVALUE::TRACKER);
    m_ghostCircle->hide();
    scene->addItem(m_ghostCircle);

    m_ghostLabel = new QGraphicsSimpleTextItem();
    m_ghostLabel->setBrush(QBrush(QColor(37, 99, 235)));
    m_ghostLabel->setZValue(ZVALUE::TRACKER + 3);
    m_ghostLabel->hide();
    scene->addItem(m_ghostLabel);
}

void HandlerDetailView::clearPreviewItems()
{
    auto removeSafe = [](QGraphicsItem* item) {
        if (item) {
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            delete item;
        }
    };
    removeSafe(m_detailCircle);
    removeSafe(m_ghostCircle);
    removeSafe(m_ghostLabel);
    m_detailCircle = nullptr;
    m_ghostCircle = nullptr;
    m_ghostLabel = nullptr;
}

TechDraw::DrawViewPart* HandlerDetailView::findViewPartAtScenePos(const QPointF& scenePos) const
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

std::string HandlerDetailView::nextDetailReference()
{
    std::set<std::string> used;
    if (TechDraw::DrawPage* page = getPage()) {
        for (App::DocumentObject* obj : page->Views.getValues()) {
            if (auto* det = dynamic_cast<TechDraw::DrawViewDetail*>(obj)) {
                used.insert(det->Reference.getValue());
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

void HandlerDetailView::updatePreview(const QPointF& cursorScenePos)
{
    if (m_stage == Stage::PickRadius) {
        m_radiusScene = std::max(
            TechDrawGui::Rez::guiX(2.0),
            std::hypot(cursorScenePos.x() - m_centerScene.x(), cursorScenePos.y() - m_centerScene.y())
        );
        if (m_detailCircle) {
            m_detailCircle->setRect(
                m_centerScene.x() - m_radiusScene,
                m_centerScene.y() - m_radiusScene,
                m_radiusScene * 2.0,
                m_radiusScene * 2.0
            );
            m_detailCircle->show();
        }
        return;
    }

    if (m_stage == Stage::PlaceDetail) {
        const double ghostR = m_radiusScene * m_scaleFactor;
        if (m_ghostCircle) {
            m_ghostCircle->setRect(
                cursorScenePos.x() - ghostR,
                cursorScenePos.y() - ghostR,
                ghostR * 2.0,
                ghostR * 2.0
            );
            m_ghostCircle->show();
        }
        if (m_ghostLabel) {
            const std::string ref = nextDetailReference();
            m_ghostLabel->setText(
                QStringLiteral("DETAIL %1 (%2x)").arg(QString::fromStdString(ref)).arg(m_scaleFactor, 0, 'g', 2)
            );
            QFont f = m_ghostLabel->font();
            f.setPointSizeF(30.0);
            m_ghostLabel->setFont(f);
            m_ghostLabel->setPos(cursorScenePos.x() - ghostR, cursorScenePos.y() - ghostR - 50.0);
            m_ghostLabel->show();
        }
    }
}

void HandlerDetailView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    updatePreview(viewPage->mapToScene(event->pos()));
    event->accept();
}

void HandlerDetailView::mouseReleaseEvent(QMouseEvent* event)
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

    if (m_stage == Stage::PickCenter) {
        if (!m_baseView) {
            m_baseView = findViewPartAtScenePos(scenePos);
        }
        if (!m_baseView) {
            event->accept();
            return;
        }
        m_centerScene = scenePos;
        m_stage = Stage::PickRadius;
        if (auto* mw = Gui::getMainWindow()) {
            mw->showMessage(QObject::tr("Move outward to set detail radius and click"), 4000);
        }
        updatePreview(scenePos);
        event->accept();
        return;
    }

    if (m_stage == Stage::PickRadius) {
        m_radiusScene = std::max(
            TechDrawGui::Rez::guiX(2.0),
            std::hypot(scenePos.x() - m_centerScene.x(), scenePos.y() - m_centerScene.y())
        );
        m_stage = Stage::PlaceDetail;
        if (auto* mw = Gui::getMainWindow()) {
            mw->showMessage(QObject::tr("Click where to place the magnified Detail View"), 4000);
        }
        updatePreview(scenePos);
        event->accept();
        return;
    }

    // Stage::PlaceDetail — create TechDraw::DrawViewDetail!
    TechDraw::DrawPage* page = getPage();
    if (!page || !page->getDocument() || !m_baseView) {
        quit();
        return;
    }

    const double baseScale = std::max(1e-6, m_baseView->getScale());
    const QPointF basePosScene(
        TechDrawGui::Rez::guiX(m_baseView->X.getValue()),
        TechDrawGui::Rez::guiX(-m_baseView->Y.getValue())
    );
    const QPointF displaceScene = m_centerScene - basePosScene;
    const Base::Vector3d anchorUnscaled(
        TechDrawGui::Rez::appX(displaceScene.x() / baseScale),
        TechDrawGui::Rez::appX(-displaceScene.y() / baseScale),
        0.0
    );
    const double radiusUnscaled = TechDrawGui::Rez::appX(m_radiusScene / baseScale);
    const std::string ref = nextDetailReference();

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert Detail View");

    auto* detail = doc->addObject<TechDraw::DrawViewDetail>("DetailView");
    if (detail) {
        detail->Label.setValue("Detail " + ref);
        detail->Reference.setValue(ref);
        detail->BaseView.setValue(m_baseView);
        detail->Source.setValues(m_baseView->Source.getValues());
        detail->XSource.setValues(m_baseView->XSource.getValues());
        detail->Direction.setValue(m_baseView->Direction.getValue());
        detail->XDirection.setValue(m_baseView->getXDirection());
        detail->AnchorPoint.setValue(anchorUnscaled);
        detail->Radius.setValue(radiusUnscaled);
        detail->ScaleType.setValue("Custom");
        detail->Scale.setValue(baseScale * m_scaleFactor);
        detail->X.setValue(TechDrawGui::Rez::appX(scenePos.x()));
        detail->Y.setValue(TechDrawGui::Rez::appX(-scenePos.y()));
        detail->Caption.setValue(detail->makeCaption());
        page->addView(detail, false);
    }

    doc->recompute();
    doc->commitTransaction();

    event->accept();
    quit();
}

void HandlerDetailView::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
