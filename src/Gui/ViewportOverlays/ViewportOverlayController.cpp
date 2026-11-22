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

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMdiArea>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTimer>

#include <App/Application.h>
#include <App/DocumentObject.h>
#include <Base/Type.h>
#include <Gui/Application.h>
#include <Gui/Control.h>
#include <Gui/MDIView.h>
#include <Gui/MainWindow.h>
#include <Gui/TaskView/TaskDialog.h>
#include <Gui/View3DInventor.h>
#include <Gui/View3DInventorViewer.h>
#include <Gui/ViewProviderDocumentObject.h>

#include "ConfirmationCorner.h"
#include "ContextActionRegistry.h"
#include "ContextMiniToolbar.h"
#include "HeadsUpViewBar.h"
#include "ShortcutBarPopup.h"
#include "ViewportOverlayController.h"

namespace Gui
{

namespace
{

constexpr int kClickDragThresholdPx = 6;

QString displayNameForShortcutContext(const std::string& contextId)
{
    if (contextId == "Part") {
        return QCoreApplication::translate("ShortcutBar", "Part Shortcuts");
    }
    if (contextId == "Sketch") {
        return QCoreApplication::translate("ShortcutBar", "Sketch Shortcuts");
    }
    if (contextId == "Assembly") {
        return QCoreApplication::translate("ShortcutBar", "Assembly Shortcuts");
    }
    if (contextId == "Drawing") {
        return QCoreApplication::translate("ShortcutBar", "Drawing Shortcuts");
    }
    return QCoreApplication::translate("ShortcutBar", "Quick Shortcuts");
}

}  // namespace

ViewportOverlayController::ViewportOverlayController(MainWindow* mainWindow)
    : QObject(mainWindow)
    , SelectionObserver(true)
    , m_mainWindow(mainWindow)
{
    ContextActionRegistry::instance().ensureDefaultsRegistered();

    m_mdiArea = m_mainWindow ? m_mainWindow->getMdiArea() : nullptr;
    QWidget* overlayHost
        = m_mdiArea ? static_cast<QWidget*>(m_mdiArea.data()) : static_cast<QWidget*>(m_mainWindow.data());

    m_headsUpBar = new HeadsUpViewBar(overlayHost);
    m_confirmationCorner = new ConfirmationCorner(overlayHost);
    m_miniToolbar = new ContextMiniToolbar(m_mainWindow);
    m_shortcutPopup = new ShortcutBarPopup(m_mainWindow);

    if (m_mdiArea) {
        m_mdiArea->installEventFilter(this);
    }

    connect(m_confirmationCorner, &ConfirmationCorner::accepted, this, []() {
        Control().accept();
    });
    connect(m_confirmationCorner, &ConfirmationCorner::rejected, this, []() {
        Control().reject();
    });

    connect(&Control(), &ControlSingleton::dialogShown, this, &ViewportOverlayController::onTaskDialogShown);
    connect(&Control(), &ControlSingleton::dialogClosed, this, &ViewportOverlayController::onTaskDialogClosed);

    if (m_mainWindow) {
        connect(m_mainWindow, &MainWindow::windowActivated, this, &ViewportOverlayController::onWindowActivated);
        connect(
            m_mainWindow,
            &MainWindow::workbenchActivated,
            this,
            &ViewportOverlayController::onWorkbenchActivated
        );
    }

    m_hMainWin = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    m_hNaviCube = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/NaviCube"
    );
    loadPreferences();

    m_connParam = App::GetApplication().GetUserParameter().signalParamChanged.connect(
        [this](ParameterGrp* param, ParameterGrp::ParamType, const char* name, const char*) {
            if (!name) {
                return;
            }
            if (param == m_hMainWin || param == m_hNaviCube) {
                loadPreferences();
                refreshHeadsUpBar();
                refreshConfirmationCorner();
                repositionOverlays();
                if (!m_showContextMiniToolbar && m_miniToolbar) {
                    m_miniToolbar->dismiss();
                }
            }
        }
    );

    if (Application::Instance) {
        m_connInEdit = Application::Instance->signalInEdit.connect(
            [this](const ViewProviderDocumentObject& vp) {
                const App::DocumentObject* obj = vp.getObject();
                const Base::Type sketchType = Base::Type::fromName("Sketcher::SketchObject");
                if (obj && !sketchType.isBad() && obj->isDerivedFrom(sketchType)) {
                    m_inSketchEdit = true;
                }
            }
        );
        m_connResetEdit = Application::Instance->signalResetEdit.connect(
            [this](const ViewProviderDocumentObject& vp) {
                const App::DocumentObject* obj = vp.getObject();
                const Base::Type sketchType = Base::Type::fromName("Sketcher::SketchObject");
                if (obj && !sketchType.isBad() && obj->isDerivedFrom(sketchType)) {
                    m_inSketchEdit = false;
                }
            }
        );
    }
}

ViewportOverlayController::~ViewportOverlayController()
{
    detachFromView();
    if (m_mdiArea) {
        m_mdiArea->removeEventFilter(this);
    }
}

HeadsUpViewBar* ViewportOverlayController::headsUpViewBar() const
{
    return m_headsUpBar;
}

ConfirmationCorner* ViewportOverlayController::confirmationCorner() const
{
    return m_confirmationCorner;
}

ContextMiniToolbar* ViewportOverlayController::contextMiniToolbar() const
{
    return m_miniToolbar;
}

ShortcutBarPopup* ViewportOverlayController::shortcutBarPopup() const
{
    return m_shortcutPopup;
}

void ViewportOverlayController::loadPreferences()
{
    m_showHeadsUpBar = m_hMainWin->GetBool("ShowHeadsUpViewBar", true);
    m_showConfirmationCorner = m_hMainWin->GetBool("ShowConfirmationCorner", true);
    m_showContextMiniToolbar = m_hMainWin->GetBool("ShowContextMiniToolbar", true);
    m_enableShortcutBar = m_hMainWin->GetBool("EnableShortcutBar", true);
}

bool ViewportOverlayController::isView3D(const MDIView* view)
{
    return view && view->isDerivedFrom<View3DInventor>();
}

bool ViewportOverlayController::isDrawingView(const MDIView* view)
{
    if (!view) {
        return false;
    }
    const Base::Type tdPageType = Base::Type::fromName("TechDrawGui::MDIViewPage");
    return !tdPageType.isBad() && view->isDerivedFrom(tdPageType);
}

bool ViewportOverlayController::isTextInputFocused()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused) {
        return false;
    }
    if (qobject_cast<QLineEdit*>(focused) || qobject_cast<QTextEdit*>(focused)
        || qobject_cast<QPlainTextEdit*>(focused) || qobject_cast<QAbstractSpinBox*>(focused)) {
        return true;
    }
    if (auto* combo = qobject_cast<QComboBox*>(focused)) {
        return combo->isEditable();
    }
    return false;
}

bool ViewportOverlayController::isTopRightNaviCubeVisible()
{
    auto hNavi = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/NaviCube"
    );
    const bool showCube = hNavi->GetBool("ShowNaviCube", true);
    const int corner = static_cast<int>(hNavi->GetInt("CornerNaviCube", 1));
    // Corner 1 is NaviCube::TopRight
    return showCube && corner == 1;
}

void ViewportOverlayController::detachFromView()
{
    if (m_watchedViewport) {
        m_watchedViewport->removeEventFilter(this);
        m_watchedViewport = nullptr;
    }
    if (m_activeView) {
        m_activeView->removeEventFilter(this);
        m_activeView = nullptr;
    }
}

void ViewportOverlayController::attachToView(MDIView* view)
{
    detachFromView();
    m_activeView = view;
    if (!m_activeView) {
        return;
    }

    m_activeView->installEventFilter(this);

    // Install on the inner graphics viewport so mouse releases and S-key presses inside
    // Coin3D (View3DInventorViewer) or TechDraw (QGraphicsView) are observed cleanly.
    if (auto* view3d = freecad_cast<View3DInventor*>(m_activeView.data())) {
        if (auto* viewer = view3d->getViewer()) {
            m_watchedViewport = viewer->viewport() ? viewer->viewport() : static_cast<QWidget*>(viewer);
        }
    }
    else if (QWidget* central = m_activeView->centralWidget()) {
        if (auto* scrollArea = qobject_cast<QAbstractScrollArea*>(central)) {
            m_watchedViewport = scrollArea->viewport() ? scrollArea->viewport() : central;
        }
        else {
            m_watchedViewport = central;
        }
    }

    if (m_watchedViewport && m_watchedViewport != m_activeView) {
        m_watchedViewport->installEventFilter(this);
    }
}

void ViewportOverlayController::onWindowActivated(MDIView* view)
{
    if (m_miniToolbar) {
        m_miniToolbar->dismiss();
    }
    attachToView(view);
    refreshHeadsUpBar();
    refreshConfirmationCorner();
    repositionOverlays();
}

void ViewportOverlayController::onWorkbenchActivated(const QString& workbenchName)
{
    m_activeWorkbench = workbenchName.toStdString();
    if (m_miniToolbar) {
        m_miniToolbar->dismiss();
    }
    refreshHeadsUpBar();
    repositionOverlays();
}

void ViewportOverlayController::onTaskDialogShown(TaskView::TaskDialog* dialog)
{
    if (!m_confirmationCorner) {
        return;
    }
    if (!m_showConfirmationCorner || !m_activeView
        || (!isView3D(m_activeView) && !isDrawingView(m_activeView))) {
        m_confirmationCorner->hide();
        return;
    }
    m_confirmationCorner->setActiveDialog(dialog);
    repositionOverlays();
}

void ViewportOverlayController::onTaskDialogClosed()
{
    if (m_confirmationCorner) {
        m_confirmationCorner->setActiveDialog(Control().activeDialog());
        repositionOverlays();
    }
}

void ViewportOverlayController::refreshHeadsUpBar()
{
    if (!m_headsUpBar) {
        return;
    }
    if (!m_showHeadsUpBar || !m_activeView) {
        m_headsUpBar->setMode(HeadsUpViewBar::Mode::Hidden);
        return;
    }

    if (isView3D(m_activeView)) {
        m_headsUpBar->setMode(HeadsUpViewBar::Mode::View3D);
    }
    else if (isDrawingView(m_activeView)) {
        m_headsUpBar->setMode(HeadsUpViewBar::Mode::Drawing2D);
    }
    else {
        m_headsUpBar->setMode(HeadsUpViewBar::Mode::Hidden);
    }
}

void ViewportOverlayController::refreshConfirmationCorner()
{
    if (!m_confirmationCorner) {
        return;
    }
    if (!m_showConfirmationCorner || !m_activeView
        || (!isView3D(m_activeView) && !isDrawingView(m_activeView))) {
        m_confirmationCorner->hide();
        return;
    }
    m_confirmationCorner->setActiveDialog(Control().activeDialog());
}

void ViewportOverlayController::repositionOverlays()
{
    if (!m_activeView || !m_mdiArea) {
        return;
    }

    const QPoint topLeft = m_activeView->mapTo(m_mdiArea, QPoint(0, 0));
    const QRect viewRect(topLeft, m_activeView->size());

    if (m_headsUpBar && !m_headsUpBar->isHidden()) {
        m_headsUpBar->updateGeometryInRect(viewRect);
    }
    if (m_confirmationCorner && !m_confirmationCorner->isHidden()) {
        const bool avoidCube = isView3D(m_activeView) && isTopRightNaviCubeVisible();
        m_confirmationCorner->updateGeometryInRect(viewRect, avoidCube);
    }
}

void ViewportOverlayController::onSelectionChanged(const SelectionChanges& msg)
{
    if (!m_showContextMiniToolbar || !m_miniToolbar) {
        return;
    }

    if (msg.Type == SelectionChanges::ClrSelection) {
        m_selectionPendingMiniBar = false;
        m_miniToolbar->dismiss();
        return;
    }

    if (msg.Type == SelectionChanges::RmvSelection) {
        if (!Selection().hasSelection()) {
            m_selectionPendingMiniBar = false;
            m_miniToolbar->dismiss();
        }
        return;
    }

    if (msg.Type == SelectionChanges::AddSelection || msg.Type == SelectionChanges::SetSelection) {
        m_selectionPendingMiniBar = true;
    }
}

void ViewportOverlayController::maybeShowMiniToolbar(const QPoint& globalPos)
{
    m_selectionPendingMiniBar = false;
    if (!m_showContextMiniToolbar || !m_miniToolbar || !Selection().hasSelection()) {
        return;
    }
    // Avoid popping over an active task dialog edit session (e.g., picking references).
    if (Control().activeDialog() != nullptr) {
        return;
    }

    const auto commands
        = ContextActionRegistry::instance().resolveMiniToolbarCommands(m_activeWorkbench);
    if (!commands.empty()) {
        m_miniToolbar->showForCommands(commands, globalPos);
    }
    else {
        m_miniToolbar->dismiss();
    }
}

void ViewportOverlayController::triggerShortcutBarAtCursor()
{
    if (!m_enableShortcutBar || !m_shortcutPopup || !m_activeView) {
        return;
    }
    if (!isView3D(m_activeView) && !isDrawingView(m_activeView)) {
        return;
    }

    const std::string contextId = ContextActionRegistry::shortcutContextForState(
        m_activeWorkbench,
        m_inSketchEdit,
        isDrawingView(m_activeView)
    );
    const auto commands = ContextActionRegistry::instance().resolveShortcutBarCommands(contextId);
    if (!commands.empty()) {
        if (m_miniToolbar) {
            m_miniToolbar->dismiss();
        }
        m_shortcutPopup->showForContext(
            displayNameForShortcutContext(contextId),
            commands,
            QCursor::pos()
        );
    }
}

bool ViewportOverlayController::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return false;
    }

    switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
            repositionOverlays();
            break;

        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            m_mousePressGlobalPos = me->globalPosition().toPoint();
            if (m_miniToolbar) {
                m_miniToolbar->dismiss();
            }
            break;
        }

        case QEvent::Wheel:
            if (m_miniToolbar) {
                m_miniToolbar->dismiss();
            }
            break;

        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && m_showContextMiniToolbar) {
                const QPoint releasePos = me->globalPosition().toPoint();
                if ((releasePos - m_mousePressGlobalPos).manhattanLength() <= kClickDragThresholdPx) {
                    QTimer::singleShot(0, this, [this, releasePos]() {
                        if (m_selectionPendingMiniBar) {
                            maybeShowMiniToolbar(releasePos);
                        }
                    });
                }
                else {
                    m_selectionPendingMiniBar = false;
                }
            }
            break;
        }

        case QEvent::KeyPress: {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape && m_miniToolbar && m_miniToolbar->isVisible()) {
                m_miniToolbar->dismiss();
                break;
            }
            if (ke->key() == Qt::Key_S && ke->modifiers() == Qt::NoModifier && !ke->isAutoRepeat()
                && m_enableShortcutBar && !isTextInputFocused()) {
                triggerShortcutBarAtCursor();
                return true;
            }
            break;
        }

        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}

}  // namespace Gui
