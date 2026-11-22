// SPDX-License-Identifier: LGPL-2.1-or-later

#include <cmath>
#include <filesystem>

#include <gtest/gtest.h>

#include <App/Application.h>
#include <App/Document.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <src/App/InitApplication.h>

namespace
{

class ProjectionMathTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        tests::initApplication();
        m_docName = App::GetApplication().getUniqueDocumentName("drawing_proj_test");
        m_doc = App::GetApplication().newDocument(m_docName.c_str(), "testUser");
    }

    void TearDown() override
    {
        App::GetApplication().closeDocument(m_docName.c_str());
    }

    std::string m_docName;
    App::Document* m_doc {nullptr};
};

TEST_F(ProjectionMathTest, classifiesAllEightCompassSectors)
{
    using Drawing::ProjectionDirection8;
    using Drawing::ProjectionMath;

    EXPECT_EQ(ProjectionMath::classifyDirection8(10.0, 0.0), ProjectionDirection8::East);
    EXPECT_EQ(ProjectionMath::classifyDirection8(10.0, 10.0), ProjectionDirection8::NorthEast);
    EXPECT_EQ(ProjectionMath::classifyDirection8(0.0, 10.0), ProjectionDirection8::North);
    EXPECT_EQ(ProjectionMath::classifyDirection8(-10.0, 10.0), ProjectionDirection8::NorthWest);
    EXPECT_EQ(ProjectionMath::classifyDirection8(-10.0, 0.0), ProjectionDirection8::West);
    EXPECT_EQ(ProjectionMath::classifyDirection8(-10.0, -10.0), ProjectionDirection8::SouthWest);
    EXPECT_EQ(ProjectionMath::classifyDirection8(0.0, -10.0), ProjectionDirection8::South);
    EXPECT_EQ(ProjectionMath::classifyDirection8(10.0, -10.0), ProjectionDirection8::SouthEast);
}

TEST_F(ProjectionMathTest, computesThirdAndFirstAngleOrthogonalProjections)
{
    using Drawing::AlignmentKind;
    using Drawing::ProjectionDirection8;
    using Drawing::ProjectionMath;

    const auto front = ProjectionMath::standardOrientation(Drawing::StandardOrientation::Front);

    // Third angle (ASME): dragging East -> Right view (+X), dragging North -> Top view (+Z)
    const auto right3 = ProjectionMath::computeProjectedView(
        front.direction,
        front.xDirection,
        ProjectionDirection8::East,
        true
    );
    EXPECT_NEAR(right3.direction.x, 1.0, 1e-6);
    EXPECT_NEAR(right3.direction.y, 0.0, 1e-6);
    EXPECT_NEAR(right3.direction.z, 0.0, 1e-6);
    EXPECT_NEAR(right3.direction * right3.xDirection, 0.0, 1e-6);
    EXPECT_EQ(right3.alignment, AlignmentKind::Horizontal);
    EXPECT_EQ(right3.label, "Right");

    const auto top3 = ProjectionMath::computeProjectedView(
        front.direction,
        front.xDirection,
        ProjectionDirection8::North,
        true
    );
    EXPECT_NEAR(top3.direction.z, 1.0, 1e-6);
    EXPECT_NEAR(top3.direction * top3.xDirection, 0.0, 1e-6);
    EXPECT_EQ(top3.alignment, AlignmentKind::Vertical);
    EXPECT_EQ(top3.label, "Top");

    // First angle (ISO): dragging East -> Left view (-X), dragging South -> Top view (+Z)
    const auto east1 = ProjectionMath::computeProjectedView(
        front.direction,
        front.xDirection,
        ProjectionDirection8::East,
        false
    );
    EXPECT_NEAR(east1.direction.x, -1.0, 1e-6);
    EXPECT_EQ(east1.alignment, AlignmentKind::Horizontal);
    EXPECT_EQ(east1.label, "Left");

    const auto south1 = ProjectionMath::computeProjectedView(
        front.direction,
        front.xDirection,
        ProjectionDirection8::South,
        false
    );
    EXPECT_NEAR(south1.direction.z, 1.0, 1e-6);
    EXPECT_EQ(south1.alignment, AlignmentKind::Vertical);
    EXPECT_EQ(south1.label, "Top");

    // Diagonal isometric view produces orthonormal basis and Vector alignment
    const auto iso = ProjectionMath::computeProjectedView(
        front.direction,
        front.xDirection,
        ProjectionDirection8::NorthEast,
        true
    );
    EXPECT_NEAR(iso.direction.Length(), 1.0, 1e-6);
    EXPECT_NEAR(iso.xDirection.Length(), 1.0, 1e-6);
    EXPECT_NEAR(iso.direction * iso.xDirection, 0.0, 1e-6);
    EXPECT_EQ(iso.alignment, AlignmentKind::Vector);
}

TEST_F(ProjectionMathTest, attachesBreaksRestoresAndRoundTripsDynamicAlignmentInFCStd)
{
    using Drawing::AlignmentKind;
    using Drawing::ProjectionMath;

    auto* page = m_doc->addObject<TechDraw::DrawPage>("Page");
    auto* parent = m_doc->addObject<TechDraw::DrawViewPart>("FrontView");
    auto* child = m_doc->addObject<TechDraw::DrawViewPart>("RightView");
    ASSERT_NE(page, nullptr);
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);

    parent->X.setValue(100.0);
    parent->Y.setValue(150.0);
    child->X.setValue(220.0);
    child->Y.setValue(150.0);
    page->addView(parent, false);
    page->addView(child, false);

    ProjectionMath::attachViewAlignment(
        child,
        parent,
        AlignmentKind::Horizontal,
        Base::Vector3d(1.0, 0.0, 0.0)
    );
    EXPECT_EQ(ProjectionMath::getViewAlignmentMode(child), AlignmentKind::Horizontal);
    EXPECT_EQ(ProjectionMath::getParentAlignedView(child), parent);

    // Break alignment, move freely off-axis, then restore -> snaps Y back to parent.Y!
    EXPECT_TRUE(ProjectionMath::breakViewAlignment(child));
    EXPECT_EQ(ProjectionMath::getViewAlignmentMode(child), AlignmentKind::Broken);

    child->Y.setValue(210.0);
    EXPECT_TRUE(ProjectionMath::restoreViewAlignment(child));
    EXPECT_EQ(ProjectionMath::getViewAlignmentMode(child), AlignmentKind::Horizontal);
    EXPECT_NEAR(child->Y.getValue(), 150.0, 1e-6);
    EXPECT_NEAR(child->X.getValue(), 220.0, 1e-6);

    // Verify 100% .FCStd save & reload round-trip preserves dynamic alignment properties!
    const auto tmpPath
        = std::filesystem::temp_directory_path() / "freecad_drawing_alignment_roundtrip.FCStd";
    std::error_code ec;
    std::filesystem::remove(tmpPath, ec);
    ASSERT_TRUE(m_doc->saveAs(tmpPath.string().c_str()));

    App::Document* reloaded = App::GetApplication().openDocument(tmpPath.string().c_str());
    ASSERT_NE(reloaded, nullptr);
    auto* reloadedParent = dynamic_cast<TechDraw::DrawViewPart*>(reloaded->getObject("FrontView"));
    auto* reloadedChild = dynamic_cast<TechDraw::DrawViewPart*>(reloaded->getObject("RightView"));
    ASSERT_NE(reloadedParent, nullptr);
    ASSERT_NE(reloadedChild, nullptr);

    EXPECT_EQ(ProjectionMath::getViewAlignmentMode(reloadedChild), AlignmentKind::Horizontal);
    EXPECT_EQ(ProjectionMath::getParentAlignedView(reloadedChild), reloadedParent);

    App::GetApplication().closeDocument(reloaded->getName());
    std::filesystem::remove(tmpPath, ec);
}

TEST_F(ProjectionMathTest, selectsUnitMatchedSheetPresetsAndStandardScales)
{
    using Drawing::ProjectionMath;

    const auto metricDefault = ProjectionMath::defaultSheetPresetForUnits(false);
    EXPECT_EQ(metricDefault.standard, "ISO5457");
    EXPECT_FALSE(metricDefault.thirdAngle);

    const auto imperialDefault = ProjectionMath::defaultSheetPresetForUnits(true);
    EXPECT_EQ(imperialDefault.standard, "ASME Y14.1");
    EXPECT_TRUE(imperialDefault.thirdAngle);

    EXPECT_NEAR(ProjectionMath::suggestStandardScale(100.0, 80.0, 420.0, 297.0), 1.0, 1e-6);
    EXPECT_NEAR(ProjectionMath::suggestStandardScale(400.0, 300.0, 420.0, 297.0), 0.25, 1e-6);
}

}  // namespace
