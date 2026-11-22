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

#include <unordered_map>

#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/Selection/Selection.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/Drawing/App/TitleBlockBinder.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawView.h>
#include <Mod/TechDraw/App/DrawViewDetail.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/DrawViewSection.h>
#include <Mod/TechDraw/Gui/ViewProviderPage.h>

#include "DrawingUpdateController.h"
#include "SheetsViewsPanel.h"
#include "TaskSheetSetup.h"

namespace DrawingGui
{

namespace
{

constexpr int kObjNameRole = Qt::UserRole + 1;

QString badgeForView(const TechDraw::DrawView* view)
{
    if (!view) {
        return {};
    }
    if (view->isDerivedFrom<TechDraw::DrawViewSection>()) {
        return QStringLiteral("[Section]");
    }
    if (view->isDerivedFrom<TechDraw::DrawViewDetail>()) {
        return QStringLiteral("[Detail]");
    }
    const auto alignMode = Drawing::ProjectionMath::getViewAlignmentMode(view);
    if (alignMode == Drawing::AlignmentKind::Horizontal
        || alignMode == Drawing::AlignmentKind::Vertical
        || alignMode == Drawing::AlignmentKind::Vector) {
        return QStringLiteral("[Aligned]");
    }
    if (alignMode == Drawing::AlignmentKind::Broken) {
        return QStringLiteral("[Free]");
    }
    return {};
}

}  // namespace

SheetsViewsPanel::SheetsViewsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("DrawingSheetsViewsPanel"));
    setWindowTitle(tr("Sheets & Views"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(4);

    m_addSheetBtn = new QPushButton(tr("+ Sheet"), this);
    m_addSheetBtn->setToolTip(tr("Add a new drawing sheet"));
    connect(m_addSheetBtn, &QPushButton::clicked, this, &SheetsViewsPanel::onAddSheetClicked);

    m_propsBtn = new QPushButton(tr("Properties"), this);
    m_propsBtn->setToolTip(tr("Edit selected sheet properties"));
    connect(m_propsBtn, &QPushButton::clicked, this, &SheetsViewsPanel::onPropertiesClicked);

    m_updateBtn = new QPushButton(tr("Update"), this);
    m_updateBtn->setToolTip(tr("Update pending drawing views from 3D model"));
    connect(m_updateBtn, &QPushButton::clicked, this, &SheetsViewsPanel::onUpdateClicked);

    btnRow->addWidget(m_addSheetBtn);
    btnRow->addWidget(m_propsBtn);
    btnRow->addWidget(m_updateBtn);
    root->addLayout(btnRow);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Sheet / View"), tr("Info")});
    m_tree->setColumnWidth(0, 165);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setAlternatingRowColors(true);
    connect(m_tree, &QTreeWidget::itemClicked, this, &SheetsViewsPanel::onItemClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &SheetsViewsPanel::onItemDoubleClicked);
    connect(
        m_tree,
        &QTreeWidget::customContextMenuRequested,
        this,
        &SheetsViewsPanel::onContextMenuRequested
    );
    root->addWidget(m_tree, 1);

    m_connNewObj = App::GetApplication().signalNewObject.connect(
        [this](const App::DocumentObject& obj) {
            if (obj.isDerivedFrom<TechDraw::DrawPage>() || obj.isDerivedFrom<TechDraw::DrawView>()) {
                scheduleRebuild();
            }
        }
    );
    m_connDelObj = App::GetApplication().signalDeletedObject.connect(
        [this](const App::DocumentObject& obj) {
            if (obj.isDerivedFrom<TechDraw::DrawPage>() || obj.isDerivedFrom<TechDraw::DrawView>()) {
                scheduleRebuild();
            }
        }
    );
    m_connChangedObj = App::GetApplication().signalChangedObject.connect(
        [this](const App::DocumentObject& obj, const App::Property& prop) {
            if (obj.isDerivedFrom<TechDraw::DrawPage>()) {
                const char* pName = prop.getName();
                if (pName && (std::string(pName) == "Views" || std::string(pName) == "Scale")) {
                    scheduleRebuild();
                }
            }
            else if (obj.isDerivedFrom<TechDraw::DrawView>()) {
                const char* pName = prop.getName();
                if (pName
                    && (std::string(pName) == "AlignmentMode"
                        || std::string(pName) == "ParentView")) {
                    scheduleRebuild();
                }
            }
        }
    );
    m_connRelabelObj = App::GetApplication().signalRelabelObject.connect(
        [this](const App::DocumentObject& obj) {
            if (obj.isDerivedFrom<TechDraw::DrawPage>() || obj.isDerivedFrom<TechDraw::DrawView>()) {
                scheduleRebuild();
            }
        }
    );
    m_connActiveDoc = App::GetApplication().signalActiveDocument.connect(
        [this](const App::Document&) { scheduleRebuild(); }
    );

    if (auto* ctrl = DrawingUpdateController::instance()) {
        connect(
            ctrl,
            &DrawingUpdateController::dirtyStateChanged,
            this,
            &SheetsViewsPanel::updateDirtyIndicator
        );
    }

    rebuildTree();
}

SheetsViewsPanel::~SheetsViewsPanel() = default;

void SheetsViewsPanel::scheduleRebuild()
{
    if (m_rebuildScheduled) {
        return;
    }
    m_rebuildScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_rebuildScheduled = false;
        rebuildTree();
    });
}

App::DocumentObject* SheetsViewsPanel::objectForItem(QTreeWidgetItem* item) const
{
    if (!item) {
        return nullptr;
    }
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc) {
        return nullptr;
    }
    const std::string name = item->data(0, kObjNameRole).toString().toStdString();
    return name.empty() ? nullptr : doc->getObject(name.c_str());
}

void SheetsViewsPanel::rebuildTree()
{
    m_tree->clear();
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc) {
        updateDirtyIndicator();
        return;
    }

    auto* ctrl = DrawingUpdateController::instance();
    const auto pages = doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId());

    for (App::DocumentObject* pageObj : pages) {
        auto* page = dynamic_cast<TechDraw::DrawPage*>(pageObj);
        if (!page) {
            continue;
        }

        const QString scaleStr = QString::fromStdString(
            Drawing::TitleBlockBinder::formatScaleRatio(page->Scale.getValue())
        );
        const bool dirty = ctrl && ctrl->isPageDirty(page);
        const QString pageInfo = dirty ? tr("%1 • Out of date").arg(scaleStr) : scaleStr;

        auto* sheetItem = new QTreeWidgetItem(m_tree);
        sheetItem->setText(0, QString::fromUtf8(page->Label.getValue()));
        sheetItem->setText(1, pageInfo);
        sheetItem->setData(0, kObjNameRole, QString::fromUtf8(page->getNameInDocument()));
        if (dirty) {
            sheetItem->setForeground(1, QColor(217, 119, 6));
        }

        // First pass: create items for all views on this page (excluding dimensions/hatches so tree stays clean).
        std::unordered_map<const App::DocumentObject*, QTreeWidgetItem*> viewItemMap;
        std::vector<TechDraw::DrawView*> pageViews;

        for (App::DocumentObject* viewObj : page->Views.getValues()) {
            auto* dv = dynamic_cast<TechDraw::DrawView*>(viewObj);
            if (!dv) {
                continue;
            }
            if (!dv->isDerivedFrom<TechDraw::DrawViewPart>()) {
                continue;
            }
            pageViews.push_back(dv);
        }

        // Attach root views directly under sheet, and aligned/section/detail children under their parent view.
        for (TechDraw::DrawView* dv : pageViews) {
            auto* item = new QTreeWidgetItem();
            item->setText(0, QString::fromUtf8(dv->Label.getValue()));
            item->setText(1, badgeForView(dv));
            item->setData(0, kObjNameRole, QString::fromUtf8(dv->getNameInDocument()));
            viewItemMap[dv] = item;
        }

        for (TechDraw::DrawView* dv : pageViews) {
            QTreeWidgetItem* item = viewItemMap[dv];
            App::DocumentObject* parentObj = Drawing::ProjectionMath::getParentAlignedView(dv);
            if (!parentObj) {
                if (auto* sec = dynamic_cast<TechDraw::DrawViewSection*>(dv)) {
                    parentObj = sec->BaseView.getValue();
                }
                else if (auto* det = dynamic_cast<TechDraw::DrawViewDetail*>(dv)) {
                    parentObj = det->BaseView.getValue();
                }
            }
            if (parentObj && viewItemMap.contains(parentObj) && parentObj != dv) {
                viewItemMap[parentObj]->addChild(item);
            }
            else {
                sheetItem->addChild(item);
            }
        }

        sheetItem->setExpanded(true);
    }

    m_tree->expandAll();
    updateDirtyIndicator();
}

void SheetsViewsPanel::updateDirtyIndicator()
{
    auto* ctrl = DrawingUpdateController::instance();
    const bool dirty = ctrl && ctrl->hasPendingUpdates(App::GetApplication().getActiveDocument());
    if (dirty) {
        m_updateBtn->setText(tr("Update ●"));
        m_updateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #d97706; color: #ffffff; font-weight: 600; }"
        ));
    }
    else {
        m_updateBtn->setText(tr("Update"));
        m_updateBtn->setStyleSheet(QString());
    }
}

void SheetsViewsPanel::onAddSheetClicked()
{
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc) {
        doc = App::GetApplication().newDocument("Drawing");
    }
    SheetSetupDialog dlg(doc, nullptr, nullptr, this);
    if (dlg.exec() == QDialog::Accepted) {
        SheetSetupDialog::createSheet(doc, dlg.result());
        rebuildTree();
    }
}

void SheetsViewsPanel::onPropertiesClicked()
{
    App::DocumentObject* obj = objectForItem(m_tree->currentItem());
    auto* page = dynamic_cast<TechDraw::DrawPage*>(obj);
    if (!page && obj) {
        if (auto* dv = dynamic_cast<TechDraw::DrawView*>(obj)) {
            page = dv->findParentPage();
        }
    }
    if (!page) {
        return;
    }

    SheetSetupDialog dlg(page->getDocument(), page, nullptr, this);
    if (dlg.exec() == QDialog::Accepted) {
        SheetSetupDialog::applyPropertiesToSheet(page, dlg.result());
        rebuildTree();
    }
}

void SheetsViewsPanel::onUpdateClicked()
{
    DrawingUpdateController::instance()->updateAllPendingPages(
        App::GetApplication().getActiveDocument()
    );
    rebuildTree();
}

void SheetsViewsPanel::onItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    App::DocumentObject* obj = objectForItem(item);
    if (!obj || !obj->getDocument()) {
        return;
    }

    Gui::Selection().clearSelection();
    Gui::Selection().addSelection(obj->getDocument()->getName(), obj->getNameInDocument());

    if (auto* page = dynamic_cast<TechDraw::DrawPage*>(obj)) {
        if (Gui::Document* gdoc
            = Gui::Application::Instance ? Gui::Application::Instance->getDocument(page->getDocument())
                                         : nullptr) {
            if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderPage*>(gdoc->getViewProvider(page))) {
                vp->show();
            }
        }
    }
}

void SheetsViewsPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    onItemClicked(item, column);
    if (dynamic_cast<TechDraw::DrawPage*>(objectForItem(item))) {
        onPropertiesClicked();
    }
}

void SheetsViewsPanel::onContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    App::DocumentObject* obj = objectForItem(item);
    if (!obj || !obj->getDocument()) {
        return;
    }

    QMenu menu(this);
    if (auto* page = dynamic_cast<TechDraw::DrawPage*>(obj)) {
        QAction* actProps = menu.addAction(tr("Sheet Properties…"));
        QAction* actRename = menu.addAction(tr("Rename Sheet…"));
        QAction* actUpdate = menu.addAction(tr("Update Sheet"));
        menu.addSeparator();
        QAction* actDelete = menu.addAction(tr("Delete Sheet"));

        QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen == actProps) {
            onPropertiesClicked();
        }
        else if (chosen == actRename) {
            bool ok = false;
            const QString newName = QInputDialog::getText(
                this,
                tr("Rename Sheet"),
                tr("Sheet name:"),
                QLineEdit::Normal,
                QString::fromUtf8(page->Label.getValue()),
                &ok
            );
            if (ok && !newName.trimmed().isEmpty()) {
                page->getDocument()->openTransaction("Rename Sheet");
                page->Label.setValue(newName.trimmed().toUtf8().constData());
                Drawing::TitleBlockBinder::bindTitleBlock(page, nullptr, false);
                page->getDocument()->recompute();
                page->getDocument()->commitTransaction();
                rebuildTree();
            }
        }
        else if (chosen == actUpdate) {
            DrawingUpdateController::instance()->updatePage(page);
            rebuildTree();
        }
        else if (chosen == actDelete) {
            App::Document* doc = page->getDocument();
            doc->openTransaction("Delete Sheet");
            doc->removeObject(page->getNameInDocument());
            Drawing::TitleBlockBinder::refreshAllPagesInDocument(doc);
            doc->recompute();
            doc->commitTransaction();
            rebuildTree();
        }
    }
    else if (auto* dv = dynamic_cast<TechDraw::DrawView*>(obj)) {
        const auto alignMode = Drawing::ProjectionMath::getViewAlignmentMode(dv);
        QAction* actBreak = nullptr;
        QAction* actRestore = nullptr;
        if (alignMode == Drawing::AlignmentKind::Horizontal
            || alignMode == Drawing::AlignmentKind::Vertical
            || alignMode == Drawing::AlignmentKind::Vector) {
            actBreak = menu.addAction(tr("Break Alignment"));
        }
        else if (alignMode == Drawing::AlignmentKind::Broken
                 && Drawing::ProjectionMath::getParentAlignedView(dv) != nullptr) {
            actRestore = menu.addAction(tr("Restore Alignment"));
        }

        QAction* actLock = menu.addAction(dv->isLocked() ? tr("Unlock Position") : tr("Lock Position"));
        menu.addSeparator();
        QAction* actDelete = menu.addAction(tr("Delete View"));

        QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen && chosen == actBreak) {
            dv->getDocument()->openTransaction("Break View Alignment");
            Drawing::ProjectionMath::breakViewAlignment(dv);
            dv->getDocument()->commitTransaction();
            rebuildTree();
        }
        else if (chosen && chosen == actRestore) {
            dv->getDocument()->openTransaction("Restore View Alignment");
            Drawing::ProjectionMath::restoreViewAlignment(dv);
            dv->recomputeFeature();
            dv->getDocument()->commitTransaction();
            rebuildTree();
        }
        else if (chosen == actLock) {
            dv->getDocument()->openTransaction("Toggle Lock View");
            dv->LockPosition.setValue(!dv->isLocked());
            dv->getDocument()->commitTransaction();
        }
        else if (chosen == actDelete) {
            App::Document* doc = dv->getDocument();
            doc->openTransaction("Delete View");
            doc->removeObject(dv->getNameInDocument());
            doc->recompute();
            doc->commitTransaction();
            rebuildTree();
        }
    }
}

}  // namespace DrawingGui
