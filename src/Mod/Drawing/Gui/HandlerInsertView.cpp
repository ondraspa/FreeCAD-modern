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
#include <QPointer>
#include <QTimer>

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Base/BoundBox.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/MainWindow.h>
#include <Mod/Drawing/App/TitleBlockBinder.h>
#include <Mod/Part/App/PartFeature.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/QGSPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/Rez.h>
#include <Mod/TechDraw/Gui/ViewProviderViewPart.h>
#include <Mod/TechDraw/Gui/ZVALUE.h>

#include "HandlerInsertView.h"
#include "HandlerProjectedView.h"

namespace DrawingGui
{

namespace
{

Base::BoundBox3d sourceBoundBox(App::DocumentObject* obj)
{
    Base::BoundBox3d bb;
    if (!obj) {
        return bb;
    }
    if (auto* pf = dynamic_cast<Part::Feature*>(obj)) {
        if (!pf->Shape.getValue().IsNull()) {
            return pf->Shape.getBoundingBox();
        }
    }
    for (App::DocumentObject* child : obj->getOutListRecursive()) {
        if (auto* pf = dynamic_cast<Part::Feature*>(child)) {
            if (!pf->Shape.getValue().IsNull()) {
                bb.Add(pf->Shape.getBoundingBox());
            }
        }
    }
    return bb;
}

}  // namespace

HandlerInsertView::HandlerInsertView(
    App::DocumentObject* source,
    Drawing::StandardOrientation orientation,
    bool autoStartProjected,
    bool enableCentermarks
)
    : m_source(source)
    , m_orientation(orientation)
    , m_autoStartProjected(autoStartProjected)
    , m_enableCentermarks(enableCentermarks)
{}

HandlerInsertView::~HandlerInsertView()
{
    clearPreview();
}

void HandlerInsertView::activated()
{
    if (!viewPage || !viewPage->getScene() || !m_source) {
        quit();
        return;
    }

    computePreviewSizeScene();

    auto* scene = viewPage->getScene();
    m_ghostRect = new QGraphicsRectItem();
    QPen pen(QColor(37, 99, 235), 2.0, Qt::DashLine);
    pen.setCosmetic(true);
    m_ghostRect->setPen(pen);
    m_ghostRect->setBrush(QBrush(QColor(59, 130, 246, 36)));
    m_ghostRect->setZValue(ZVALUE::TRACKER);
    scene->addItem(m_ghostRect);

    m_ghostLabel = new QGraphicsSimpleTextItem();
    m_ghostLabel->setBrush(QBrush(QColor(37, 99, 235)));
    m_ghostLabel->setZValue(ZVALUE::TRACKER + 1);
    scene->addItem(m_ghostLabel);

    setWidgetCursor(Qt::CrossCursor);
    if (auto* mw = Gui::getMainWindow()) {
        mw->showMessage(
            QObject::tr("Click to place %1 view • Press R/Tab to cycle orientation • Esc to cancel")
                .arg(QString::fromStdString(Drawing::ProjectionMath::standardOrientation(m_orientation).label)),
            4000
        );
    }

    const QPoint localCursor = viewPage->mapFromGlobal(QCursor::pos());
    updatePreviewAt(viewPage->mapToScene(localCursor));
}

void HandlerInsertView::deactivated()
{
    clearPreview();
    if (viewPage) {
        viewPage->resetCursor();
    }
}

void HandlerInsertView::clearPreview()
{
    if (m_ghostRect) {
        if (m_ghostRect->scene()) {
            m_ghostRect->scene()->removeItem(m_ghostRect);
        }
        delete m_ghostRect;
        m_ghostRect = nullptr;
    }
    if (m_ghostLabel) {
        if (m_ghostLabel->scene()) {
            m_ghostLabel->scene()->removeItem(m_ghostLabel);
        }
        delete m_ghostLabel;
        m_ghostLabel = nullptr;
    }
}

void HandlerInsertView::computePreviewSizeScene()
{
    TechDraw::DrawPage* page = getPage();
    const double scale = page ? page->Scale.getValue() : 1.0;

    double wMm = 60.0;
    double hMm = 45.0;
    const Base::BoundBox3d bb = sourceBoundBox(m_source);
    if (bb.IsValid()) {
        const double lx = std::max(1.0, bb.LengthX());
        const double ly = std::max(1.0, bb.LengthY());
        const double lz = std::max(1.0, bb.LengthZ());
        switch (m_orientation) {
            case Drawing::StandardOrientation::Front:
            case Drawing::StandardOrientation::Back:
                wMm = lx * scale;
                hMm = lz * scale;
                break;
            case Drawing::StandardOrientation::Top:
            case Drawing::StandardOrientation::Bottom:
                wMm = lx * scale;
                hMm = ly * scale;
                break;
            case Drawing::StandardOrientation::Left:
            case Drawing::StandardOrientation::Right:
                wMm = ly * scale;
                hMm = lz * scale;
                break;
            case Drawing::StandardOrientation::Isometric:
                wMm = std::max(lx, ly) * scale * 1.15;
                hMm = std::max(lz, std::min(lx, ly)) * scale * 1.15;
                break;
        }
    }

    m_previewWidthScene = TechDrawGui::Rez::guiX(std::clamp(wMm, 16.0, 260.0));
    m_previewHeightScene = TechDrawGui::Rez::guiX(std::clamp(hMm, 16.0, 220.0));
}

void HandlerInsertView::updatePreviewAt(const QPointF& scenePos)
{
    m_lastScenePos = scenePos;
    if (!m_ghostRect || !m_ghostLabel) {
        return;
    }

    m_ghostRect->setRect(
        scenePos.x() - m_previewWidthScene * 0.5,
        scenePos.y() - m_previewHeightScene * 0.5,
        m_previewWidthScene,
        m_previewHeightScene
    );

    const auto orient = Drawing::ProjectionMath::standardOrientation(m_orientation);
    const TechDraw::DrawPage* page = getPage();
    const QString scaleStr = page
        ? QString::fromStdString(Drawing::TitleBlockBinder::formatScaleRatio(page->Scale.getValue()))
        : QStringLiteral("1:1");
    m_ghostLabel->setText(
        QStringLiteral("%1 (%2)").arg(QString::fromStdString(orient.label), scaleStr)
    );
    QFont font = m_ghostLabel->font();
    font.setPointSizeF(32.0);
    m_ghostLabel->setFont(font);
    m_ghostLabel->setPos(
        scenePos.x() - m_previewWidthScene * 0.5,
        scenePos.y() - m_previewHeightScene * 0.5 - 52.0
    );
}

void HandlerInsertView::cycleOrientation()
{
    static constexpr Drawing::StandardOrientation kOrder[] = {
        Drawing::StandardOrientation::Front,
        Drawing::StandardOrientation::Top,
        Drawing::StandardOrientation::Right,
        Drawing::StandardOrientation::Left,
        Drawing::StandardOrientation::Back,
        Drawing::StandardOrientation::Bottom,
        Drawing::StandardOrientation::Isometric,
    };
    for (std::size_t i = 0; i < std::size(kOrder); ++i) {
        if (kOrder[i] == m_orientation) {
            m_orientation = kOrder[(i + 1) % std::size(kOrder)];
            break;
        }
    }
    computePreviewSizeScene();
    updatePreviewAt(m_lastScenePos);
}

void HandlerInsertView::mouseMoveEvent(QMouseEvent* event)
{
    if (!viewPage || !event) {
        return;
    }
    updatePreviewAt(viewPage->mapToScene(event->pos()));
    event->accept();
}

void HandlerInsertView::mouseReleaseEvent(QMouseEvent* event)
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

    TechDraw::DrawPage* page = getPage();
    if (!page || !page->getDocument() || !m_source) {
        quit();
        return;
    }

    const QPointF scenePos = viewPage->mapToScene(event->pos());
    const double pageX = TechDrawGui::Rez::appX(scenePos.x());
    const double pageY = TechDrawGui::Rez::appX(-scenePos.y());

    App::Document* doc = page->getDocument();
    const auto orient = Drawing::ProjectionMath::standardOrientation(m_orientation);

    doc->openTransaction("Insert Model View");
    auto* newView = doc->addObject<TechDraw::DrawViewPart>("View");
    if (newView) {
        newView->Label.setValue(orient.label);
        newView->Source.setValues({m_source});
        newView->Direction.setValue(orient.direction);
        newView->XDirection.setValue(orient.xDirection);
        newView->ScaleType.setValue("Page");
        newView->X.setValue(pageX);
        newView->Y.setValue(pageY);
        page->addView(newView, false);

        if (m_enableCentermarks && Gui::Application::Instance) {
            if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderViewPart*>(
                    Gui::Application::Instance->getViewProvider(newView)
                )) {
                vp->ArcCenterMarks.setValue(true);
            }
        }
    }
    doc->recompute();
    Drawing::TitleBlockBinder::bindTitleBlock(page, m_source, false);
    doc->commitTransaction();

    event->accept();

    const bool autoProject = m_autoStartProjected && newView != nullptr;
    QPointer<TechDrawGui::QGVPage> targetPage(viewPage);
    quit();

    // Auto-start Projected View smoothly right after placing the base view!
    if (autoProject && targetPage) {
        QTimer::singleShot(0, targetPage, [targetPage, newView]() {
            if (targetPage && newView) {
                targetPage->activateHandler(new HandlerProjectedView(newView));
            }
        });
    }
}

void HandlerInsertView::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        return;
    }
    if (event->key() == Qt::Key_R || event->key() == Qt::Key_Tab) {
        cycleOrientation();
        event->accept();
    }
    else if (event->key() == Qt::Key_Escape) {
        quit();
        event->accept();
    }
}

}  // namespace DrawingGui
