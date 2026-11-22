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

#include <string>

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <fastsignals/signal.h>

#include <Base/Parameter.h>
#include <FCGlobal.h>
#include <Gui/Selection/Selection.h>

class QMdiArea;
class QWidget;

namespace Gui
{

class ConfirmationCorner;
class ContextMiniToolbar;
class HeadsUpViewBar;
class MDIView;
class MainWindow;
class ShortcutBarPopup;

namespace TaskView
{
class TaskDialog;
}

/**
 * Coordinates the SolidWorks-inspired viewport overlays across active 3D and 2D MDIViews:
 * - HeadsUpViewBar (top-center viewport overlay)
 * - ConfirmationCorner (top-right OK / Cancel overlay during active TaskDialogs)
 * - ContextMiniToolbar (near-cursor selection quick-action bar)
 * - ShortcutBarPopup (S-Key context palette)
 */
class GuiExport ViewportOverlayController: public QObject, public SelectionObserver
{
    Q_OBJECT

public:
    explicit ViewportOverlayController(MainWindow* mainWindow);
    ~ViewportOverlayController() override;

    HeadsUpViewBar* headsUpViewBar() const;
    ConfirmationCorner* confirmationCorner() const;
    ContextMiniToolbar* contextMiniToolbar() const;
    ShortcutBarPopup* shortcutBarPopup() const;

    void triggerShortcutBarAtCursor();

public Q_SLOTS:
    void onWindowActivated(Gui::MDIView* view);
    void onWorkbenchActivated(const QString& workbenchName);
    void onTaskDialogShown(Gui::TaskView::TaskDialog* dialog);
    void onTaskDialogClosed();
    void repositionOverlays();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void onSelectionChanged(const SelectionChanges& msg) override;

private:
    void loadPreferences();
    void attachToView(MDIView* view);
    void detachFromView();
    void refreshHeadsUpBar();
    void refreshConfirmationCorner();
    void maybeShowMiniToolbar(const QPoint& globalPos);

    static bool isView3D(const MDIView* view);
    static bool isDrawingView(const MDIView* view);
    static bool isTextInputFocused();
    static bool isTopRightNaviCubeVisible();

    QPointer<MainWindow> m_mainWindow;
    QPointer<QMdiArea> m_mdiArea;
    QPointer<MDIView> m_activeView;
    QPointer<QWidget> m_watchedViewport;

    QPointer<HeadsUpViewBar> m_headsUpBar;
    QPointer<ConfirmationCorner> m_confirmationCorner;
    QPointer<ContextMiniToolbar> m_miniToolbar;
    QPointer<ShortcutBarPopup> m_shortcutPopup;

    std::string m_activeWorkbench;
    bool m_inSketchEdit {false};
    bool m_selectionPendingMiniBar {false};
    QPoint m_mousePressGlobalPos;

    bool m_showHeadsUpBar {true};
    bool m_showConfirmationCorner {true};
    bool m_showContextMiniToolbar {true};
    bool m_enableShortcutBar {true};

    ParameterGrp::handle m_hMainWin;
    ParameterGrp::handle m_hNaviCube;
    fastsignals::scoped_connection m_connParam;
    fastsignals::scoped_connection m_connInEdit;
    fastsignals::scoped_connection m_connResetEdit;
};

}  // namespace Gui
