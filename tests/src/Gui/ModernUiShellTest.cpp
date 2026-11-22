// SPDX-License-Identifier: LGPL-2.1-or-later

#include <QAction>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
#include <QWidget>

#include <Gui/CommandRibbon/CommandRibbonBar.h>
#include <Gui/CommandRibbon/CommandRibbonRegistry.h>
#include <Gui/ViewportOverlays/ConfirmationCorner.h>
#include <Gui/ViewportOverlays/ContextMiniToolbar.h>
#include <Gui/ViewportOverlays/HeadsUpViewBar.h>
#include <Gui/ViewportOverlays/ShortcutBarPopup.h>

#include <src/App/InitApplication.h>

// NOLINTBEGIN(readability-magic-numbers)

class testModernUiShell: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        tests::initApplication();
    }

    void testCommandRibbonBarContextAndCollapse()
    {
        Gui::CommandRibbonBar ribbon;
        ribbon.refreshContext("PartDesignWorkbench", false, false);

        const auto tabs = ribbon.visibleTabIds();
        QCOMPARE(static_cast<int>(tabs.size()), 4);
        QCOMPARE(ribbon.activeTabId(), std::string("part_features"));

        QVERIFY(ribbon.selectTabById("evaluate_tools"));
        QCOMPARE(ribbon.activeTabId(), std::string("evaluate_tools"));

        // Auto-switch to sketch tab during sketch edit
        ribbon.refreshContext("PartDesignWorkbench", true, false);
        QCOMPARE(ribbon.activeTabId(), std::string("sketch_tools"));

        // Collapse / expand state
        QSignalSpy collapseSpy(&ribbon, &Gui::CommandRibbonBar::collapsedChanged);
        ribbon.setCollapsed(true);
        QVERIFY(ribbon.isCollapsed());
        ribbon.setCollapsed(false);
        QVERIFY(!ribbon.isCollapsed());
        QVERIFY(collapseSpy.count() >= 1);
    }

    void testConfirmationCornerButtonsAndSignals()
    {
        QWidget host;
        host.resize(800, 600);
        host.show();

        Gui::ConfirmationCorner corner(&host);
        QVERIFY(corner.isHidden());

        QSignalSpy acceptSpy(&corner, &Gui::ConfirmationCorner::accepted);
        QSignalSpy rejectSpy(&corner, &Gui::ConfirmationCorner::rejected);

        corner.configureButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVERIFY(!corner.isHidden());
        QVERIFY(corner.isAcceptVisible());
        QVERIFY(corner.isRejectVisible());

        corner.updateGeometryInRect(host.rect(), false);
        const int yNoCube = corner.y();
        corner.updateGeometryInRect(host.rect(), true);
        const int yWithCube = corner.y();
        QVERIFY(yWithCube > yNoCube);

        auto* acceptBtn = corner.findChild<QToolButton*>(QStringLiteral("confirmationCornerAccept"));
        auto* rejectBtn = corner.findChild<QToolButton*>(QStringLiteral("confirmationCornerReject"));
        QVERIFY(acceptBtn != nullptr);
        QVERIFY(rejectBtn != nullptr);

        acceptBtn->click();
        rejectBtn->click();
        QCOMPARE(acceptSpy.count(), 1);
        QCOMPARE(rejectSpy.count(), 1);

        // Close-only mode (like Sketcher edit) hides reject button
        corner.configureButtons(QDialogButtonBox::Close);
        QVERIFY(corner.isAcceptVisible());
        QVERIFY(!corner.isRejectVisible());

        corner.setActiveDialog(nullptr);
        QVERIFY(corner.isHidden());
    }

    void testHeadsUpViewBarModesAndBadge()
    {
        QWidget host;
        host.resize(1000, 700);
        host.show();

        Gui::HeadsUpViewBar bar(&host);
        QCOMPARE(bar.mode(), Gui::HeadsUpViewBar::Mode::Hidden);
        QVERIFY(bar.isHidden());

        auto* badge = new QLabel(QStringLiteral("Update Drawing"));
        bar.setBadgeWidget(badge);
        bar.setMode(Gui::HeadsUpViewBar::Mode::Drawing2D);
        QCOMPARE(bar.mode(), Gui::HeadsUpViewBar::Mode::Drawing2D);
        QVERIFY(!bar.isHidden());

        bar.updateGeometryInRect(host.rect());
        QVERIFY(bar.x() > 0);
        QVERIFY(bar.x() < host.width());

        bar.setMode(Gui::HeadsUpViewBar::Mode::Hidden);
        QVERIFY(bar.isHidden());
    }

    void testContextMiniToolbarShowAndAutoDismiss()
    {
        Gui::ContextMiniToolbar mini;
        QAction act1(QStringLiteral("Fillet"));
        QAction act2(QStringLiteral("Chamfer"));

        QVERIFY(mini.showForActions({&act1, &act2}, QPoint(300, 300)));
        QCOMPARE(mini.actionCount(), 2);
        QVERIFY(mini.isVisible());

        act1.trigger();
        QVERIFY(!mini.isVisible());

        QVERIFY(!mini.showForActions({}, QPoint(300, 300)));
        QVERIFY(!mini.isVisible());
    }

    void testShortcutBarPopupGridAndEscapeDismiss()
    {
        Gui::ShortcutBarPopup popup;
        QAction a1(QStringLiteral("Line"));
        QAction a2(QStringLiteral("Circle"));
        QAction a3(QStringLiteral("Arc"));
        QAction a4(QStringLiteral("Rect"));
        QAction a5(QStringLiteral("Dim"));

        QVERIFY(popup.showForActions(
            QStringLiteral("Sketch Shortcuts"),
            {&a1, &a2, &a3, &a4, &a5},
            QPoint(400, 300)
        ));
        QCOMPARE(popup.actionCount(), 5);
        QCOMPARE(popup.headerText(), QStringLiteral("Sketch Shortcuts"));
        QVERIFY(popup.isVisible());

        QTest::keyClick(&popup, Qt::Key_Escape);
        QVERIFY(!popup.isVisible());
    }
};

// NOLINTEND(readability-magic-numbers)

QTEST_MAIN(testModernUiShell)

#include "ModernUiShellTest.moc"
