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

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Gui/MainWindow.h>
#include <Mod/TechDraw/Gui/MDIViewPage.h>
#include <Mod/TechDraw/Gui/QGVPage.h>
#include <Mod/TechDraw/Gui/ViewProviderPage.h>

#include "HandlerInsertView.h"
#include "TaskSheetSetup.h"
#include "ViewPaletteDock.h"

namespace DrawingGui
{

ViewPaletteDock::ViewPaletteDock(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("DrawingViewPaletteDock"));
    setWindowTitle(tr("View Palette"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* srcHeader = new QHBoxLayout();
    srcHeader->addWidget(new QLabel(tr("Model:"), this));
    auto* refreshBtn = new QPushButton(tr("↻"), this);
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip(tr("Refresh model list"));
    connect(refreshBtn, &QPushButton::clicked, this, &ViewPaletteDock::refreshSources);
    srcHeader->addStretch(1);
    srcHeader->addWidget(refreshBtn);
    root->addLayout(srcHeader);

    m_sourceCombo = new QComboBox(this);
    root->addWidget(m_sourceCombo);

    auto* hintLbl = new QLabel(tr("Click an orientation to place on sheet:"), this);
    hintLbl->setWordWrap(true);
    root->addWidget(hintLbl);

    auto* grid = new QGridLayout();
    grid->setSpacing(6);

    struct OrientationCard
    {
        const char* title;
        Drawing::StandardOrientation orient;
        int row;
        int col;
    };
    const OrientationCard cards[] = {
        {"Front", Drawing::StandardOrientation::Front, 0, 0},
        {"Top", Drawing::StandardOrientation::Top, 0, 1},
        {"Right", Drawing::StandardOrientation::Right, 1, 0},
        {"Left", Drawing::StandardOrientation::Left, 1, 1},
        {"Back", Drawing::StandardOrientation::Back, 2, 0},
        {"Bottom", Drawing::StandardOrientation::Bottom, 2, 1},
        {"Isometric", Drawing::StandardOrientation::Isometric, 3, 0},
    };

    for (const auto& card : cards) {
        auto* btn = new QPushButton(tr(card.title), this);
        btn->setMinimumHeight(40);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr("Place %1 view on the active drawing sheet").arg(tr(card.title)));
        const auto orient = card.orient;
        connect(btn, &QPushButton::clicked, this, [this, orient]() {
            startInsertOrientation(orient);
        });
        if (card.orient == Drawing::StandardOrientation::Isometric) {
            grid->addWidget(btn, card.row, card.col, 1, 2);
        }
        else {
            grid->addWidget(btn, card.row, card.col);
        }
    }
    root->addLayout(grid);

    m_autoProjectedCheck = new QCheckBox(tr("Auto-start Projected View"), this);
    m_autoProjectedCheck->setChecked(true);
    m_autoProjectedCheck->setToolTip(
        tr("Automatically project orthogonal/isometric views right after placing a base view")
    );
    root->addWidget(m_autoProjectedCheck);

    m_autoCentermarksCheck = new QCheckBox(tr("Auto-insert Hole Centermarks"), this);
    m_autoCentermarksCheck->setChecked(true);
    m_autoCentermarksCheck->setToolTip(
        tr("Show centermarks on circular arcs/holes in newly placed views")
    );
    root->addWidget(m_autoCentermarksCheck);

    root->addStretch(1);

    m_connNewObj = App::GetApplication().signalNewObject.connect(
        [this](const App::DocumentObject&) { refreshSources(); }
    );
    m_connDelObj = App::GetApplication().signalDeletedObject.connect(
        [this](const App::DocumentObject&) { refreshSources(); }
    );
    m_connActiveDoc = App::GetApplication().signalActiveDocument.connect(
        [this](const App::Document&) { refreshSources(); }
    );

    refreshSources();
}

ViewPaletteDock::~ViewPaletteDock() = default;

void ViewPaletteDock::refreshSources()
{
    App::DocumentObject* previous = selectedSource();
    m_sourceCombo->clear();
    m_sources = SheetSetupDialog::collectDrawableSources(App::GetApplication().getActiveDocument());

    if (m_sources.empty()) {
        m_sourceCombo->addItem(tr("(No 3D model available)"));
        m_sourceCombo->setEnabled(false);
        return;
    }

    m_sourceCombo->setEnabled(true);
    int selectIdx = 0;
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        m_sourceCombo->addItem(QString::fromUtf8(m_sources[i]->Label.getValue()));
        if (m_sources[i] == previous) {
            selectIdx = static_cast<int>(i);
        }
    }
    m_sourceCombo->setCurrentIndex(selectIdx);
}

App::DocumentObject* ViewPaletteDock::selectedSource() const
{
    const int idx = m_sourceCombo ? m_sourceCombo->currentIndex() : -1;
    if (idx < 0 || static_cast<std::size_t>(idx) >= m_sources.size()) {
        return nullptr;
    }
    return m_sources[static_cast<std::size_t>(idx)];
}

bool ViewPaletteDock::autoStartProjectedEnabled() const
{
    return !m_autoProjectedCheck || m_autoProjectedCheck->isChecked();
}

void ViewPaletteDock::startInsertOrientation(Drawing::StandardOrientation orientation)
{
    App::DocumentObject* src = selectedSource();
    if (!src) {
        return;
    }

    Gui::MainWindow* mw = Gui::getMainWindow();
    if (!mw) {
        return;
    }
    auto* mdiPage = qobject_cast<TechDrawGui::MDIViewPage*>(mw->activeWindow());
    if (!mdiPage || !mdiPage->getViewProviderPage()) {
        return;
    }
    TechDrawGui::QGVPage* gvPage = mdiPage->getViewProviderPage()->getQGVPage();
    if (!gvPage) {
        return;
    }

    const bool autoCenter = m_autoCentermarksCheck && m_autoCentermarksCheck->isChecked();
    gvPage->activateHandler(
        new HandlerInsertView(src, orientation, autoStartProjectedEnabled(), autoCenter)
    );
}

}  // namespace DrawingGui
