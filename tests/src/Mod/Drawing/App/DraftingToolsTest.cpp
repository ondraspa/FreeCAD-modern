// SPDX-License-Identifier: LGPL-2.1-or-later

#include <cmath>
#include <filesystem>

#include <gtest/gtest.h>

#include <BRepPrimAPI_MakeBox.hxx>

#include <App/Application.h>
#include <App/Document.h>
#include <App/PropertyStandard.h>
#include <Mod/Drawing/App/DraftingTools.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/Part/App/PartFeature.h>
#include <Mod/TechDraw/App/DrawBrokenView.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawViewBalloon.h>
#include <Mod/TechDraw/App/DrawViewClip.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/DrawViewSpreadsheet.h>
#include <Mod/TechDraw/App/DrawViewSymbol.h>
#include <src/App/InitApplication.h>

#include <Base/Interpreter.h>

#include <Mod/TechDraw/App/DrawSVGTemplate.h>

namespace
{

class DraftingToolsTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        tests::initApplication();
        Base::Interpreter().loadModule("Part");
        Base::Interpreter().loadModule("Spreadsheet");
        Base::Interpreter().loadModule("TechDraw");
        Base::Interpreter().loadModule("Drawing");
        m_docName = App::GetApplication().getUniqueDocumentName("drawing_milestone3_test");
        m_doc = App::GetApplication().newDocument(m_docName.c_str(), "testUser");
    }

    void TearDown() override
    {
        App::GetApplication().closeDocument(m_docName.c_str());
    }

    TechDraw::DrawPage* createTestPageWithTemplate()
    {
        auto* page = m_doc->addObject<TechDraw::DrawPage>("Page");
        auto* tmpl = m_doc->addObject<TechDraw::DrawSVGTemplate>("Template");
        if (page && tmpl) {
            tmpl->Width.setValue(420.0);
            tmpl->Height.setValue(297.0);
            page->Template.setValue(tmpl);
        }
        return page;
    }

    std::string m_docName;
    App::Document* m_doc {nullptr};
};

TEST_F(DraftingToolsTest, computesAuxiliaryViewFromSlanted2DEdge)
{
    using Drawing::AlignmentKind;
    using Drawing::ProjectionMath;

    const auto front = ProjectionMath::standardOrientation(Drawing::StandardOrientation::Front);
    // 45-degree slanted edge in 2D page space (+X, +Y)
    const Base::Vector3d edgeTang(1.0, 1.0, 0.0);
    // Unfolding up-left (-1, +1) in Third Angle
    const Base::Vector3d cursorOffset(-20.0, 20.0, 0.0);

    const auto aux3 = ProjectionMath::computeAuxiliaryView(
        front.direction,
        front.xDirection,
        edgeTang,
        cursorOffset,
        true,
        "A"
    );

    EXPECT_EQ(aux3.alignment, AlignmentKind::Vector);
    EXPECT_EQ(aux3.label, "Auxiliary A");
    EXPECT_NEAR(aux3.direction.Length(), 1.0, 1e-6);
    EXPECT_NEAR(aux3.xDirection.Length(), 1.0, 1e-6);
    EXPECT_NEAR(aux3.direction * aux3.xDirection, 0.0, 1e-6);
    // Alignment vector should be perpendicular to edgeTang (dot == 0) and point into the (-X, +Y) half-plane
    EXPECT_NEAR(
        aux3.alignmentVector.x * edgeTang.x + aux3.alignmentVector.y * edgeTang.y,
        0.0,
        1e-6
    );
    EXPECT_LT(aux3.alignmentVector.x, 0.0);
    EXPECT_GT(aux3.alignmentVector.y, 0.0);

    // First Angle reverses viewing direction along the fold normal
    const auto aux1 = ProjectionMath::computeAuxiliaryView(
        front.direction,
        front.xDirection,
        edgeTang,
        cursorOffset,
        false,
        "B"
    );
    EXPECT_NEAR(aux1.direction * aux3.direction, -1.0, 1e-6);
    EXPECT_EQ(aux1.label, "Auxiliary B");
}

TEST_F(DraftingToolsTest, creates2DBrokenViewAndInPlaceCropViewWithout3DSketches)
{
    using Drawing::DraftingTools;

    auto* page = createTestPageWithTemplate();
    auto* boxFeat = m_doc->addObject<Part::Feature>("LongBar");
    ASSERT_NE(page, nullptr);
    ASSERT_NE(boxFeat, nullptr);
    boxFeat->Shape.setValue(BRepPrimAPI_MakeBox(120.0, 20.0, 20.0).Shape());

    auto* baseView = m_doc->addObject<TechDraw::DrawViewPart>("BaseView");
    ASSERT_NE(baseView, nullptr);
    baseView->Source.setValues({boxFeat});
    baseView->X.setValue(150.0);
    baseView->Y.setValue(120.0);
    page->addView(baseView, false);

    // 1. Create a 2D Break View removing [-25mm, +25mm] horizontally (vertical break lines)
    TechDraw::DrawBrokenView* broken
        = DraftingTools::createBrokenViewFrom2DCut(page, baseView, -25.0, 25.0, true, 8.0);
    ASSERT_NE(broken, nullptr);
    EXPECT_EQ(broken->Breaks.getValues().size(), 1U);
    EXPECT_NEAR(broken->Gap.getValue(), 8.0, 1e-6);
    auto* orientProp
        = dynamic_cast<App::PropertyString*>(broken->getPropertyByName("BreakOrientation2D"));
    ASSERT_NE(orientProp, nullptr);
    EXPECT_EQ(orientProp->getStrValue(), "Vertical");

    // 2. Create a second view and crop it in place with a DrawViewClip
    auto* cropTarget = m_doc->addObject<TechDraw::DrawViewPart>("CropTarget");
    ASSERT_NE(cropTarget, nullptr);
    cropTarget->Source.setValues({boxFeat});
    cropTarget->X.setValue(240.0);
    cropTarget->Y.setValue(120.0);
    page->addView(cropTarget, false);

    TechDraw::DrawViewClip* clip
        = DraftingTools::cropViewInPlace(page, cropTarget, 245.0, 118.0, 50.0, 35.0, false);
    ASSERT_NE(clip, nullptr);
    EXPECT_NEAR(clip->Width.getValue(), 50.0, 1e-6);
    EXPECT_NEAR(clip->Height.getValue(), 35.0, 1e-6);
    EXPECT_TRUE(clip->isViewInClip(cropTarget));
}

TEST_F(DraftingToolsTest, buildsAndSerializesDatumAndGdtFeatureControlFrames)
{
    using Drawing::DatumSymbolSpec;
    using Drawing::DraftingTools;
    using Drawing::FeatureControlFrameSpec;
    using Drawing::GdtCharacteristic;
    using Drawing::MaterialConditionModifier;

    auto* page = createTestPageWithTemplate();
    ASSERT_NE(page, nullptr);

    DatumSymbolSpec datumSpec;
    datumSpec.letter = "A";
    datumSpec.filledTriangle = true;
    auto* datumSym = DraftingTools::createDatumSymbolOnPage(page, datumSpec, 80.0, 90.0);
    ASSERT_NE(datumSym, nullptr);
    EXPECT_NE(datumSym->Symbol.getStrValue().find(">A</text>"), std::string::npos);

    FeatureControlFrameSpec fcfSpec;
    fcfSpec.characteristic = GdtCharacteristic::Position;
    fcfSpec.diameterZone = true;
    fcfSpec.toleranceValue = "0.05";
    fcfSpec.materialModifier = MaterialConditionModifier::MMC;
    fcfSpec.datumA = "A";
    fcfSpec.datumB = "B";
    fcfSpec.datumBModifier = MaterialConditionModifier::MMC;
    fcfSpec.datumC = "C";

    auto* gdtSym = DraftingTools::createGdtFrameOnPage(page, fcfSpec, 140.0, 95.0);
    ASSERT_NE(gdtSym, nullptr);
    const std::string svg = gdtSym->Symbol.getStrValue();
    EXPECT_NE(svg.find("⌖"), std::string::npos);
    EXPECT_NE(svg.find("Ø0.05 Ⓜ"), std::string::npos);
    EXPECT_NE(svg.find(">A</text>"), std::string::npos);
    EXPECT_NE(svg.find(">B Ⓜ</text>"), std::string::npos);
    EXPECT_NE(svg.find(">C</text>"), std::string::npos);
}

TEST_F(DraftingToolsTest, formatsParametricHoleAndThreadCallouts)
{
    using Drawing::DraftingTools;

    auto* mockHole = m_doc->addObject<Part::Feature>("HoleFeature");
    ASSERT_NE(mockHole, nullptr);

    auto* pThreaded = static_cast<App::PropertyBool*>(
        mockHole->addDynamicProperty("App::PropertyBool", "Threaded")
    );
    auto* pThreadSize = static_cast<App::PropertyString*>(
        mockHole->addDynamicProperty("App::PropertyString", "ThreadSize")
    );
    auto* pThreadClass = static_cast<App::PropertyString*>(
        mockHole->addDynamicProperty("App::PropertyString", "ThreadClass")
    );
    auto* pDia = static_cast<App::PropertyFloat*>(
        mockHole->addDynamicProperty("App::PropertyFloat", "Diameter")
    );
    auto* pDepthType = static_cast<App::PropertyString*>(
        mockHole->addDynamicProperty("App::PropertyString", "DepthType")
    );
    auto* pDepth = static_cast<App::PropertyFloat*>(
        mockHole->addDynamicProperty("App::PropertyFloat", "Depth")
    );
    auto* pCutType = static_cast<App::PropertyString*>(
        mockHole->addDynamicProperty("App::PropertyString", "HoleCutType")
    );
    auto* pCutDia = static_cast<App::PropertyFloat*>(
        mockHole->addDynamicProperty("App::PropertyFloat", "HoleCutDiameter")
    );
    auto* pCutDepth = static_cast<App::PropertyFloat*>(
        mockHole->addDynamicProperty("App::PropertyFloat", "HoleCutDepth")
    );

    // Case 1: Plain counterbored through hole (Ø6.5 THRU | ⌴ Ø11.0 ↧ 6.0)
    pThreaded->setValue(false);
    pDia->setValue(6.5);
    pDepthType->setValue("ThroughAll");
    pCutType->setValue("Counterbore");
    pCutDia->setValue(11.0);
    pCutDepth->setValue(6.0);

    const auto cbInfo = DraftingTools::extractHoleCalloutFromFeature(mockHole);
    EXPECT_EQ(cbInfo.formattedCallout, "Ø6.5 THRU | ⌴ Ø11.0 ↧ 6.0");

    // Case 2: Blind metric threaded hole (M8x1.25 - 6H ↧ 16.0)
    pThreaded->setValue(true);
    pThreadSize->setValue("M8x1.25");
    pThreadClass->setValue("6H");
    pDepthType->setValue("Dimension");
    pDepth->setValue(16.0);
    pCutType->setValue("None");

    const auto threadInfo = DraftingTools::extractHoleCalloutFromFeature(mockHole);
    EXPECT_EQ(threadInfo.formattedCallout, "M8x1.25 - 6H ↧ 16.0");
}

TEST_F(DraftingToolsTest, buildsAssemblyBomTableAndAlignsLinkedBalloonsOnMagneticLine)
{
    using Drawing::DraftingTools;

    auto* page = createTestPageWithTemplate();
    auto* part1 = m_doc->addObject<Part::Feature>("Housing");
    auto* part2 = m_doc->addObject<Part::Feature>("Shaft");
    ASSERT_NE(page, nullptr);
    ASSERT_NE(part1, nullptr);
    ASSERT_NE(part2, nullptr);

    part1->Label.setValue("Gearbox Housing");
    part2->Label.setValue("Drive Shaft");
    part1->Shape.setValue(BRepPrimAPI_MakeBox(40.0, 30.0, 20.0).Shape());
    part2->Shape.setValue(BRepPrimAPI_MakeBox(15.0, 60.0, 15.0).Shape());

    if (auto* mat1 = static_cast<App::PropertyString*>(
            part1->addDynamicProperty("App::PropertyString", "MaterialName")
        )) {
        mat1->setValue("EN-GJL-250");
    }
    if (auto* mat2 = static_cast<App::PropertyString*>(
            part2->addDynamicProperty("App::PropertyString", "MaterialName")
        )) {
        mat2->setValue("42CrMo4");
    }

    // 1. Collect BOM entries & create BOM table on sheet
    const auto entries = DraftingTools::collectBomEntries(m_doc);
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].itemNumber, "1");
    EXPECT_EQ(entries[0].partNumber, "Gearbox Housing");
    EXPECT_EQ(entries[0].material, "EN-GJL-250");
    EXPECT_EQ(entries[1].itemNumber, "2");
    EXPECT_EQ(entries[1].partNumber, "Drive Shaft");
    EXPECT_EQ(entries[1].material, "42CrMo4");

    TechDraw::DrawViewSpreadsheet* bomTable = DraftingTools::createOrUpdateBomTableOnPage(page);
    ASSERT_NE(bomTable, nullptr);
    EXPECT_EQ(bomTable->CellStart.getStrValue(), "A1");
    EXPECT_EQ(bomTable->CellEnd.getStrValue(), "E3");

    // 2. Resolve item numbers by component
    EXPECT_EQ(DraftingTools::resolveBomItemNumber(m_doc, part1, entries), "1");
    EXPECT_EQ(DraftingTools::resolveBomItemNumber(m_doc, part2, entries), "2");

    // 3. Auto-Balloon + Magnetic Line alignment
    auto* viewPart = m_doc->addObject<TechDraw::DrawViewPart>("AssemblyView");
    ASSERT_NE(viewPart, nullptr);
    viewPart->Source.setValues({part1, part2});
    page->addView(viewPart, false);

    auto balloons = DraftingTools::createAutoBalloonsForView(page, viewPart);
    ASSERT_EQ(balloons.size(), 2U);

    // Align balloons along a horizontal magnetic line from (-30, 40) to (+30, 40)
    DraftingTools::alignBalloonsOnMagneticLine(
        balloons,
        Base::Vector3d(-30.0, 40.0, 0.0),
        Base::Vector3d(30.0, 40.0, 0.0)
    );
    EXPECT_NEAR(balloons[0]->Y.getValue(), 40.0, 1e-6);
    EXPECT_NEAR(balloons[1]->Y.getValue(), 40.0, 1e-6);
    EXPECT_NEAR(std::abs(balloons[1]->X.getValue() - balloons[0]->X.getValue()), 60.0, 1e-6);
}

}  // namespace
