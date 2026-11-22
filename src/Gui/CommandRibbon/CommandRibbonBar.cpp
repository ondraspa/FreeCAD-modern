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

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <App/Application.h>
#include <App/DocumentObject.h>
#include <Base/Parameter.h>
#include <Base/Type.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/MDIView.h>
#include <Gui/ToolBarManager.h>
#include <Gui/ViewProviderDocumentObject.h>
#include <Gui/WorkbenchManager.h>

#include "CommandRibbonBar.h"

namespace Gui
{

namespace
{

constexpr int kQuickAccessIconPx = 16;
constexpr int kLargeRibbonIconPx = 24;
constexpr int kCompactRibbonIconPx = 16;
constexpr int kCompactRowsPerColumn = 2;

bool isStandardUtilityToolbar(const std::string& name)
{
    return name == "File" || name == "Edit" || name == "Clipboard" || name == "Workbench"
        || name == "Macro" || name == "View" || name == "Individual views"
        || name == "Structure" || name == "Help";
}

}  // namespace

CommandRibbonBar::CommandRibbonBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("CommandRibbonBar"));
    CommandRibbonRegistry::instance().ensureDefaultsRegistered();

    setupUi();
    setupQuickAccessBar();
    connectApplicationSignals();

    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    setCollapsed(hGrp->GetBool("CommandRibbonCollapsed", false));
    retranslateUi();
}

CommandRibbonBar::~CommandRibbonBar() = default;

void CommandRibbonBar::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Top row: Quick Access + Context Tab Strip + "+ More" Workbench Switcher + Collapse Toggle
    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName(QStringLiteral("commandRibbonHeader"));
    auto* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(6, 2, 6, 0);
    headerLayout->setSpacing(6);

    m_quickAccessBar = new QToolBar(m_headerWidget);
    m_quickAccessBar->setObjectName(QStringLiteral("*commandRibbonQuickAccess"));
    m_quickAccessBar->setMovable(false);
    m_quickAccessBar->setFloatable(false);
    m_quickAccessBar->setIconSize(QSize(kQuickAccessIconPx, kQuickAccessIconPx));
    m_quickAccessBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    headerLayout->addWidget(m_quickAccessBar);

    auto* headerDivider = new QFrame(m_headerWidget);
    headerDivider->setObjectName(QStringLiteral("commandRibbonHeaderDivider"));
    headerDivider->setFrameShape(QFrame::VLine);
    headerDivider->setFrameShadow(QFrame::Plain);
    headerLayout->addWidget(headerDivider);

    m_tabBar = new QTabBar(m_headerWidget);
    m_tabBar->setObjectName(QStringLiteral("commandRibbonTabBar"));
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setUsesScrollButtons(true);
    connect(m_tabBar, &QTabBar::currentChanged, this, &CommandRibbonBar::onTabBarCurrentChanged);
    headerLayout->addWidget(m_tabBar);

    headerLayout->addStretch(1);

    m_moreMenu = new QMenu(this);
    m_moreButton = new QToolButton(m_headerWidget);
    m_moreButton->setObjectName(QStringLiteral("commandRibbonMoreButton"));
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_moreButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_moreButton->setMenu(m_moreMenu);
    connect(m_moreMenu, &QMenu::aboutToShow, this, &CommandRibbonBar::rebuildMoreWorkbenchesMenu);
    headerLayout->addWidget(m_moreButton);

    m_collapseButton = new QToolButton(m_headerWidget);
    m_collapseButton->setObjectName(QStringLiteral("commandRibbonCollapseButton"));
    m_collapseButton->setCheckable(true);
    m_collapseButton->setAutoRaise(true);
    connect(m_collapseButton, &QToolButton::toggled, this, &CommandRibbonBar::onCollapseToggled);
    headerLayout->addWidget(m_collapseButton);

    rootLayout->addWidget(m_headerWidget);

    // Bottom row: Horizontally scrollable command strip
    m_contentScroll = new QScrollArea(this);
    m_contentScroll->setObjectName(QStringLiteral("commandRibbonScroll"));
    m_contentScroll->setFrameShape(QFrame::NoFrame);
    m_contentScroll->setWidgetResizable(true);
    m_contentScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_contentScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_contentScroll->setFixedHeight(78);

    m_stripContainer = new QWidget(m_contentScroll);
    m_stripContainer->setObjectName(QStringLiteral("commandRibbonStrip"));
    m_stripLayout = new QHBoxLayout(m_stripContainer);
    m_stripLayout->setContentsMargins(6, 3, 6, 3);
    m_stripLayout->setSpacing(4);
    m_stripLayout->addStretch(1);

    m_contentScroll->setWidget(m_stripContainer);
    m_contentScroll->hide();
    m_collapseButton->hide();
    rootLayout->addWidget(m_contentScroll);
}

void CommandRibbonBar::setupQuickAccessBar()
{
    if (!Application::Instance) {
        return;
    }
    CommandManager& cmdMgr = Application::Instance->commandManager();
    for (const char* cmdName : {"Std_New", "Std_Open", "Std_Save", "Std_Undo", "Std_Redo"}) {
        cmdMgr.addTo(cmdName, m_quickAccessBar);
    }
}

void CommandRibbonBar::connectApplicationSignals()
{
    if (!Application::Instance) {
        return;
    }

    m_connInEdit = Application::Instance->signalInEdit.connect(
        [this](const ViewProviderDocumentObject& vp) {
            if (isSketchObjectType(vp)) {
                m_inSketchEdit = true;
                refreshContext(m_currentWorkbench, m_inSketchEdit, m_isDrawingView);
            }
        }
    );

    m_connResetEdit = Application::Instance->signalResetEdit.connect(
        [this](const ViewProviderDocumentObject& vp) {
            if (isSketchObjectType(vp) && m_inSketchEdit) {
                m_inSketchEdit = false;
                refreshContext(m_currentWorkbench, m_inSketchEdit, m_isDrawingView);
            }
        }
    );

    m_connRefreshWorkbenches = Application::Instance->signalRefreshWorkbenches.connect([this]() {
        rebuildMoreWorkbenchesMenu();
    });
}

bool CommandRibbonBar::isSketchObjectType(const ViewProviderDocumentObject& vp)
{
    const App::DocumentObject* obj = vp.getObject();
    if (!obj) {
        return false;
    }
    const Base::Type sketchType = Base::Type::fromName("Sketcher::SketchObject");
    return !sketchType.isBad() && obj->isDerivedFrom(sketchType);
}

bool CommandRibbonBar::isDrawingMdiView(const MDIView* view)
{
    if (!view) {
        return false;
    }
    const Base::Type drawingPageViewType = Base::Type::fromName("TechDrawGui::MDIViewPage");
    return !drawingPageViewType.isBad() && view->isDerivedFrom(drawingPageViewType);
}

void CommandRibbonBar::onWorkbenchActivated(const QString& workbenchName)
{
    refreshContext(workbenchName.toStdString(), m_inSketchEdit, m_isDrawingView);
}

void CommandRibbonBar::onWindowActivated(MDIView* view)
{
    const bool drawingNow = isDrawingMdiView(view);
    if (drawingNow != m_isDrawingView) {
        m_isDrawingView = drawingNow;
        refreshContext(m_currentWorkbench, m_inSketchEdit, m_isDrawingView);
    }
}

void CommandRibbonBar::setFallbackToolBarItems(
    const ToolBarItem* rootItems,
    const std::string& workbenchName
)
{
    m_fallbackGroups.clear();
    m_fallbackWorkbench = workbenchName;
    if (!rootItems) {
        return;
    }

    const QList<ToolBarItem*> toolbars = rootItems->getItems();
    for (ToolBarItem* tbItem : toolbars) {
        if (!tbItem || isStandardUtilityToolbar(tbItem->command())) {
            continue;
        }
        RibbonGroupDef group;
        group.id = "wb_tb_" + tbItem->command();
        group.title = tbItem->command();
        for (ToolBarItem* cmdItem : tbItem->getItems()) {
            if (!cmdItem || cmdItem->command() == "Separator") {
                continue;
            }
            group.commands.push_back(RibbonCommandEntry {
                .commandName = cmdItem->command(),
                .size = RibbonButtonSize::Large,
            });
        }
        if (!group.commands.empty()) {
            m_fallbackGroups.push_back(std::move(group));
        }
    }

    if (!CommandRibbonRegistry::instance().hasContext(workbenchName) && !m_isDrawingView) {
        refreshContext(workbenchName, m_inSketchEdit, m_isDrawingView);
    }
}

std::vector<RibbonGroupDef> CommandRibbonBar::buildFallbackGroupsFromToolBars() const
{
    if (m_fallbackWorkbench == m_currentWorkbench && !m_fallbackGroups.empty()) {
        return m_fallbackGroups;
    }
    return {};
}

void CommandRibbonBar::refreshContext(
    const std::string& workbenchName,
    bool inSketchEdit,
    bool isDrawingView
)
{
    m_currentWorkbench = workbenchName;
    m_inSketchEdit = inSketchEdit;
    m_isDrawingView = isDrawingView;

    auto& registry = CommandRibbonRegistry::instance();
    registry.ensureDefaultsRegistered();

    m_visibleTabs = registry.resolveTabsForContext(workbenchName, inSketchEdit, isDrawingView);
    std::string targetTabId = registry.resolveActiveTabId(workbenchName, inSketchEdit, isDrawingView);

    // For workbenches without an explicit curated context (CAM, FEM, BIM, Draft, Addons),
    // synthesize a clean ribbon tab from their registered toolbars + Evaluate tab.
    if (m_visibleTabs.empty() && !workbenchName.empty() && workbenchName != "NoneWorkbench") {
        RibbonTabDef fallbackTab;
        fallbackTab.id = "fallback_" + workbenchName;
        fallbackTab.workbenchName = workbenchName;
        if (Application::Instance) {
            QString menuText = Application::Instance->workbenchMenuText(
                QString::fromStdString(workbenchName)
            );
            fallbackTab.title = !menuText.isEmpty() ? menuText.toStdString() : workbenchName;
        }
        else {
            fallbackTab.title = workbenchName;
        }
        fallbackTab.groups = buildFallbackGroupsFromToolBars();
        m_visibleTabs.push_back(std::move(fallbackTab));
        if (const auto* evalTab = registry.findTab("evaluate_tools")) {
            m_visibleTabs.push_back(*evalTab);
        }
        if (targetTabId.empty()) {
            targetTabId = m_visibleTabs.front().id;
        }
    }

    m_updatingTabs = true;
    {
        const QSignalBlocker blocker(m_tabBar);
        while (m_tabBar->count() > 0) {
            m_tabBar->removeTab(0);
        }

        int targetIndex = 0;
        for (std::size_t i = 0; i < m_visibleTabs.size(); ++i) {
            const auto& tab = m_visibleTabs[i];
            QString label = QCoreApplication::translate("CommandRibbon", tab.title.c_str());
            label.replace(QLatin1Char('&'), QStringLiteral("&&"));
            const int idx = m_tabBar->addTab(label);
            m_tabBar->setTabData(idx, QString::fromStdString(tab.id));
            if (tab.id == targetTabId) {
                targetIndex = idx;
            }
        }

        if (m_tabBar->count() > 0) {
            m_tabBar->setCurrentIndex(targetIndex);
        }
    }
    m_updatingTabs = false;

    buildActiveTabContent();
}

bool CommandRibbonBar::selectTabById(const std::string& tabId)
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString().toStdString() == tabId) {
            m_tabBar->setCurrentIndex(i);
            return true;
        }
    }
    return false;
}

void CommandRibbonBar::setCollapsed(bool collapsed)
{
    const bool wantVisible = !collapsed && !m_visibleTabs.empty();
    if (m_collapsed == collapsed && m_contentScroll->isVisible() == wantVisible) {
        return;
    }
    m_collapsed = collapsed;
    m_contentScroll->setVisible(wantVisible);
    {
        const QSignalBlocker blocker(m_collapseButton);
        m_collapseButton->setChecked(m_collapsed);
    }
    m_collapseButton->setText(m_collapsed ? QStringLiteral("▾") : QStringLiteral("▴"));
    m_collapseButton->setToolTip(
        m_collapsed ? tr("Expand Command Ribbon") : tr("Collapse Command Ribbon")
    );
    Q_EMIT collapsedChanged(m_collapsed);
}

bool CommandRibbonBar::isCollapsed() const
{
    return m_collapsed;
}

std::vector<std::string> CommandRibbonBar::visibleTabIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_visibleTabs.size());
    for (const auto& tab : m_visibleTabs) {
        ids.push_back(tab.id);
    }
    return ids;
}

std::string CommandRibbonBar::activeTabId() const
{
    const int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabBar->count()) {
        return {};
    }
    return m_tabBar->tabData(idx).toString().toStdString();
}

void CommandRibbonBar::onTabBarCurrentChanged(int index)
{
    if (m_updatingTabs || index < 0 || static_cast<std::size_t>(index) >= m_visibleTabs.size()) {
        return;
    }

    const RibbonTabDef selectedTab = m_visibleTabs[static_cast<std::size_t>(index)];

    // Lazy-activate the owning workbench when clicking a cross-workbench tab
    // (e.g., clicking "Sketch" while in PartDesign) without losing tab selection.
    if (Application::Instance && !selectedTab.workbenchName.empty()
        && selectedTab.workbenchName != m_currentWorkbench) {
        const QStringList available = Application::Instance->workbenches();
        const QString targetWb = QString::fromStdString(selectedTab.workbenchName);
        if (available.contains(targetWb)) {
            Application::Instance->activateWorkbench(selectedTab.workbenchName.c_str());
            selectTabById(selectedTab.id);
        }
    }

    buildActiveTabContent();
    Q_EMIT tabActivated(QString::fromStdString(selectedTab.id));
}

void CommandRibbonBar::onCollapseToggled(bool checked)
{
    setCollapsed(checked);
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow"
    );
    hGrp->SetBool("CommandRibbonCollapsed", checked);
}

void CommandRibbonBar::clearStripLayout()
{
    while (QLayoutItem* item = m_stripLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

void CommandRibbonBar::buildActiveTabContent()
{
    clearStripLayout();

    const bool hasTabs = !m_visibleTabs.empty();
    m_collapseButton->setVisible(hasTabs);
    m_contentScroll->setVisible(hasTabs && !m_collapsed);

    const int idx = m_tabBar->currentIndex();
    if (idx < 0 || static_cast<std::size_t>(idx) >= m_visibleTabs.size()) {
        m_stripLayout->addStretch(1);
        return;
    }

    const auto& tab = m_visibleTabs[static_cast<std::size_t>(idx)];
    bool addedAnyGroup = false;

    for (const auto& group : tab.groups) {
        QWidget* groupWidget = createGroupWidget(group);
        if (!groupWidget) {
            continue;
        }
        if (addedAnyGroup) {
            auto* sep = new QFrame(m_stripContainer);
            sep->setObjectName(QStringLiteral("commandRibbonSeparator"));
            sep->setFrameShape(QFrame::VLine);
            sep->setFrameShadow(QFrame::Plain);
            m_stripLayout->addWidget(sep);
        }
        m_stripLayout->addWidget(groupWidget);
        addedAnyGroup = true;
    }

    m_stripLayout->addStretch(1);
}

QWidget* CommandRibbonBar::createGroupWidget(const RibbonGroupDef& group)
{
    if (!Application::Instance) {
        return nullptr;
    }

    CommandManager& cmdMgr = Application::Instance->commandManager();

    auto* frame = new QFrame(m_stripContainer);
    frame->setObjectName(QStringLiteral("commandRibbonGroup"));
    auto* outerLayout = new QVBoxLayout(frame);
    outerLayout->setContentsMargins(4, 1, 4, 1);
    outerLayout->setSpacing(1);

    auto* controlsRow = new QHBoxLayout();
    controlsRow->setContentsMargins(0, 0, 0, 0);
    controlsRow->setSpacing(2);

    QToolBar* largeBar = nullptr;
    std::vector<std::string> compactBatch;
    int addedCommands = 0;

    auto flushCompactColumn = [&]() {
        if (compactBatch.empty()) {
            return;
        }
        auto* colWidget = new QWidget(frame);
        auto* colLayout = new QVBoxLayout(colWidget);
        colLayout->setContentsMargins(0, 0, 0, 0);
        colLayout->setSpacing(0);

        for (const auto& cmdName : compactBatch) {
            auto* rowBar = new QToolBar(colWidget);
            rowBar->setObjectName(QStringLiteral("*ribbonCompactRow"));
            rowBar->setMovable(false);
            rowBar->setFloatable(false);
            rowBar->setIconSize(QSize(kCompactRibbonIconPx, kCompactRibbonIconPx));
            rowBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            if (cmdMgr.addTo(cmdName.c_str(), rowBar)) {
                colLayout->addWidget(rowBar);
                ++addedCommands;
            }
            else {
                rowBar->deleteLater();
            }
        }
        colLayout->addStretch(1);
        controlsRow->addWidget(colWidget);
        compactBatch.clear();
    };

    for (const auto& entry : group.commands) {
        if (!cmdMgr.getCommandByName(entry.commandName.c_str())) {
            continue;
        }
        if (entry.size == RibbonButtonSize::Large) {
            flushCompactColumn();
            if (!largeBar) {
                largeBar = new QToolBar(frame);
                largeBar->setObjectName(QStringLiteral("*ribbonLargeBar"));
                largeBar->setMovable(false);
                largeBar->setFloatable(false);
                largeBar->setIconSize(QSize(kLargeRibbonIconPx, kLargeRibbonIconPx));
                largeBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                controlsRow->addWidget(largeBar);
            }
            if (cmdMgr.addTo(entry.commandName.c_str(), largeBar)) {
                ++addedCommands;
            }
        }
        else {
            largeBar = nullptr;
            compactBatch.push_back(entry.commandName);
            if (static_cast<int>(compactBatch.size()) >= kCompactRowsPerColumn) {
                flushCompactColumn();
            }
        }
    }
    flushCompactColumn();

    if (addedCommands == 0) {
        frame->deleteLater();
        return nullptr;
    }

    outerLayout->addLayout(controlsRow, 1);

    auto* titleLabel = new QLabel(
        QCoreApplication::translate("CommandRibbon", group.title.c_str()),
        frame
    );
    titleLabel->setObjectName(QStringLiteral("commandRibbonGroupTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    outerLayout->addWidget(titleLabel, 0);

    return frame;
}

void CommandRibbonBar::rebuildMoreWorkbenchesMenu()
{
    m_moreMenu->clear();
    if (!Application::Instance) {
        return;
    }

    QStringList workbenches = Application::Instance->workbenches();
    workbenches.sort(Qt::CaseInsensitive);

    for (const QString& wbName : workbenches) {
        if (wbName == QStringLiteral("NoneWorkbench")) {
            continue;
        }
        QString label = Application::Instance->workbenchMenuText(wbName);
        if (label.isEmpty()) {
            label = wbName;
        }
        QAction* act = m_moreMenu->addAction(QIcon(Application::Instance->workbenchIcon(wbName)), label);
        act->setCheckable(true);
        act->setChecked(wbName.toStdString() == m_currentWorkbench);
        connect(act, &QAction::triggered, this, [wbName]() {
            if (Application::Instance) {
                Application::Instance->activateWorkbench(wbName.toLatin1().constData());
            }
        });
    }
}

void CommandRibbonBar::retranslateUi()
{
    m_moreButton->setText(tr("+ More"));
    m_moreButton->setToolTip(tr("Switch to another FreeCAD workbench"));
    m_collapseButton->setText(m_collapsed ? QStringLiteral("▾") : QStringLiteral("▴"));
    m_collapseButton->setToolTip(
        m_collapsed ? tr("Expand Command Ribbon") : tr("Collapse Command Ribbon")
    );

    // Retranslate tab labels and group titles
    if (!m_visibleTabs.empty()) {
        const QSignalBlocker blocker(m_tabBar);
        for (int i = 0; i < m_tabBar->count() && static_cast<std::size_t>(i) < m_visibleTabs.size();
             ++i) {
            m_tabBar->setTabText(
                i,
                QCoreApplication::translate(
                    "CommandRibbon",
                    m_visibleTabs[static_cast<std::size_t>(i)].title.c_str()
                )
            );
        }
        buildActiveTabContent();
    }
}

void CommandRibbonBar::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

}  // namespace Gui
