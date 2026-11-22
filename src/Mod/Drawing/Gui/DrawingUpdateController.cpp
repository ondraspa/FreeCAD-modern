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

#include <QCursor>
#include <QPushButton>
#include <QTimer>

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Gui/Application.h>
#include <Gui/Control.h>
#include <Gui/MainWindow.h>
#include <Gui/ViewportOverlays/HeadsUpViewBar.h>
#include <Gui/ViewportOverlays/ViewportOverlayController.h>
#include <Mod/Drawing/App/TitleBlockBinder.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewDimension.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/MDIViewPage.h>

#include "DimensionPalettePopup.h"
#include "DrawingUpdateController.h"

namespace DrawingGui
{

DrawingUpdateController* DrawingUpdateController::instance()
{
    static DrawingUpdateController* inst = nullptr;
    if (!inst) {
        inst = new DrawingUpdateController(Gui::getMainWindow());
    }
    return inst;
}

DrawingUpdateController::DrawingUpdateController(QObject* parent)
    : QObject(parent)
    , Gui::SelectionObserver(true)
{}

DrawingUpdateController::~DrawingUpdateController() = default;

void DrawingUpdateController::install()
{
    if (m_installed) {
        return;
    }
    m_installed = true;

    Gui::MainWindow* mw = Gui::getMainWindow();
    if (mw) {
        connect(
            mw,
            &Gui::MainWindow::windowActivated,
            this,
            &DrawingUpdateController::onWindowActivated
        );

        m_dimensionPalette = new DimensionPalettePopup(mw);

        if (auto* overlays = mw->viewportOverlayController()) {
            if (auto* headsUp = overlays->headsUpViewBar()) {
                m_updateBadge = new QPushButton(tr("Update Drawing"), headsUp);
                m_updateBadge->setObjectName(QStringLiteral("drawingUpdateBadge"));
                m_updateBadge->setCursor(Qt::PointingHandCursor);
                m_updateBadge->setToolTip(
                    tr("3D model changes are pending. Click to update drawing views now.")
                );
                m_updateBadge->setStyleSheet(QStringLiteral(
                    "QPushButton#drawingUpdateBadge {"
                    "  background-color: #d97706;"
                    "  color: #ffffff;"
                    "  border: 1px solid #b45309;"
                    "  border-radius: 4px;"
                    "  padding: 2px 8px;"
                    "  font-weight: 600;"
                    "}"
                    "QPushButton#drawingUpdateBadge:hover {"
                    "  background-color: #f59e0b;"
                    "}"
                ));
                m_updateBadge->hide();
                connect(m_updateBadge, &QPushButton::clicked, this, &DrawingUpdateController::onBadgeClicked);
                headsUp->setBadgeWidget(m_updateBadge);
            }
        }
    }

    m_connChangedObj = App::GetApplication().signalChangedObject.connect(
        [this](const App::DocumentObject& obj, const App::Property& prop) {
            onObjectChanged(obj, prop);
        }
    );
    m_connDeletedObj = App::GetApplication().signalDeletedObject.connect(
        [this](const App::DocumentObject& obj) { onDeletedObject(obj); }
    );
    m_connDeletedDoc = App::GetApplication().signalDeleteDocument.connect(
        [this](const App::Document& doc) { onDeletedDocument(doc); }
    );
}

bool DrawingUpdateController::hasPendingUpdates(const App::Document* doc) const
{
    if (!doc) {
        return !m_dirtyPages.empty();
    }
    for (const TechDraw::DrawPage* page : m_dirtyPages) {
        if (page && page->getDocument() == doc) {
            return true;
        }
    }
    return false;
}

bool DrawingUpdateController::isPageDirty(const TechDraw::DrawPage* page) const
{
    return page && m_dirtyPages.contains(page);
}

void DrawingUpdateController::markPageDirty(TechDraw::DrawPage* page)
{
    if (!page || m_updating) {
        return;
    }
    const bool inserted = m_dirtyPages.insert(page).second;
    if (inserted) {
        updateBadgeAppearance();
        Q_EMIT dirtyStateChanged();
    }
}

void DrawingUpdateController::updatePage(TechDraw::DrawPage* page)
{
    if (!page || m_updating) {
        return;
    }

    m_updating = true;
    const bool wasDeferred = (m_deferredPages.erase(page) > 0);
    m_dirtyPages.erase(page);

    if (wasDeferred || !page->KeepUpdated.getValue()) {
        page->KeepUpdated.setValue(true);
    }
    page->updateAllViews();
    Drawing::TitleBlockBinder::bindTitleBlock(page, nullptr, false);
    if (App::Document* doc = page->getDocument()) {
        doc->recompute();
    }

    m_updating = false;
    updateBadgeAppearance();
    Q_EMIT dirtyStateChanged();
}

void DrawingUpdateController::updateAllPendingPages(App::Document* doc)
{
    if (m_updating) {
        return;
    }

    std::vector<TechDraw::DrawPage*> targets;
    if (doc) {
        for (App::DocumentObject* obj : doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId())) {
            if (auto* page = dynamic_cast<TechDraw::DrawPage*>(obj)) {
                if (m_dirtyPages.contains(page) || !page->KeepUpdated.getValue()) {
                    targets.push_back(page);
                }
            }
        }
        // If user clicked Update Drawing explicitly with no dirty flag, refresh active drawing page.
        if (targets.empty()) {
            if (auto* activePage = activeDrawingPage(); activePage && activePage->getDocument() == doc) {
                targets.push_back(activePage);
            }
        }
    }
    else {
        for (const TechDraw::DrawPage* p : m_dirtyPages) {
            targets.push_back(const_cast<TechDraw::DrawPage*>(p));
        }
    }

    for (TechDraw::DrawPage* page : targets) {
        updatePage(page);
    }
}

DimensionPalettePopup* DrawingUpdateController::dimensionPalette() const
{
    return m_dimensionPalette;
}

void DrawingUpdateController::showDimensionPaletteForSelection(bool forceShow)
{
    if (!m_dimensionPalette) {
        return;
    }
    if (!forceShow && Gui::Control().activeDialog() != nullptr) {
        return;
    }

    auto selectedDims = Gui::Selection().getObjectsOfType(TechDraw::DrawViewDimension::getClassTypeId());
    if (selectedDims.size() == 1) {
        if (auto* dim = dynamic_cast<TechDraw::DrawViewDimension*>(selectedDims.front())) {
            m_dimensionPalette->showForDimension(dim, QCursor::pos());
            return;
        }
    }
    if (!forceShow) {
        m_dimensionPalette->hide();
    }
}

TechDraw::DrawPage* DrawingUpdateController::activeDrawingPage()
{
    Gui::MainWindow* mw = Gui::getMainWindow();
    if (!mw) {
        return nullptr;
    }
    auto* pageView = qobject_cast<TechDrawGui::MDIViewPage*>(mw->activeWindow());
    return pageView ? pageView->getPage() : nullptr;
}

bool DrawingUpdateController::pageDependsOnObject(
    const TechDraw::DrawPage* page,
    const App::DocumentObject* obj
)
{
    if (!page || !obj) {
        return false;
    }
    const auto inList = obj->getInListRecursive();
    for (App::DocumentObject* viewObj : page->Views.getValues()) {
        auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(viewObj);
        if (!dvp) {
            continue;
        }
        for (App::DocumentObject* src : dvp->getAllSources()) {
            if (src == obj) {
                return true;
            }
            if (std::find(inList.begin(), inList.end(), src) != inList.end()) {
                return true;
            }
        }
    }
    return false;
}

void DrawingUpdateController::onObjectChanged(
    const App::DocumentObject& obj,
    const App::Property& prop
)
{
    Q_UNUSED(prop);
    if (m_updating || obj.isRestoring()) {
        return;
    }
    // Ignore changes on TechDraw objects themselves (view moves, dimensions, templates, etc.).
    if (obj.isDerivedFrom<TechDraw::DrawPage>() || obj.isDerivedFrom<TechDraw::DrawView>()) {
        return;
    }
    App::Document* doc = obj.getDocument();
    if (!doc || doc->isPerformingTransaction()) {
        return;
    }

    TechDraw::DrawPage* currentDrawing = activeDrawingPage();
    bool anyMarked = false;

    for (App::DocumentObject* pageObj : doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId())) {
        auto* page = dynamic_cast<TechDraw::DrawPage*>(pageObj);
        if (!page || !pageDependsOnObject(page, &obj)) {
            continue;
        }

        // If user is currently editing in 3D (not viewing this drawing tab), defer HLR until return!
        if (page != currentDrawing) {
            if (page->KeepUpdated.getValue()) {
                m_updating = true;
                page->KeepUpdated.setValue(false);
                m_updating = false;
                m_deferredPages.insert(page);
            }
            if (m_dirtyPages.insert(page).second) {
                anyMarked = true;
            }
        }
    }

    if (anyMarked) {
        updateBadgeAppearance();
        Q_EMIT dirtyStateChanged();
    }
}

void DrawingUpdateController::onDeletedObject(const App::DocumentObject& obj)
{
    if (const auto* page = dynamic_cast<const TechDraw::DrawPage*>(&obj)) {
        const bool removed = (m_dirtyPages.erase(page) > 0);
        m_deferredPages.erase(page);
        if (removed) {
            updateBadgeAppearance();
            Q_EMIT dirtyStateChanged();
        }
    }
}

void DrawingUpdateController::onDeletedDocument(const App::Document& doc)
{
    bool changed = false;
    for (auto it = m_dirtyPages.begin(); it != m_dirtyPages.end();) {
        if (!(*it) || (*it)->getDocument() == &doc) {
            it = m_dirtyPages.erase(it);
            changed = true;
        }
        else {
            ++it;
        }
    }
    for (auto it = m_deferredPages.begin(); it != m_deferredPages.end();) {
        if (!(*it) || (*it)->getDocument() == &doc) {
            it = m_deferredPages.erase(it);
        }
        else {
            ++it;
        }
    }
    if (changed) {
        updateBadgeAppearance();
        Q_EMIT dirtyStateChanged();
    }
}

void DrawingUpdateController::onWindowActivated(Gui::MDIView* view)
{
    if (m_dimensionPalette) {
        m_dimensionPalette->hide();
    }

    auto* pageView = qobject_cast<TechDrawGui::MDIViewPage*>(view);
    if (pageView) {
        if (TechDraw::DrawPage* page = pageView->getPage()) {
            if (m_dirtyPages.contains(page)) {
                // Auto-update smoothly right after the tab finishes activating.
                QPointer<TechDrawGui::MDIViewPage> guardedView(pageView);
                QTimer::singleShot(0, this, [this, guardedView]() {
                    if (guardedView && guardedView->getPage()
                        && m_dirtyPages.contains(guardedView->getPage())) {
                        updatePage(guardedView->getPage());
                    }
                });
            }
        }
    }
    updateBadgeAppearance();
}

void DrawingUpdateController::onBadgeClicked()
{
    updateAllPendingPages(App::GetApplication().getActiveDocument());
}

void DrawingUpdateController::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (!m_dimensionPalette) {
        return;
    }
    if (msg.Type == Gui::SelectionChanges::ClrSelection) {
        m_dimensionPalette->hide();
        return;
    }
    if (msg.Type == Gui::SelectionChanges::RmvSelection) {
        if (Gui::Selection().getObjectsOfType(TechDraw::DrawViewDimension::getClassTypeId()).size() != 1) {
            m_dimensionPalette->hide();
        }
        return;
    }
    // Auto-surface floating Dimension Palette when user selects a single dimension on an open drawing sheet.
    if ((msg.Type == Gui::SelectionChanges::AddSelection
         || msg.Type == Gui::SelectionChanges::SetSelection)
        && activeDrawingPage() != nullptr) {
        auto dims = Gui::Selection().getObjectsOfType(TechDraw::DrawViewDimension::getClassTypeId());
        if (dims.size() == 1 && Gui::Selection().getSelection().size() == 1) {
            QTimer::singleShot(0, this, [this]() { showDimensionPaletteForSelection(false); });
        }
        else {
            m_dimensionPalette->hide();
        }
    }
}

void DrawingUpdateController::updateBadgeAppearance()
{
    if (!m_updateBadge) {
        return;
    }
    const App::Document* activeDoc = App::GetApplication().getActiveDocument();
    const bool pending = hasPendingUpdates(activeDoc);
    m_updateBadge->setVisible(pending);
    if (auto* mw = Gui::getMainWindow()) {
        if (auto* overlays = mw->viewportOverlayController()) {
            overlays->repositionOverlays();
        }
    }
}

}  // namespace DrawingGui
