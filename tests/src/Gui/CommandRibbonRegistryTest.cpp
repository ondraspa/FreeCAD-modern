// SPDX-License-Identifier: LGPL-2.1-or-later

#include <gtest/gtest.h>

#include <Gui/CommandRibbon/CommandRibbonRegistry.h>

namespace
{

class CommandRibbonRegistryTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto& reg = Gui::CommandRibbonRegistry::instance();
        reg.clear();
        reg.registerDefaultContexts();
    }

    void TearDown() override
    {
        auto& reg = Gui::CommandRibbonRegistry::instance();
        reg.clear();
        reg.registerDefaultContexts();
    }
};

TEST_F(CommandRibbonRegistryTest, resolvesPartDesignContextTabs)
{
    const auto& reg = Gui::CommandRibbonRegistry::instance();

    EXPECT_TRUE(reg.hasContext("PartDesignWorkbench"));
    const auto tabs = reg.resolveTabsForContext("PartDesignWorkbench");
    ASSERT_EQ(tabs.size(), 4U);
    EXPECT_EQ(tabs[0].id, "part_features");
    EXPECT_EQ(tabs[1].id, "sketch_tools");
    EXPECT_EQ(tabs[2].id, "reference_geometry");
    EXPECT_EQ(tabs[3].id, "evaluate_tools");
    EXPECT_EQ(reg.resolveActiveTabId("PartDesignWorkbench"), "part_features");
}

TEST_F(CommandRibbonRegistryTest, autoSwitchesToSketchTabDuringSketchEdit)
{
    const auto& reg = Gui::CommandRibbonRegistry::instance();

    EXPECT_EQ(reg.resolveActiveTabId("PartDesignWorkbench", true, false), "sketch_tools");

    // Even from Assembly (which doesn't normally include Sketch), sketch edit injects it.
    const auto asmTabs = reg.resolveTabsForContext("AssemblyWorkbench", true, false);
    ASSERT_FALSE(asmTabs.empty());
    EXPECT_EQ(asmTabs.front().id, "sketch_tools");
    EXPECT_EQ(reg.resolveActiveTabId("AssemblyWorkbench", true, false), "sketch_tools");
}

TEST_F(CommandRibbonRegistryTest, autoSwitchesToDrawingContextForDrawingViews)
{
    const auto& reg = Gui::CommandRibbonRegistry::instance();

    const auto tabs = reg.resolveTabsForContext("PartDesignWorkbench", false, true);
    ASSERT_GE(tabs.size(), 2U);
    EXPECT_EQ(tabs[0].id, "drawing_views");
    EXPECT_EQ(tabs[1].id, "drawing_annotations");
    EXPECT_EQ(reg.resolveActiveTabId("PartDesignWorkbench", false, true), "drawing_views");
}

TEST_F(CommandRibbonRegistryTest, allowsModuleOverrideForDrawingWorkbenchContext)
{
    auto& reg = Gui::CommandRibbonRegistry::instance();

    reg.registerTab(Gui::RibbonTabDef {
        .id = "modern_drawing_views",
        .title = "View Layout",
        .workbenchName = "DrawingWorkbench",
        .groups = {},
    });
    reg.registerContext(Gui::RibbonContextDef {
        .id = "modern_drawing_context",
        .matchWorkbench = "DrawingWorkbench",
        .tabIds = {"modern_drawing_views", "evaluate_tools"},
        .defaultTabId = "modern_drawing_views",
    });

    EXPECT_TRUE(reg.hasContext("DrawingWorkbench"));
    const auto tabs = reg.resolveTabsForContext("PartDesignWorkbench", false, true);
    ASSERT_EQ(tabs.size(), 2U);
    EXPECT_EQ(tabs[0].id, "modern_drawing_views");
    EXPECT_EQ(reg.resolveActiveTabId("PartDesignWorkbench", false, true), "modern_drawing_views");
}

TEST_F(CommandRibbonRegistryTest, returnsEmptyTabsForUnregisteredWorkbench)
{
    const auto& reg = Gui::CommandRibbonRegistry::instance();

    EXPECT_FALSE(reg.hasContext("CustomAddonWorkbench"));
    EXPECT_TRUE(reg.resolveTabsForContext("CustomAddonWorkbench").empty());
    EXPECT_TRUE(reg.resolveActiveTabId("CustomAddonWorkbench").empty());
}

}  // namespace
