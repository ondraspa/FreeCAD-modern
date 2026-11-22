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

#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/DockWindowManager.h>
#include <Gui/MainWindow.h>
#include <Gui/Selection/Selection.h>
#include <Mod/Drawing/App/DraftingTools.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewBalloon.h>
#include <Mod/TechDraw/App/DrawViewDimension.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/MDIViewPage.h>
#include <Mod/TechDraw/Gui/PagePrinter.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/ViewProviderPage.h>
#include <Mod/TechDraw/Gui/ViewProviderViewPart.h>

#include "DrawingUpdateController.h"
#include "HandlerAuxiliaryView.h"
#include "HandlerBreakView.h"
#include "HandlerCropView.h"
#include "HandlerDetailView.h"
#include "HandlerInsertView.h"
#include "HandlerProjectedView.h"
#include "HandlerSectionView.h"
#include "TaskGdtDialog.h"
#include "TaskSheetSetup.h"
#include "ViewPaletteDock.h"

namespace
{

TechDrawGui::MDIViewPage* activeMdiPage()
{
    Gui::MainWindow* mw = Gui::getMainWindow();
    if (!mw) {
        return nullptr;
    }
    return qobject_cast<TechDrawGui::MDIViewPage*>(mw->activeWindow());
}

TechDrawGui::QGVPage* activeGvPage()
{
    auto* mdi = activeMdiPage();
    if (!mdi || !mdi->getViewProviderPage()) {
        return nullptr;
    }
    return mdi->getViewProviderPage()->getQGVPage();
}

TechDraw::DrawViewPart* singleSelectedViewPart()
{
    auto objs = Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId());
    if (objs.size() == 1) {
        return dynamic_cast<TechDraw::DrawViewPart*>(objs.front());
    }
    return nullptr;
}

App::DocumentObject* pickDefaultModelSource(App::Document* doc)
{
    // 1. Check active selection
    for (const auto& sel : Gui::Selection().getSelection()) {
        if (sel.pObject && !sel.pObject->isDerivedFrom<TechDraw::DrawPage>()
            && !sel.pObject->isDerivedFrom<TechDraw::DrawView>()) {
            return sel.pObject;
        }
    }
    // 2. Check ViewPaletteDock selection if open
    if (QWidget* w = Gui::DockWindowManager::instance()->getDockWindow("DrawingViewPaletteDock")) {
        if (auto* palette = qobject_cast<DrawingGui::ViewPaletteDock*>(w)) {
            if (App::DocumentObject* src = palette->selectedSource()) {
                return src;
            }
        }
    }
    // 3. Fallback to first drawable source in document
    const auto sources = DrawingGui::SheetSetupDialog::collectDrawableSources(doc);
    return sources.empty() ? nullptr : sources.front();
}

}  // namespace

//===========================================================================
// Drawing_MakeDrawingFromPart
//===========================================================================
DEF_STD_CMD_A(CmdDrawingMakeFromPart)

CmdDrawingMakeFromPart::CmdDrawingMakeFromPart()
    : Command("Drawing_MakeDrawingFromPart")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Make Drawing from Part/Assembly…");
    sToolTipText = QT_TR_NOOP("Create a new technical drawing sheet from the active part or assembly");
    sWhatsThis = "Drawing_MakeDrawingFromPart";
    sStatusTip = sToolTipText;
    sPixmap = "DrawingWorkbench";
}

void CmdDrawingMakeFromPart::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc) {
        doc = App::GetApplication().newDocument("Drawing");
    }

    App::DocumentObject* defaultSrc = pickDefaultModelSource(doc);

    if (Gui::Application::Instance
        && Gui::Application::Instance->workbenches().contains(QStringLiteral("DrawingWorkbench"))) {
        Gui::Application::Instance->activateWorkbench("DrawingWorkbench");
    }

    DrawingGui::SheetSetupDialog dlg(doc, nullptr, defaultSrc, Gui::getMainWindow());
    if (dlg.exec() == QDialog::Accepted) {
        DrawingGui::SheetSetupDialog::createSheet(doc, dlg.result());
    }
}

bool CmdDrawingMakeFromPart::isActive()
{
    return App::GetApplication().getActiveDocument() != nullptr;
}

//===========================================================================
// Drawing_NewSheet
//===========================================================================
DEF_STD_CMD_A(CmdDrawingNewSheet)

CmdDrawingNewSheet::CmdDrawingNewSheet()
    : Command("Drawing_NewSheet")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("New Sheet…");
    sToolTipText = QT_TR_NOOP("Create a new unit-matched drawing sheet (ISO5457 / ASME Y14.1)");
    sWhatsThis = "Drawing_NewSheet";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_PageDefault";
}

void CmdDrawingNewSheet::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc) {
        doc = App::GetApplication().newDocument("Drawing");
    }
    App::DocumentObject* defaultSrc = pickDefaultModelSource(doc);
    DrawingGui::SheetSetupDialog dlg(doc, nullptr, defaultSrc, Gui::getMainWindow());
    if (dlg.exec() == QDialog::Accepted) {
        DrawingGui::SheetSetupDialog::createSheet(doc, dlg.result());
    }
}

bool CmdDrawingNewSheet::isActive()
{
    return true;
}

//===========================================================================
// Drawing_SheetProperties
//===========================================================================
DEF_STD_CMD_A(CmdDrawingSheetProperties)

CmdDrawingSheetProperties::CmdDrawingSheetProperties()
    : Command("Drawing_SheetProperties")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Sheet Properties…");
    sToolTipText = QT_TR_NOOP("Edit sheet template, projection angle, and scale");
    sWhatsThis = "Drawing_SheetProperties";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_PageTemplate";
}

void CmdDrawingSheetProperties::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page) {
        return;
    }
    DrawingGui::SheetSetupDialog dlg(page->getDocument(), page, nullptr, Gui::getMainWindow());
    if (dlg.exec() == QDialog::Accepted) {
        DrawingGui::SheetSetupDialog::applyPropertiesToSheet(page, dlg.result());
    }
}

bool CmdDrawingSheetProperties::isActive()
{
    return activeMdiPage() != nullptr;
}

//===========================================================================
// Drawing_UpdateDrawing
//===========================================================================
DEF_STD_CMD_A(CmdDrawingUpdate)

CmdDrawingUpdate::CmdDrawingUpdate()
    : Command("Drawing_UpdateDrawing")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Update Drawing");
    sToolTipText = QT_TR_NOOP("Synchronize drawing views and title blocks with latest 3D model changes");
    sWhatsThis = "Drawing_UpdateDrawing";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_RedrawPage";
}

void CmdDrawingUpdate::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    DrawingGui::DrawingUpdateController::instance()->updateAllPendingPages(
        App::GetApplication().getActiveDocument()
    );
}

bool CmdDrawingUpdate::isActive()
{
    App::Document* doc = App::GetApplication().getActiveDocument();
    return doc && !doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId()).empty();
}

//===========================================================================
// Drawing_InsertView
//===========================================================================
DEF_STD_CMD_A(CmdDrawingInsertView)

CmdDrawingInsertView::CmdDrawingInsertView()
    : Command("Drawing_InsertView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Model View");
    sToolTipText = QT_TR_NOOP("Place a 2D model view with live preview (auto-starts Projected View)");
    sWhatsThis = "Drawing_InsertView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_View";
}

void CmdDrawingInsertView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDrawGui::QGVPage* gvPage = activeGvPage();
    if (!gvPage || !gvPage->getDrawPage()) {
        return;
    }
    App::Document* doc = gvPage->getDrawPage()->getDocument();
    App::DocumentObject* source = pickDefaultModelSource(doc);
    if (!source) {
        QMessageBox::information(
            Gui::getMainWindow(),
            QObject::tr("Model View"),
            QObject::tr("No 3D part, body, or assembly was found in the active document.")
        );
        return;
    }
    gvPage->activateHandler(
        new DrawingGui::HandlerInsertView(source, Drawing::StandardOrientation::Front, true, true)
    );
}

bool CmdDrawingInsertView::isActive()
{
    return activeGvPage() != nullptr;
}

//===========================================================================
// Drawing_ProjectedView
//===========================================================================
DEF_STD_CMD_A(CmdDrawingProjectedView)

CmdDrawingProjectedView::CmdDrawingProjectedView()
    : Command("Drawing_ProjectedView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Projected View");
    sToolTipText = QT_TR_NOOP("Project aligned orthogonal or isometric views from any existing view");
    sWhatsThis = "Drawing_ProjectedView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ProjectionGroup";
}

void CmdDrawingProjectedView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDrawGui::QGVPage* gvPage = activeGvPage();
    if (!gvPage) {
        return;
    }
    gvPage->activateHandler(new DrawingGui::HandlerProjectedView(singleSelectedViewPart()));
}

bool CmdDrawingProjectedView::isActive()
{
    return activeGvPage() != nullptr;
}

//===========================================================================
// Drawing_SectionView
//===========================================================================
DEF_STD_CMD_A(CmdDrawingSectionView)

CmdDrawingSectionView::CmdDrawingSectionView()
    : Command("Drawing_SectionView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Section View");
    sToolTipText = QT_TR_NOOP("Draw an interactive cutting line directly on a view to create an aligned section view");
    sWhatsThis = "Drawing_SectionView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_SectionView";
}

void CmdDrawingSectionView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDrawGui::QGVPage* gvPage = activeGvPage();
    if (!gvPage) {
        return;
    }
    gvPage->activateHandler(new DrawingGui::HandlerSectionView(singleSelectedViewPart()));
}

bool CmdDrawingSectionView::isActive()
{
    return activeGvPage() != nullptr;
}

//===========================================================================
// Drawing_DetailView
//===========================================================================
DEF_STD_CMD_A(CmdDrawingDetailView)

CmdDrawingDetailView::CmdDrawingDetailView()
    : Command("Drawing_DetailView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Detail View");
    sToolTipText = QT_TR_NOOP("Click center, drag radius, and click to place a magnified detail view");
    sWhatsThis = "Drawing_DetailView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_DetailView";
}

void CmdDrawingDetailView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    TechDrawGui::QGVPage* gvPage = activeGvPage();
    if (!gvPage) {
        return;
    }
    gvPage->activateHandler(new DrawingGui::HandlerDetailView(singleSelectedViewPart()));
}

bool CmdDrawingDetailView::isActive()
{
    return activeGvPage() != nullptr;
}

//===========================================================================
// Drawing_SmartDimension
//===========================================================================
DEF_STD_CMD_A(CmdDrawingSmartDimension)

CmdDrawingSmartDimension::CmdDrawingSmartDimension()
    : Command("Drawing_SmartDimension")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Smart Dimension");
    sToolTipText = QT_TR_NOOP("Place smart linear, radial, diameter, or angular dimensions with parallel spacing snap");
    sWhatsThis = "Drawing_SmartDimension";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_Dimension";
    sAccel = "D";
}

void CmdDrawingSmartDimension::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    if (Gui::Application::Instance) {
        Gui::Application::Instance->commandManager().runCommandByName("TechDraw_Dimension");
    }
}

bool CmdDrawingSmartDimension::isActive()
{
    return activeGvPage() != nullptr;
}

//===========================================================================
// Drawing_AutoCentermarks
//===========================================================================
DEF_STD_CMD_A(CmdDrawingAutoCentermarks)

CmdDrawingAutoCentermarks::CmdDrawingAutoCentermarks()
    : Command("Drawing_AutoCentermarks")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Toggle Hole Centermarks");
    sToolTipText = QT_TR_NOOP("Toggle circular arc/hole centermarks on selected views (or all views on active sheet)");
    sWhatsThis = "Drawing_AutoCentermarks";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExtensionCircleCenterLines";
}

void CmdDrawingAutoCentermarks::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    if (!mdi || !mdi->getPage() || !Gui::Application::Instance) {
        return;
    }

    std::vector<TechDraw::DrawViewPart*> targets;
    for (App::DocumentObject* obj :
         Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId())) {
        if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(obj)) {
            targets.push_back(dvp);
        }
    }
    if (targets.empty()) {
        for (App::DocumentObject* obj : mdi->getPage()->Views.getValues()) {
            if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(obj)) {
                targets.push_back(dvp);
            }
        }
    }
    if (targets.empty()) {
        return;
    }

    App::Document* doc = mdi->getPage()->getDocument();
    doc->openTransaction("Toggle Hole Centermarks");
    for (TechDraw::DrawViewPart* dvp : targets) {
        if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderViewPart*>(
                Gui::Application::Instance->getViewProvider(dvp)
            )) {
            vp->ArcCenterMarks.setValue(!vp->ArcCenterMarks.getValue());
        }
    }
    doc->commitTransaction();
}

bool CmdDrawingAutoCentermarks::isActive()
{
    return activeMdiPage() != nullptr;
}

//===========================================================================
// Drawing_DimensionPalette
//===========================================================================
DEF_STD_CMD_A(CmdDrawingDimensionPalette)

CmdDrawingDimensionPalette::CmdDrawingDimensionPalette()
    : Command("Drawing_DimensionPalette")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Dimension Palette…");
    sToolTipText = QT_TR_NOOP("Open floating Dimension Palette for the selected dimension");
    sWhatsThis = "Drawing_DimensionPalette";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExtensionCustomizeFormat";
}

void CmdDrawingDimensionPalette::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    DrawingGui::DrawingUpdateController::instance()->showDimensionPaletteForSelection(true);
}

bool CmdDrawingDimensionPalette::isActive()
{
    return Gui::Selection().getObjectsOfType(TechDraw::DrawViewDimension::getClassTypeId()).size()
        == 1;
}

//===========================================================================
// Drawing_BreakAlignment
//===========================================================================
DEF_STD_CMD_A(CmdDrawingBreakAlignment)

CmdDrawingBreakAlignment::CmdDrawingBreakAlignment()
    : Command("Drawing_BreakAlignment")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Break Alignment");
    sToolTipText = QT_TR_NOOP("Release parent-child projection alignment on selected view(s)");
    sWhatsThis = "Drawing_BreakAlignment";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExtensionLockUnlockView";
}

void CmdDrawingBreakAlignment::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto views = Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId());
    if (views.empty()) {
        return;
    }
    App::Document* doc = views.front()->getDocument();
    if (!doc) {
        return;
    }
    doc->openTransaction("Break View Alignment");
    for (App::DocumentObject* obj : views) {
        if (auto* dv = dynamic_cast<TechDraw::DrawView*>(obj)) {
            Drawing::ProjectionMath::breakViewAlignment(dv);
        }
    }
    doc->commitTransaction();
}

bool CmdDrawingBreakAlignment::isActive()
{
    for (App::DocumentObject* obj :
         Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId())) {
        const auto mode
            = Drawing::ProjectionMath::getViewAlignmentMode(dynamic_cast<TechDraw::DrawView*>(obj));
        if (mode == Drawing::AlignmentKind::Horizontal || mode == Drawing::AlignmentKind::Vertical
            || mode == Drawing::AlignmentKind::Vector) {
            return true;
        }
    }
    return false;
}

//===========================================================================
// Drawing_RestoreAlignment
//===========================================================================
DEF_STD_CMD_A(CmdDrawingRestoreAlignment)

CmdDrawingRestoreAlignment::CmdDrawingRestoreAlignment()
    : Command("Drawing_RestoreAlignment")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Restore Alignment");
    sToolTipText = QT_TR_NOOP("Re-snap selected view(s) to their parent view's projection axis");
    sWhatsThis = "Drawing_RestoreAlignment";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExtensionLockUnlockView";
}

void CmdDrawingRestoreAlignment::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto views = Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId());
    if (views.empty()) {
        return;
    }
    App::Document* doc = views.front()->getDocument();
    if (!doc) {
        return;
    }
    doc->openTransaction("Restore View Alignment");
    for (App::DocumentObject* obj : views) {
        if (auto* dv = dynamic_cast<TechDraw::DrawView*>(obj)) {
            if (Drawing::ProjectionMath::restoreViewAlignment(dv)) {
                dv->recomputeFeature();
            }
        }
    }
    doc->commitTransaction();
}

bool CmdDrawingRestoreAlignment::isActive()
{
    for (App::DocumentObject* obj :
         Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId())) {
        auto* dv = dynamic_cast<TechDraw::DrawView*>(obj);
        if (Drawing::ProjectionMath::getViewAlignmentMode(dv) == Drawing::AlignmentKind::Broken
            && Drawing::ProjectionMath::getParentAlignedView(dv) != nullptr) {
            return true;
        }
    }
    return false;
}

//===========================================================================
// Export Commands (PDF / DXF / SVG)
//===========================================================================
DEF_STD_CMD_A(CmdDrawingExportPDF)

CmdDrawingExportPDF::CmdDrawingExportPDF()
    : Command("Drawing_ExportPDF")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Export PDF…");
    sToolTipText = QT_TR_NOOP("Export active drawing sheet as PDF");
    sWhatsThis = "Drawing_ExportPDF";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExportPagePDF";
}

void CmdDrawingExportPDF::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    if (auto* mdi = activeMdiPage()) {
        mdi->exportAsPdf();
    }
}

bool CmdDrawingExportPDF::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingExportDXF)

CmdDrawingExportDXF::CmdDrawingExportDXF()
    : Command("Drawing_ExportDXF")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Export DXF…");
    sToolTipText = QT_TR_NOOP("Export active drawing sheet as DXF");
    sWhatsThis = "Drawing_ExportDXF";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExportPageDXF";
}

void CmdDrawingExportDXF::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    if (auto* mdi = activeMdiPage()) {
        mdi->saveDXF();
    }
}

bool CmdDrawingExportDXF::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingExportSVG)

CmdDrawingExportSVG::CmdDrawingExportSVG()
    : Command("Drawing_ExportSVG")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Export SVG…");
    sToolTipText = QT_TR_NOOP("Export active drawing sheet as SVG");
    sWhatsThis = "Drawing_ExportSVG";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExportPageSVG";
}

void CmdDrawingExportSVG::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    if (auto* mdi = activeMdiPage()) {
        mdi->saveSVG();
    }
}

bool CmdDrawingExportSVG::isActive()
{
    return activeMdiPage() != nullptr;
}

//===========================================================================
// Milestone 3 Commands — Advanced Drafting, GD&T & Assembly Documentation
//===========================================================================

DEF_STD_CMD_A(CmdDrawingAuxiliaryView)

CmdDrawingAuxiliaryView::CmdDrawingAuxiliaryView()
    : Command("Drawing_AuxiliaryView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Auxiliary View");
    sToolTipText = QT_TR_NOOP("Click a slanted 2D edge on any view and unfold an aligned perpendicular Auxiliary View");
    sWhatsThis = "Drawing_AuxiliaryView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ProjGroup";
}

void CmdDrawingAuxiliaryView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* gvPage = activeGvPage();
    if (!gvPage) {
        return;
    }
    gvPage->activateHandler(new DrawingGui::HandlerAuxiliaryView(singleSelectedViewPart()));
}

bool CmdDrawingAuxiliaryView::isActive()
{
    return activeGvPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingBreakView)

CmdDrawingBreakView::CmdDrawingBreakView()
    : Command("Drawing_BreakView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Break View (2D)");
    sToolTipText = QT_TR_NOOP("Click two cut positions directly on a 2D drawing view to break and foreshorten it (V=Vertical, H=Horizontal)");
    sWhatsThis = "Drawing_BreakView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_BrokenView";
}

void CmdDrawingBreakView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* gvPage = activeGvPage();
    if (!gvPage) {
        return;
    }
    gvPage->activateHandler(new DrawingGui::HandlerBreakView(singleSelectedViewPart()));
}

bool CmdDrawingBreakView::isActive()
{
    return activeGvPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingCropView)

CmdDrawingCropView::CmdDrawingCropView()
    : Command("Drawing_CropView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Crop View");
    sToolTipText = QT_TR_NOOP("Click two corners on a drawing view to crop it in place");
    sWhatsThis = "Drawing_CropView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ClipGroup";
}

void CmdDrawingCropView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* gvPage = activeGvPage();
    if (!gvPage) {
        return;
    }
    gvPage->activateHandler(new DrawingGui::HandlerCropView(singleSelectedViewPart()));
}

bool CmdDrawingCropView::isActive()
{
    return activeGvPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingDatumSymbol)

CmdDrawingDatumSymbol::CmdDrawingDatumSymbol()
    : Command("Drawing_DatumSymbol")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Datum Feature Symbol…");
    sToolTipText = QT_TR_NOOP("Insert an ISO 5459 / ASME Y14.5 Datum Feature triangle and boxed letter");
    sWhatsThis = "Drawing_DatumSymbol";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_Balloon";
}

void CmdDrawingDatumSymbol::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page || !page->getDocument()) {
        return;
    }

    DrawingGui::DatumSymbolDialog dlg(Gui::getMainWindow());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    TechDraw::DrawViewPart* ownerView = singleSelectedViewPart();
    const double placeX = ownerView ? (ownerView->X.getValue() + 25.0) : (page->getPageWidth() * 0.5);
    const double placeY = ownerView ? (ownerView->Y.getValue() + 25.0) : (page->getPageHeight() * 0.5);

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert Datum Symbol");
    Drawing::DraftingTools::createDatumSymbolOnPage(page, dlg.resultSpec(), placeX, placeY, ownerView);
    doc->recompute();
    doc->commitTransaction();
}

bool CmdDrawingDatumSymbol::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingGdtFrame)

CmdDrawingGdtFrame::CmdDrawingGdtFrame()
    : Command("Drawing_GdtFrame")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Geometric Tolerance (GD&T)…");
    sToolTipText = QT_TR_NOOP("Build and place an ISO 1101 / ASME Y14.5 Feature Control Frame");
    sWhatsThis = "Drawing_GdtFrame";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_RichTextAnnotation";
}

void CmdDrawingGdtFrame::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page || !page->getDocument()) {
        return;
    }

    DrawingGui::GdtFrameDialog dlg(Gui::getMainWindow());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    TechDraw::DrawViewPart* ownerView = singleSelectedViewPart();
    const double placeX = ownerView ? (ownerView->X.getValue() + 30.0) : (page->getPageWidth() * 0.5);
    const double placeY = ownerView ? (ownerView->Y.getValue() + 20.0) : (page->getPageHeight() * 0.5);

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert GD&T Frame");
    Drawing::DraftingTools::createGdtFrameOnPage(page, dlg.resultSpec(), placeX, placeY, ownerView);
    doc->recompute();
    doc->commitTransaction();
}

bool CmdDrawingGdtFrame::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingHoleCallout)

CmdDrawingHoleCallout::CmdDrawingHoleCallout()
    : Command("Drawing_HoleCallout")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Hole / Thread Callout");
    sToolTipText = QT_TR_NOOP("Insert a parametric Hole & Thread Callout inspecting PartDesign::Hole or circular edges");
    sWhatsThis = "Drawing_HoleCallout";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ExtensionHoleCircle";
}

void CmdDrawingHoleCallout::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page || !page->getDocument()) {
        return;
    }

    TechDraw::DrawViewPart* targetView = singleSelectedViewPart();
    std::string subName;
    for (const auto& sel : Gui::Selection().getSelection()) {
        if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(sel.pObject)) {
            targetView = dvp;
            if (sel.SubName && sel.SubName[0] != '\0') {
                subName = sel.SubName;
            }
            break;
        }
    }
    if (!targetView) {
        for (App::DocumentObject* v : page->Views.getValues()) {
            if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(v)) {
                targetView = dvp;
                break;
            }
        }
    }
    if (!targetView) {
        return;
    }

    App::Document* doc = page->getDocument();
    doc->openTransaction("Insert Hole Callout");
    Drawing::DraftingTools::createHoleCalloutOnPage(
        page,
        targetView,
        subName,
        0.0,
        0.0,
        24.0,
        18.0
    );
    doc->recompute();
    doc->commitTransaction();
}

bool CmdDrawingHoleCallout::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingBomTable)

CmdDrawingBomTable::CmdDrawingBomTable()
    : Command("Drawing_BomTable")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Bill of Materials (BOM)");
    sToolTipText = QT_TR_NOOP("Insert or update a Bill of Materials table on the active drawing sheet");
    sWhatsThis = "Drawing_BomTable";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_SpreadsheetView";
}

void CmdDrawingBomTable::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page || !page->getDocument()) {
        return;
    }

    App::Document* doc = page->getDocument();
    App::DocumentObject* source = pickDefaultModelSource(doc);

    doc->openTransaction("Insert BOM Table");
    Drawing::DraftingTools::createOrUpdateBomTableOnPage(page, source);
    doc->recompute();
    doc->commitTransaction();
}

bool CmdDrawingBomTable::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingAutoBalloon)

CmdDrawingAutoBalloon::CmdDrawingAutoBalloon()
    : Command("Drawing_AutoBalloon")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Auto-Balloon");
    sToolTipText = QT_TR_NOOP("Automatically generate BOM-linked item balloons aligned on a Magnetic Line");
    sWhatsThis = "Drawing_AutoBalloon";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_Balloon";
}

void CmdDrawingAutoBalloon::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page || !page->getDocument()) {
        return;
    }

    TechDraw::DrawViewPart* targetView = singleSelectedViewPart();
    if (!targetView) {
        for (App::DocumentObject* v : page->Views.getValues()) {
            if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(v)) {
                targetView = dvp;
                break;
            }
        }
    }
    if (!targetView) {
        return;
    }

    App::Document* doc = page->getDocument();
    doc->openTransaction("Auto-Balloon View");
    Drawing::DraftingTools::createAutoBalloonsForView(page, targetView);
    doc->recompute();
    doc->commitTransaction();
}

bool CmdDrawingAutoBalloon::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingMagneticLine)

CmdDrawingMagneticLine::CmdDrawingMagneticLine()
    : Command("Drawing_MagneticLine")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Align Balloons (Magnetic Line)");
    sToolTipText = QT_TR_NOOP("Snap selected or sheet balloons evenly along a Magnetic Alignment Line");
    sWhatsThis = "Drawing_MagneticLine";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_Balloon";
}

void CmdDrawingMagneticLine::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    TechDraw::DrawPage* page = mdi ? mdi->getPage() : nullptr;
    if (!page || !page->getDocument()) {
        return;
    }

    std::vector<TechDraw::DrawViewBalloon*> balloons;
    for (App::DocumentObject* obj :
         Gui::Selection().getObjectsOfType(TechDraw::DrawViewBalloon::getClassTypeId())) {
        if (auto* b = dynamic_cast<TechDraw::DrawViewBalloon*>(obj)) {
            balloons.push_back(b);
        }
    }
    if (balloons.empty()) {
        for (App::DocumentObject* obj : page->Views.getValues()) {
            if (auto* b = dynamic_cast<TechDraw::DrawViewBalloon*>(obj)) {
                balloons.push_back(b);
            }
        }
    }
    if (balloons.size() < 2) {
        return;
    }

    const Base::Vector3d startPt(balloons.front()->X.getValue(), balloons.front()->Y.getValue(), 0.0);
    const Base::Vector3d endPt(balloons.back()->X.getValue(), balloons.front()->Y.getValue(), 0.0);

    App::Document* doc = page->getDocument();
    doc->openTransaction("Align Balloons on Magnetic Line");
    Drawing::DraftingTools::alignBalloonsOnMagneticLine(balloons, startPt, endPt);
    doc->recompute();
    doc->commitTransaction();
}

bool CmdDrawingMagneticLine::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingToggleShadedView)

CmdDrawingToggleShadedView::CmdDrawingToggleShadedView()
    : Command("Drawing_ToggleShadedView")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Shaded with Edges");
    sToolTipText = QT_TR_NOOP("Toggle shaded face fill on selected or all drawing views");
    sWhatsThis = "Drawing_ToggleShadedView";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_ToggleFrame";
}

void CmdDrawingToggleShadedView::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    auto* mdi = activeMdiPage();
    if (!mdi || !mdi->getPage()) {
        return;
    }

    std::vector<TechDraw::DrawViewPart*> targets;
    for (App::DocumentObject* obj :
         Gui::Selection().getObjectsOfType(TechDraw::DrawViewPart::getClassTypeId())) {
        if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(obj)) {
            targets.push_back(dvp);
        }
    }
    if (targets.empty()) {
        for (App::DocumentObject* obj : mdi->getPage()->Views.getValues()) {
            if (auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(obj)) {
                targets.push_back(dvp);
            }
        }
    }
    if (targets.empty()) {
        return;
    }

    App::Document* doc = mdi->getPage()->getDocument();
    doc->openTransaction("Toggle Shaded View Mode");
    for (TechDraw::DrawViewPart* dvp : targets) {
        if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderViewPart*>(
                Gui::Application::Instance->getViewProvider(dvp)
            )) {
            const bool currentlyTransparent = vp->FaceTransparency.getValue() >= 80;
            if (currentlyTransparent) {
                vp->FaceColor.setValue(Base::Color(0.84f, 0.88f, 0.93f));
                vp->FaceTransparency.setValue(15);
            }
            else {
                vp->FaceTransparency.setValue(100);
            }
        }
    }
    doc->commitTransaction();
}

bool CmdDrawingToggleShadedView::isActive()
{
    return activeMdiPage() != nullptr;
}

DEF_STD_CMD_A(CmdDrawingExportAllPDF)

CmdDrawingExportAllPDF::CmdDrawingExportAllPDF()
    : Command("Drawing_ExportAllPDF")
{
    sAppModule = "Drawing";
    sGroup = QT_TR_NOOP("Drawing");
    sMenuText = QT_TR_NOOP("Export All Sheets to PDF…");
    sToolTipText = QT_TR_NOOP("Batch export all drawing sheets in the document into a single multi-page PDF");
    sWhatsThis = "Drawing_ExportAllPDF";
    sStatusTip = sToolTipText;
    sPixmap = "actions/TechDraw_PrintAll";
}

void CmdDrawingExportAllPDF::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc || doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId()).empty()) {
        return;
    }

    const QString defaultFile = QString::fromUtf8(doc->Label.getValue()) + QStringLiteral(".pdf");
    const QString outPath = QFileDialog::getSaveFileName(
        Gui::getMainWindow(),
        QObject::tr("Export All Drawing Sheets to Multi-Page PDF"),
        defaultFile,
        QObject::tr("PDF Files (*.pdf)")
    );
    if (outPath.isEmpty()) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outPath);
    TechDrawGui::PagePrinter::printAllPdf(&printer, doc);
}

bool CmdDrawingExportAllPDF::isActive()
{
    App::Document* doc = App::GetApplication().getActiveDocument();
    return doc && !doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId()).empty();
}

void CreateDrawingCommands()
{
    Gui::CommandManager& rcCmdMgr = Gui::Application::Instance->commandManager();
    rcCmdMgr.addCommand(new CmdDrawingMakeFromPart());
    rcCmdMgr.addCommand(new CmdDrawingNewSheet());
    rcCmdMgr.addCommand(new CmdDrawingSheetProperties());
    rcCmdMgr.addCommand(new CmdDrawingUpdate());
    rcCmdMgr.addCommand(new CmdDrawingInsertView());
    rcCmdMgr.addCommand(new CmdDrawingProjectedView());
    rcCmdMgr.addCommand(new CmdDrawingAuxiliaryView());
    rcCmdMgr.addCommand(new CmdDrawingSectionView());
    rcCmdMgr.addCommand(new CmdDrawingDetailView());
    rcCmdMgr.addCommand(new CmdDrawingBreakView());
    rcCmdMgr.addCommand(new CmdDrawingCropView());
    rcCmdMgr.addCommand(new CmdDrawingSmartDimension());
    rcCmdMgr.addCommand(new CmdDrawingAutoCentermarks());
    rcCmdMgr.addCommand(new CmdDrawingDimensionPalette());
    rcCmdMgr.addCommand(new CmdDrawingDatumSymbol());
    rcCmdMgr.addCommand(new CmdDrawingGdtFrame());
    rcCmdMgr.addCommand(new CmdDrawingHoleCallout());
    rcCmdMgr.addCommand(new CmdDrawingBomTable());
    rcCmdMgr.addCommand(new CmdDrawingAutoBalloon());
    rcCmdMgr.addCommand(new CmdDrawingMagneticLine());
    rcCmdMgr.addCommand(new CmdDrawingToggleShadedView());
    rcCmdMgr.addCommand(new CmdDrawingBreakAlignment());
    rcCmdMgr.addCommand(new CmdDrawingRestoreAlignment());
    rcCmdMgr.addCommand(new CmdDrawingExportPDF());
    rcCmdMgr.addCommand(new CmdDrawingExportAllPDF());
    rcCmdMgr.addCommand(new CmdDrawingExportDXF());
    rcCmdMgr.addCommand(new CmdDrawingExportSVG());
}
