// SPDX-License-Identifier: LGPL-2.1-or-later

#include <gtest/gtest.h>

#include <App/Application.h>
#include <Base/Parameter.h>
#include <Gui/ViewportOverlays/ContextActionRegistry.h>
#include <src/App/InitApplication.h>

namespace
{

class ContextActionRegistryTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        tests::initApplication();
        auto& reg = Gui::ContextActionRegistry::instance();
        reg.clear();
        reg.registerDefaultRules();
    }

    void TearDown() override
    {
        auto hGrp = App::GetApplication().GetParameterGroupByPath(
            "User parameter:BaseApp/Preferences/MainWindow/ShortcutBar"
        );
        if (hGrp.isValid()) {
            hGrp->RemoveASCII("Part");
            hGrp->RemoveASCII("CustomCtx");
        }

        auto& reg = Gui::ContextActionRegistry::instance();
        reg.clear();
        reg.registerDefaultRules();
    }
};

TEST_F(ContextActionRegistryTest, mapsWorkbenchAndEditFlagsToShortcutContext)
{
    EXPECT_EQ(Gui::ContextActionRegistry::shortcutContextForState("PartDesignWorkbench"), "Part");
    EXPECT_EQ(
        Gui::ContextActionRegistry::shortcutContextForState("PartDesignWorkbench", true, false),
        "Sketch"
    );
    EXPECT_EQ(
        Gui::ContextActionRegistry::shortcutContextForState("PartDesignWorkbench", false, true),
        "Drawing"
    );
    EXPECT_EQ(Gui::ContextActionRegistry::shortcutContextForState("AssemblyWorkbench"), "Assembly");
    EXPECT_EQ(Gui::ContextActionRegistry::shortcutContextForState("DrawingWorkbench"), "Drawing");
    EXPECT_EQ(Gui::ContextActionRegistry::shortcutContextForState("UnknownWorkbench"), "Default");
}

TEST_F(ContextActionRegistryTest, returnsDefaultShortcutBarAndFallsBackForUnknownContext)
{
    const auto& reg = Gui::ContextActionRegistry::instance();

    const auto partCmds = reg.resolveShortcutBarCommands("Part");
    ASSERT_FALSE(partCmds.empty());
    EXPECT_EQ(partCmds.front(), "PartDesign_NewSketch");

    const auto sketchCmds = reg.resolveShortcutBarCommands("Sketch");
    ASSERT_FALSE(sketchCmds.empty());
    EXPECT_EQ(sketchCmds.back(), "Sketcher_LeaveSketch");

    const auto fallbackCmds = reg.resolveShortcutBarCommands("NonExistentContext");
    const auto defaultCmds = reg.resolveShortcutBarCommands("Default");
    EXPECT_EQ(fallbackCmds, defaultCmds);
}

TEST_F(ContextActionRegistryTest, honorsUserParameterOverrideForShortcutBar)
{
    auto hGrp = App::GetApplication().GetParameterGroupByPath(
        "User parameter:BaseApp/Preferences/MainWindow/ShortcutBar"
    );
    hGrp->SetASCII("Part", "Std_Measure, PartDesign_Pad , PartDesign_Fillet");

    const auto& reg = Gui::ContextActionRegistry::instance();
    const auto cmds = reg.resolveShortcutBarCommands("Part");
    ASSERT_EQ(cmds.size(), 3U);
    EXPECT_EQ(cmds[0], "Std_Measure");
    EXPECT_EQ(cmds[1], "PartDesign_Pad");
    EXPECT_EQ(cmds[2], "PartDesign_Fillet");
}

TEST_F(ContextActionRegistryTest, supportsRegisterAndUnregisterShortcutBarContext)
{
    auto& reg = Gui::ContextActionRegistry::instance();
    reg.registerShortcutBar(Gui::ShortcutBarContext {
        .contextId = "CustomCtx",
        .commands = {"Std_New", "Std_Save"},
    });

    const auto cmds = reg.resolveShortcutBarCommands("CustomCtx");
    ASSERT_EQ(cmds.size(), 2U);
    EXPECT_EQ(cmds[0], "Std_New");
    EXPECT_EQ(cmds[1], "Std_Save");

    reg.unregisterShortcutBar("CustomCtx");
    EXPECT_EQ(reg.resolveShortcutBarCommands("CustomCtx"), reg.resolveShortcutBarCommands("Default"));
}

}  // namespace
