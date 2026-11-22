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

#pragma once

#include <set>
#include <string>

#include <QObject>
#include <QPointer>
#include <fastsignals/signal.h>

#include <Gui/Selection/Selection.h>
#include <Mod/Drawing/DrawingGlobal.h>

class QPushButton;

namespace App
{
class Document;
class DocumentObject;
class Property;
}  // namespace App

namespace Gui
{
class MDIView;
}  // namespace Gui

namespace TechDraw
{
class DrawPage;
}  // namespace TechDraw

namespace DrawingGui
{

class DimensionPalettePopup;

/**
 * Coordinates SolidWorks/Onshape-style deferred 3D-to-Drawing updates:
 * - While modeling in 3D, suspends costly live HLR on DrawPages whose 3D sources changed
 *   and displays a highlighted "Update Drawing" badge in the Heads-Up View Bar.
 * - Automatically updates pending DrawPages when switching back to a Drawing tab.
 * - Clicking the badge or invoking Drawing_UpdateDrawing forces an immediate sync.
 * - Also hosts the floating DimensionPalettePopup for quick dimension formatting.
 */
class DrawingGuiExport DrawingUpdateController: public QObject, public Gui::SelectionObserver
{
    Q_OBJECT

public:
    static DrawingUpdateController* instance();

    void install();
    bool hasPendingUpdates(const App::Document* doc = nullptr) const;
    bool isPageDirty(const TechDraw::DrawPage* page) const;

    void markPageDirty(TechDraw::DrawPage* page);
    void updatePage(TechDraw::DrawPage* page);
    void updateAllPendingPages(App::Document* doc = nullptr);

    void showDimensionPaletteForSelection(bool forceShow = true);
    DimensionPalettePopup* dimensionPalette() const;

Q_SIGNALS:
    void dirtyStateChanged();

private Q_SLOTS:
    void onWindowActivated(Gui::MDIView* view);
    void onBadgeClicked();

protected:
    void onSelectionChanged(const Gui::SelectionChanges& msg) override;

private:
    explicit DrawingUpdateController(QObject* parent = nullptr);
    ~DrawingUpdateController() override;

    void onObjectChanged(const App::DocumentObject& obj, const App::Property& prop);
    void onDeletedObject(const App::DocumentObject& obj);
    void onDeletedDocument(const App::Document& doc);
    void updateBadgeAppearance();
    static bool pageDependsOnObject(const TechDraw::DrawPage* page, const App::DocumentObject* obj);
    static TechDraw::DrawPage* activeDrawingPage();

    bool m_installed {false};
    bool m_updating {false};
    std::set<const TechDraw::DrawPage*> m_dirtyPages;
    std::set<const TechDraw::DrawPage*> m_deferredPages;

    QPointer<QPushButton> m_updateBadge;
    QPointer<DimensionPalettePopup> m_dimensionPalette;

    fastsignals::scoped_connection m_connChangedObj;
    fastsignals::scoped_connection m_connDeletedObj;
    fastsignals::scoped_connection m_connDeletedDoc;
};

}  // namespace DrawingGui
