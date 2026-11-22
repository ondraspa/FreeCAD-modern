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

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/Part.h>
#include <Base/BoundBox.h>
#include <Base/Type.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Mod/Drawing/App/TitleBlockBinder.h>
#include <Mod/Part/App/PartFeature.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawSVGTemplate.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/Gui/ViewProviderPage.h>

#include "TaskSheetSetup.h"

namespace DrawingGui
{

namespace
{

Base::BoundBox3d computeSourceBoundingBox(App::DocumentObject* obj)
{
    Base::BoundBox3d box;
    if (!obj) {
        return box;
    }
    if (auto* partFeat = dynamic_cast<Part::Feature*>(obj)) {
        const auto shape = partFeat->Shape.getValue();
        if (!shape.IsNull()) {
            return partFeat->Shape.getBoundingBox();
        }
    }
    for (App::DocumentObject* child : obj->getOutListRecursive()) {
        if (auto* partFeat = dynamic_cast<Part::Feature*>(child)) {
            const auto shape = partFeat->Shape.getValue();
            if (!shape.IsNull()) {
                box.Add(partFeat->Shape.getBoundingBox());
            }
        }
    }
    return box;
}

TechDraw::DrawViewPart* addStandaloneView(
    App::Document* doc,
    TechDraw::DrawPage* page,
    App::DocumentObject* source,
    const std::string& label,
    const Base::Vector3d& dir,
    const Base::Vector3d& xDir,
    double pageX,
    double pageY
)
{
    auto* view = doc->addObject<TechDraw::DrawViewPart>("View");
    if (!view) {
        return nullptr;
    }
    view->Label.setValue(label);
    if (source) {
        view->Source.setValues({source});
    }
    view->Direction.setValue(dir);
    view->XDirection.setValue(xDir);
    view->ScaleType.setValue("Page");
    view->X.setValue(pageX);
    view->Y.setValue(pageY);
    page->addView(view, false);
    return view;
}

}  // namespace

SheetSetupDialog::SheetSetupDialog(
    App::Document* doc,
    TechDraw::DrawPage* existingPage,
    App::DocumentObject* defaultSource,
    QWidget* parent
)
    : QDialog(parent)
    , m_doc(doc)
    , m_editingExisting(existingPage != nullptr)
    , m_presets(Drawing::ProjectionMath::availableSheetPresets())
{
    setWindowTitle(m_editingExisting ? tr("Sheet Properties") : tr("New Drawing Sheet"));
    setMinimumWidth(420);

    auto* mainLayout = new QVBoxLayout(this);

    auto* formGroup = new QGroupBox(tr("Sheet Format && Standard"), this);
    auto* form = new QFormLayout(formGroup);

    m_nameEdit = new QLineEdit(formGroup);
    if (existingPage) {
        m_nameEdit->setText(QString::fromUtf8(existingPage->Label.getValue()));
    }
    else {
        int nextNum = 1;
        if (doc) {
            nextNum = static_cast<int>(
                          doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId()).size()
                      )
                + 1;
        }
        m_nameEdit->setText(tr("Sheet %1").arg(nextNum));
    }
    form->addRow(tr("Sheet name:"), m_nameEdit);

    m_presetCombo = new QComboBox(formGroup);
    populatePresets(existingPage);
    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, &SheetSetupDialog::onPresetChanged);
    form->addRow(tr("Template:"), m_presetCombo);

    m_projectionCombo = new QComboBox(formGroup);
    m_projectionCombo->addItem(tr("First angle (ISO)"), false);
    m_projectionCombo->addItem(tr("Third angle (ASME)"), true);
    if (existingPage) {
        m_projectionCombo->setCurrentIndex(existingPage->ProjectionType.getValue() == 1 ? 1 : 0);
    }
    else {
        const int idx = m_presetCombo->currentIndex();
        if (idx >= 0 && static_cast<std::size_t>(idx) < m_presets.size()) {
            m_projectionCombo->setCurrentIndex(m_presets[static_cast<std::size_t>(idx)].thirdAngle ? 1 : 0);
        }
    }
    form->addRow(tr("Projection:"), m_projectionCombo);

    auto* scaleRow = new QHBoxLayout();
    m_scalePresetCombo = new QComboBox(formGroup);
    if (!m_editingExisting) {
        m_scalePresetCombo->addItem(tr("Auto-fit"), -1.0);
    }
    m_scalePresetCombo->addItem(QStringLiteral("1:1"), 1.0);
    m_scalePresetCombo->addItem(QStringLiteral("1:2"), 0.5);
    m_scalePresetCombo->addItem(QStringLiteral("1:5"), 0.2);
    m_scalePresetCombo->addItem(QStringLiteral("1:10"), 0.1);
    m_scalePresetCombo->addItem(QStringLiteral("2:1"), 2.0);
    m_scalePresetCombo->addItem(QStringLiteral("5:1"), 5.0);
    m_scalePresetCombo->addItem(tr("Custom…"), 0.0);

    m_customScaleSpin = new QDoubleSpinBox(formGroup);
    m_customScaleSpin->setRange(0.001, 1000.0);
    m_customScaleSpin->setDecimals(3);
    m_customScaleSpin->setSingleStep(0.25);
    m_customScaleSpin->setValue(existingPage ? existingPage->Scale.getValue() : 1.0);
    m_customScaleSpin->setEnabled(false);

    if (existingPage) {
        const double curScale = existingPage->Scale.getValue();
        bool matched = false;
        for (int i = 0; i < m_scalePresetCombo->count(); ++i) {
            const double v = m_scalePresetCombo->itemData(i).toDouble();
            if (v > 0.0 && std::abs(v - curScale) < 1e-4) {
                m_scalePresetCombo->setCurrentIndex(i);
                matched = true;
                break;
            }
        }
        if (!matched) {
            m_scalePresetCombo->setCurrentIndex(m_scalePresetCombo->count() - 1);
            m_customScaleSpin->setEnabled(true);
        }
    }

    connect(
        m_scalePresetCombo,
        &QComboBox::currentIndexChanged,
        this,
        &SheetSetupDialog::onScalePresetChanged
    );
    scaleRow->addWidget(m_scalePresetCombo, 1);
    scaleRow->addWidget(m_customScaleSpin, 0);
    form->addRow(tr("Sheet scale:"), scaleRow);

    mainLayout->addWidget(formGroup);

    if (!m_editingExisting) {
        auto* layoutGroup = new QGroupBox(tr("Initial Views"), this);
        auto* layoutVBox = new QVBoxLayout(layoutGroup);

        auto* srcRow = new QHBoxLayout();
        srcRow->addWidget(new QLabel(tr("Model source:"), layoutGroup));
        m_sourceCombo = new QComboBox(layoutGroup);
        populateSources(doc, defaultSource);
        srcRow->addWidget(m_sourceCombo, 1);
        layoutVBox->addLayout(srcRow);

        m_fourViewModeRadio = new QRadioButton(
            tr("Standard 3 Views + Isometric (aligned standalone views)"),
            layoutGroup
        );
        m_blankModeRadio = new QRadioButton(
            tr("Blank sheet (place views interactively from View Palette)"),
            layoutGroup
        );

        const bool hasSource = !m_sources.empty();
        m_fourViewModeRadio->setEnabled(hasSource);
        m_fourViewModeRadio->setChecked(hasSource);
        m_blankModeRadio->setChecked(!hasSource);

        layoutVBox->addWidget(m_fourViewModeRadio);
        layoutVBox->addWidget(m_blankModeRadio);
        mainLayout->addWidget(layoutGroup);
    }

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void SheetSetupDialog::populatePresets(TechDraw::DrawPage* existingPage)
{
    const Drawing::SheetPreset defPreset
        = Drawing::ProjectionMath::defaultSheetPresetForUnits(Drawing::ProjectionMath::isImperialUnitSystem());

    int selectIdx = 0;
    std::string existingTemplatePath;
    if (existingPage) {
        if (auto* svgTmpl = dynamic_cast<TechDraw::DrawSVGTemplate*>(existingPage->Template.getValue())) {
            existingTemplatePath = svgTmpl->Template.getValue();
        }
    }

    for (std::size_t i = 0; i < m_presets.size(); ++i) {
        const auto& preset = m_presets[i];
        m_presetCombo->addItem(QString::fromStdString(preset.displayName), QString::fromStdString(preset.id));
        if (!existingTemplatePath.empty()
            && existingTemplatePath.find(preset.templateRelPath) != std::string::npos) {
            selectIdx = static_cast<int>(i);
        }
        else if (existingTemplatePath.empty() && preset.id == defPreset.id) {
            selectIdx = static_cast<int>(i);
        }
    }
    m_presetCombo->setCurrentIndex(selectIdx);
}

void SheetSetupDialog::populateSources(App::Document* doc, App::DocumentObject* defaultSource)
{
    m_sources = collectDrawableSources(doc);
    m_sourceCombo->clear();

    if (m_sources.empty()) {
        m_sourceCombo->addItem(tr("(No 3D model in document)"));
        m_sourceCombo->setEnabled(false);
        return;
    }

    int defaultIdx = 0;
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        App::DocumentObject* obj = m_sources[i];
        m_sourceCombo->addItem(QString::fromUtf8(obj->Label.getValue()));
        if (obj == defaultSource) {
            defaultIdx = static_cast<int>(i);
        }
    }
    m_sourceCombo->setCurrentIndex(defaultIdx);
}

void SheetSetupDialog::onPresetChanged(int index)
{
    if (m_editingExisting || index < 0 || static_cast<std::size_t>(index) >= m_presets.size()) {
        return;
    }
    m_projectionCombo->setCurrentIndex(m_presets[static_cast<std::size_t>(index)].thirdAngle ? 1 : 0);
}

void SheetSetupDialog::onScalePresetChanged(int index)
{
    const double val = m_scalePresetCombo->itemData(index).toDouble();
    m_customScaleSpin->setEnabled(val == 0.0);
    if (val > 0.0) {
        m_customScaleSpin->setValue(val);
    }
}

SheetSetupResult SheetSetupDialog::result() const
{
    SheetSetupResult out;
    out.sheetName = m_nameEdit->text().trimmed().toStdString();
    if (out.sheetName.empty()) {
        out.sheetName = "Sheet 1";
    }

    const int presetIdx = m_presetCombo->currentIndex();
    if (presetIdx >= 0 && static_cast<std::size_t>(presetIdx) < m_presets.size()) {
        out.preset = m_presets[static_cast<std::size_t>(presetIdx)];
    }
    else {
        out.preset = Drawing::ProjectionMath::defaultSheetPresetForUnits(
            Drawing::ProjectionMath::isImperialUnitSystem()
        );
    }

    out.thirdAngle = m_projectionCombo->currentData().toBool();
    const double presetScale = m_scalePresetCombo->currentData().toDouble();
    if (presetScale < 0.0) {
        out.autoScale = true;
        out.scale = 1.0;
    }
    else if (presetScale == 0.0) {
        out.autoScale = false;
        out.scale = m_customScaleSpin->value();
    }
    else {
        out.autoScale = false;
        out.scale = presetScale;
    }

    if (!m_editingExisting && m_sourceCombo && !m_sources.empty()) {
        const int srcIdx = m_sourceCombo->currentIndex();
        if (srcIdx >= 0 && static_cast<std::size_t>(srcIdx) < m_sources.size()) {
            out.sourceObject = m_sources[static_cast<std::size_t>(srcIdx)];
        }
        out.createStandardFourViews = m_fourViewModeRadio && m_fourViewModeRadio->isChecked()
            && out.sourceObject != nullptr;
    }

    return out;
}

std::vector<App::DocumentObject*> SheetSetupDialog::collectDrawableSources(App::Document* doc)
{
    std::vector<App::DocumentObject*> result;
    if (!doc) {
        return result;
    }

    const Base::Type bodyType = Base::Type::fromName("PartDesign::Body");
    const Base::Type assemblyType = Base::Type::fromName("Assembly::AssemblyObject");
    const Base::Type sketchType = Base::Type::fromName("Sketcher::SketchObject");

    // Prefer top-level Assemblies, Parts, and PartDesign::Bodies first.
    for (App::DocumentObject* obj : doc->getObjects()) {
        if (!obj) {
            continue;
        }
        if ((!assemblyType.isBad() && obj->isDerivedFrom(assemblyType))
            || obj->isDerivedFrom<App::Part>()
            || (!bodyType.isBad() && obj->isDerivedFrom(bodyType))) {
            result.push_back(obj);
        }
    }

    // Also include standalone Part::Features that are not hidden inside a Body or Sketch.
    for (App::DocumentObject* obj : doc->getObjects()) {
        if (!obj || !obj->isDerivedFrom<Part::Feature>()) {
            continue;
        }
        if (!sketchType.isBad() && obj->isDerivedFrom(sketchType)) {
            continue;
        }
        if (!bodyType.isBad() && obj->isDerivedFrom(bodyType)) {
            continue;
        }
        // Skip internal Body features when their parent Body is already listed.
        bool insideListedContainer = false;
        for (App::DocumentObject* parent : obj->getInList()) {
            if (std::find(result.begin(), result.end(), parent) != result.end()) {
                insideListedContainer = true;
                break;
            }
        }
        if (!insideListedContainer) {
            auto* pf = static_cast<Part::Feature*>(obj);
            if (!pf->Shape.getValue().IsNull()) {
                result.push_back(obj);
            }
        }
    }

    return result;
}

TechDraw::DrawPage* SheetSetupDialog::createSheet(App::Document* doc, const SheetSetupResult& setup)
{
    if (!doc) {
        return nullptr;
    }

    doc->openTransaction("Create Drawing Sheet");

    auto* page = doc->addObject<TechDraw::DrawPage>("Page");
    auto* tmpl = doc->addObject<TechDraw::DrawSVGTemplate>("Template");
    if (!page || !tmpl) {
        doc->abortTransaction();
        return nullptr;
    }

    page->Label.setValue(setup.sheetName);
    tmpl->Label.setValue(setup.sheetName + " Template");
    tmpl->Template.setValue(Drawing::ProjectionMath::resolveTemplatePath(setup.preset));
    page->Template.setValue(tmpl);
    page->ProjectionType.setValue(setup.thirdAngle ? 1L : 0L);

    double sheetScale = setup.scale;
    if (setup.autoScale && setup.sourceObject) {
        const Base::BoundBox3d bb = computeSourceBoundingBox(setup.sourceObject);
        if (bb.IsValid()) {
            sheetScale = Drawing::ProjectionMath::suggestStandardScale(
                std::max(bb.LengthX(), bb.LengthY()),
                std::max(bb.LengthZ(), bb.LengthY()),
                setup.preset.widthMm,
                setup.preset.heightMm
            );
        }
    }
    page->Scale.setValue(sheetScale);

    if (setup.createStandardFourViews && setup.sourceObject) {
        const double w = setup.preset.widthMm;
        const double h = setup.preset.heightMm;

        // Place base Front view cleanly in left quadrant, with orthogonal Top/Bottom, Right/Left, and Isometric views.
        const double frontX = w * 0.30;
        const double frontY = setup.thirdAngle ? (h * 0.38) : (h * 0.64);
        const double offsetX = w * 0.30;
        const double offsetY = h * 0.28;

        const auto frontOrient
            = Drawing::ProjectionMath::standardOrientation(Drawing::StandardOrientation::Front);
        TechDraw::DrawViewPart* frontView = addStandaloneView(
            doc,
            page,
            setup.sourceObject,
            "Front",
            frontOrient.direction,
            frontOrient.xDirection,
            frontX,
            frontY
        );

        if (frontView) {
            // Projected side view (East of Front)
            const auto sideSpec = Drawing::ProjectionMath::computeProjectedView(
                frontOrient.direction,
                frontOrient.xDirection,
                Drawing::ProjectionDirection8::East,
                setup.thirdAngle
            );
            if (auto* sideView = addStandaloneView(
                    doc,
                    page,
                    setup.sourceObject,
                    sideSpec.label,
                    sideSpec.direction,
                    sideSpec.xDirection,
                    frontX + offsetX,
                    frontY
                )) {
                Drawing::ProjectionMath::attachViewAlignment(
                    sideView,
                    frontView,
                    sideSpec.alignment,
                    sideSpec.alignmentVector
                );
            }

            // Projected plan view (North in Third Angle, South in First Angle)
            const auto planDir = setup.thirdAngle ? Drawing::ProjectionDirection8::North
                                                  : Drawing::ProjectionDirection8::South;
            const auto planSpec = Drawing::ProjectionMath::computeProjectedView(
                frontOrient.direction,
                frontOrient.xDirection,
                planDir,
                setup.thirdAngle
            );
            const double planY = setup.thirdAngle ? (frontY + offsetY) : (frontY - offsetY);
            if (auto* planView = addStandaloneView(
                    doc,
                    page,
                    setup.sourceObject,
                    planSpec.label,
                    planSpec.direction,
                    planSpec.xDirection,
                    frontX,
                    planY
                )) {
                Drawing::ProjectionMath::attachViewAlignment(
                    planView,
                    frontView,
                    planSpec.alignment,
                    planSpec.alignmentVector
                );
            }

            // Isometric view in upper-right corner (unaligned free placement)
            const auto isoOrient
                = Drawing::ProjectionMath::standardOrientation(Drawing::StandardOrientation::Isometric);
            addStandaloneView(
                doc,
                page,
                setup.sourceObject,
                "Isometric",
                isoOrient.direction,
                isoOrient.xDirection,
                w * 0.78,
                h * 0.72
            );
        }
    }

    doc->recompute();
    Drawing::TitleBlockBinder::refreshAllPagesInDocument(doc);
    doc->recompute();
    doc->commitTransaction();

    // Open and display the MDIViewPage tab immediately.
    if (Gui::Document* gdoc = Gui::Application::Instance ? Gui::Application::Instance->getDocument(doc)
                                                         : nullptr) {
        if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderPage*>(gdoc->getViewProvider(page))) {
            vp->show();
        }
    }

    return page;
}

void SheetSetupDialog::applyPropertiesToSheet(TechDraw::DrawPage* page, const SheetSetupResult& setup)
{
    if (!page || !page->getDocument()) {
        return;
    }

    App::Document* doc = page->getDocument();
    doc->openTransaction("Edit Sheet Properties");

    page->Label.setValue(setup.sheetName);
    page->ProjectionType.setValue(setup.thirdAngle ? 1L : 0L);
    page->Scale.setValue(setup.scale);

    if (auto* svgTmpl = dynamic_cast<TechDraw::DrawSVGTemplate*>(page->Template.getValue())) {
        const std::string resolved = Drawing::ProjectionMath::resolveTemplatePath(setup.preset);
        if (svgTmpl->Template.getValue() != resolved) {
            svgTmpl->Template.setValue(resolved);
        }
    }

    doc->recompute();
    Drawing::TitleBlockBinder::bindTitleBlock(page, nullptr, false);
    doc->recompute();
    doc->commitTransaction();
}

}  // namespace DrawingGui
