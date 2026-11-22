// SPDX-License-Identifier: LGPL-2.1-or-later

#include <gtest/gtest.h>

#include <App/Application.h>
#include <App/Document.h>
#include <Mod/Drawing/App/TitleBlockBinder.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawTemplate.h>
#include <src/App/InitApplication.h>

namespace
{

class TitleBlockBinderTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        tests::initApplication();
        m_docName = App::GetApplication().getUniqueDocumentName("drawing_tb_test");
        m_doc = App::GetApplication().newDocument(m_docName.c_str(), "testUser");
    }

    void TearDown() override
    {
        App::GetApplication().closeDocument(m_docName.c_str());
    }

    std::string m_docName;
    App::Document* m_doc {nullptr};
};

TEST_F(TitleBlockBinderTest, formatsCleanDraftingScaleRatios)
{
    using Drawing::TitleBlockBinder;

    EXPECT_EQ(TitleBlockBinder::formatScaleRatio(1.0), "1:1");
    EXPECT_EQ(TitleBlockBinder::formatScaleRatio(0.5), "1:2");
    EXPECT_EQ(TitleBlockBinder::formatScaleRatio(0.2), "1:5");
    EXPECT_EQ(TitleBlockBinder::formatScaleRatio(2.0), "2:1");
    EXPECT_EQ(TitleBlockBinder::formatScaleRatio(5.0), "5:1");
}

TEST_F(TitleBlockBinderTest, bindsIsoAndAsmeFieldsAndPreservesManualOverrides)
{
    using Drawing::TitleBlockBinder;

    m_doc->Label.setValue("Gearbox Housing");
    m_doc->CreatedBy.setValue("Ada Lovelace");
    m_doc->Company.setValue("Analytical Engines Ltd.");

    auto* page = m_doc->addObject<TechDraw::DrawPage>("Sheet1");
    auto* tmpl = m_doc->addObject<TechDraw::DrawTemplate>("Template1");
    ASSERT_NE(page, nullptr);
    ASSERT_NE(tmpl, nullptr);

    page->Scale.setValue(0.5);
    page->Template.setValue(tmpl);

    // Seed ISO5457 + ASME-style editable keys with typical placeholders
    tmpl->EditableTexts.setValues({
        {"title", "Title"},
        {"creator", "Author"},
        {"legal_owner_1", "Company"},
        {"scale", "1:1"},
        {"sheet_number", "1 / 1"},
        {"DrawingTitle1", ""},
        {"DrawnBy", "DrawnBy"},
    });

    const int firstUpdated = TitleBlockBinder::bindTitleBlock(page);
    EXPECT_GE(firstUpdated, 5);

    auto vals = tmpl->EditableTexts.getValues();
    EXPECT_EQ(vals["title"], "Gearbox Housing");
    EXPECT_EQ(vals["DrawingTitle1"], "Gearbox Housing");
    EXPECT_EQ(vals["creator"], "Ada Lovelace");
    EXPECT_EQ(vals["DrawnBy"], "Ada Lovelace");
    EXPECT_EQ(vals["legal_owner_1"], "Analytical Engines Ltd.");
    EXPECT_EQ(vals["scale"], "1:2");
    EXPECT_EQ(vals["sheet_number"], "1 / 1");

    // Simulate user manually overriding "title", while changing page scale to 2:1
    vals["title"] = "Custom Rev C Title";
    tmpl->EditableTexts.setValues(vals);
    page->Scale.setValue(2.0);

    // Add a second sheet to verify sheet numbering updates to "1 / 2"
    m_doc->addObject<TechDraw::DrawPage>("Sheet2");

    TitleBlockBinder::bindTitleBlock(page);
    const auto updatedVals = tmpl->EditableTexts.getValues();
    EXPECT_EQ(updatedVals.at("title"), "Custom Rev C Title");  // manual override preserved!
    EXPECT_EQ(updatedVals.at("scale"), "2:1");                 // auto-bound field refreshed!
    EXPECT_EQ(updatedVals.at("sheet_number"), "1 / 2");        // sheet count refreshed!
}

}  // namespace
