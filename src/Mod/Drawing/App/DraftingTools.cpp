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
#include <cctype>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <set>
#include <sstream>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/Link.h>
#include <App/Part.h>
#include <App/PropertyGeo.h>
#include <App/PropertyLinks.h>
#include <App/PropertyStandard.h>
#include <App/PropertyUnits.h>
#include <Base/BoundBox.h>
#include <Base/Type.h>
#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/Part/App/PartFeature.h>
#include <Mod/Spreadsheet/App/Cell.h>
#include <Mod/Spreadsheet/App/Sheet.h>
#include <Mod/TechDraw/App/DrawBrokenView.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawTemplate.h>
#include <Mod/TechDraw/App/DrawViewBalloon.h>
#include <Mod/TechDraw/App/DrawViewClip.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/DrawViewSpreadsheet.h>
#include <Mod/TechDraw/App/DrawViewSymbol.h>
#include <Mod/TechDraw/App/Geometry.h>

#include "DraftingTools.h"

namespace Drawing
{

namespace
{

std::string xmlEscape(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

std::string formatCleanNumber(double value, int maxDecimals = 2)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(maxDecimals) << value;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.push_back('0');
        }
    }
    return s;
}

template<typename PropT>
PropT* ensureDynamicProp(
    App::DocumentObject* obj,
    const char* typeName,
    const char* propName,
    const char* groupName,
    const char* doc
)
{
    if (!obj) {
        return nullptr;
    }
    if (auto* existing = dynamic_cast<PropT*>(obj->getPropertyByName(propName))) {
        return existing;
    }
    return dynamic_cast<PropT*>(obj->addDynamicProperty(typeName, propName, groupName, doc));
}

bool getBoolProp(const App::DocumentObject* obj, const char* name, bool defVal = false)
{
    if (!obj) {
        return defVal;
    }
    if (auto* p = dynamic_cast<const App::PropertyBool*>(obj->getPropertyByName(name))) {
        return p->getValue();
    }
    return defVal;
}

double getFloatProp(const App::DocumentObject* obj, const char* name, double defVal = 0.0)
{
    if (!obj) {
        return defVal;
    }
    if (auto* p = dynamic_cast<const App::PropertyFloat*>(obj->getPropertyByName(name))) {
        return p->getValue();
    }
    return defVal;
}

std::string getEnumOrStringProp(const App::DocumentObject* obj, const char* name)
{
    if (!obj) {
        return {};
    }
    const App::Property* prop = obj->getPropertyByName(name);
    if (!prop) {
        return {};
    }
    if (auto* e = dynamic_cast<const App::PropertyEnumeration*>(prop)) {
        return e->isValid() ? std::string(e->getValueAsString()) : std::string();
    }
    if (auto* s = dynamic_cast<const App::PropertyString*>(prop)) {
        return s->getStrValue();
    }
    return {};
}

bool isHoleLikeFeature(const App::DocumentObject* obj)
{
    if (!obj) {
        return false;
    }
    const Base::Type holeType = Base::Type::fromName("PartDesign::Hole");
    if (!holeType.isBad() && obj->isDerivedFrom(holeType)) {
        return true;
    }
    return obj->getPropertyByName("Threaded") != nullptr
        && obj->getPropertyByName("HoleCutType") != nullptr
        && obj->getPropertyByName("Diameter") != nullptr;
}

int countHoleInstancesInProfile(const App::DocumentObject* holeObj)
{
    if (!holeObj) {
        return 1;
    }
    const auto* profProp = dynamic_cast<const App::PropertyLinkSub*>(holeObj->getPropertyByName("Profile"));
    if (!profProp || !profProp->getValue()) {
        return 1;
    }
    auto* partFeat = dynamic_cast<Part::Feature*>(profProp->getValue());
    if (!partFeat) {
        return 1;
    }
    const TopoDS_Shape shape = partFeat->Shape.getValue();
    if (shape.IsNull()) {
        return 1;
    }
    // Count distinct circular edges or vertices in the profile sketch
    int edgeCount = 0;
    for (const auto& sub : profProp->getSubValues()) {
        if (sub.rfind("Edge", 0) == 0 || sub.rfind("Vertex", 0) == 0) {
            ++edgeCount;
        }
    }
    if (edgeCount > 1) {
        return edgeCount;
    }
    return 1;
}

const App::DocumentObject* findHoleFeatureRecursive(
    const App::DocumentObject* root,
    std::set<const App::DocumentObject*>& visited
)
{
    if (!root || visited.contains(root)) {
        return nullptr;
    }
    visited.insert(root);
    if (isHoleLikeFeature(root)) {
        return root;
    }
    if (auto* link = dynamic_cast<const App::Link*>(root)) {
        if (const auto* linked = link->getLinkedObject(true)) {
            if (const auto* found = findHoleFeatureRecursive(linked, visited)) {
                return found;
            }
        }
    }
    if (auto* groupProp = dynamic_cast<const App::PropertyLinkList*>(root->getPropertyByName("Group"))) {
        const auto& children = groupProp->getValues();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (const auto* found = findHoleFeatureRecursive(*it, visited)) {
                return found;
            }
        }
    }
    for (const App::DocumentObject* child : root->getOutList()) {
        if (const auto* found = findHoleFeatureRecursive(child, visited)) {
            return found;
        }
    }
    return nullptr;
}

std::string extractObjectMaterial(const App::DocumentObject* obj)
{
    if (!obj) {
        return {};
    }
    if (auto* s = dynamic_cast<const App::PropertyString*>(obj->getPropertyByName("MaterialName"))) {
        if (!s->getStrValue().empty()) {
            return s->getStrValue();
        }
    }
    if (auto* m = dynamic_cast<const App::PropertyMap*>(obj->getPropertyByName("Material"))) {
        const auto& vals = m->getValues();
        if (auto it = vals.find("Name"); it != vals.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return {};
}

std::string extractObjectDescription(const App::DocumentObject* obj)
{
    if (!obj) {
        return {};
    }
    for (const char* propName : {"Description", "Label2"}) {
        if (auto* s = dynamic_cast<const App::PropertyString*>(obj->getPropertyByName(propName))) {
            if (!s->getStrValue().empty()) {
                return s->getStrValue();
            }
        }
    }
    return obj->Label.getStrValue();
}

App::DocumentObject* unwrapLinkTarget(App::DocumentObject* obj)
{
    if (!obj) {
        return nullptr;
    }
    if (auto* link = dynamic_cast<App::Link*>(obj)) {
        if (App::DocumentObject* target = link->getLinkedObject(true)) {
            return target;
        }
    }
    return obj;
}

}  // namespace

//===========================================================================
// DrawViewDatum & DrawViewGDT
//===========================================================================

PROPERTY_SOURCE(Drawing::DrawViewDatum, TechDraw::DrawViewSymbol)

DrawViewDatum::DrawViewDatum()
{
    static const char* group = "Datum Symbol";
    ADD_PROPERTY_TYPE(DatumLetter, ("A"), group, App::Prop_None, "Datum identification letter (e.g. A, B, C)");
    ADD_PROPERTY_TYPE(FilledTriangle, (true), group, App::Prop_None, "Use filled datum triangle per ISO 5459 / ASME Y14.5");
    ADD_PROPERTY_TYPE(StemLength, (8.0), group, App::Prop_None, "Stem length in mm between triangle base and datum frame");
    refreshSymbolSvg();
}

short DrawViewDatum::mustExecute() const
{
    if (!isRestoring()) {
        if (DatumLetter.isTouched() || FilledTriangle.isTouched() || StemLength.isTouched()) {
            return 1;
        }
    }
    return TechDraw::DrawViewSymbol::mustExecute();
}

void DrawViewDatum::refreshSymbolSvg()
{
    DatumSymbolSpec spec;
    spec.letter = DatumLetter.getStrValue().empty() ? "A" : DatumLetter.getStrValue();
    spec.filledTriangle = FilledTriangle.getValue();
    spec.stemLengthMm = std::max(2.0, StemLength.getValue());
    Symbol.setValue(DraftingTools::buildDatumFeatureSvg(spec));
}

App::DocumentObjectExecReturn* DrawViewDatum::execute()
{
    refreshSymbolSvg();
    return TechDraw::DrawViewSymbol::execute();
}

PROPERTY_SOURCE(Drawing::DrawViewGDT, TechDraw::DrawViewSymbol)

DrawViewGDT::DrawViewGDT()
{
    static const char* group = "GD&T Frame";
    ADD_PROPERTY_TYPE(Characteristic, ("Position"), group, App::Prop_None, "Geometric characteristic type");
    ADD_PROPERTY_TYPE(DiameterZone, (true), group, App::Prop_None, "Prefix tolerance value with cylindrical diameter symbol Ø");
    ADD_PROPERTY_TYPE(ToleranceValue, ("0.05"), group, App::Prop_None, "Geometric tolerance zone width");
    ADD_PROPERTY_TYPE(MaterialModifier, ("None"), group, App::Prop_None, "Material condition modifier (None, MMC, LMC, RFS)");
    ADD_PROPERTY_TYPE(DatumPrimary, ("A"), group, App::Prop_None, "Primary datum reference");
    ADD_PROPERTY_TYPE(DatumSecondary, (""), group, App::Prop_None, "Secondary datum reference");
    ADD_PROPERTY_TYPE(DatumTertiary, (""), group, App::Prop_None, "Tertiary datum reference");
    refreshSymbolSvg();
}

short DrawViewGDT::mustExecute() const
{
    if (!isRestoring()) {
        if (Characteristic.isTouched() || DiameterZone.isTouched() || ToleranceValue.isTouched()
            || MaterialModifier.isTouched() || DatumPrimary.isTouched()
            || DatumSecondary.isTouched() || DatumTertiary.isTouched()) {
            return 1;
        }
    }
    return TechDraw::DrawViewSymbol::mustExecute();
}

void DrawViewGDT::refreshSymbolSvg()
{
    FeatureControlFrameSpec spec;
    spec.characteristic = DraftingTools::characteristicFromString(Characteristic.getStrValue());
    spec.diameterZone = DiameterZone.getValue();
    spec.toleranceValue = ToleranceValue.getStrValue();
    spec.materialModifier = DraftingTools::materialModifierFromString(MaterialModifier.getStrValue());
    spec.datumA = DatumPrimary.getStrValue();
    spec.datumB = DatumSecondary.getStrValue();
    spec.datumC = DatumTertiary.getStrValue();
    Symbol.setValue(DraftingTools::buildFeatureControlFrameSvg(spec));
}

App::DocumentObjectExecReturn* DrawViewGDT::execute()
{
    refreshSymbolSvg();
    return TechDraw::DrawViewSymbol::execute();
}

//===========================================================================
// 2D Break View Helpers
//===========================================================================

std::pair<Base::Vector3d, Base::Vector3d> DraftingTools::compute3DBreakPointsFrom2DViewCut(
    const TechDraw::DrawViewPart* baseView,
    double cut1ViewMm,
    double cut2ViewMm,
    bool verticalBreakLine
)
{
    if (!baseView) {
        return {Base::Vector3d(cut1ViewMm, 0.0, 0.0), Base::Vector3d(cut2ViewMm, 0.0, 0.0)};
    }

    const Base::Vector3d centroid = baseView->getOriginalCentroid();
    const gp_Ax2 cs = baseView->getProjectionCS(centroid);
    const gp_Dir xDir = cs.XDirection();
    const gp_Dir yDir = cs.YDirection();

    const Base::Vector3d bx(xDir.X(), xDir.Y(), xDir.Z());
    const Base::Vector3d by(yDir.X(), yDir.Y(), yDir.Z());

    const double cMin = std::min(cut1ViewMm, cut2ViewMm);
    const double cMax = std::max(cut1ViewMm, cut2ViewMm);

    if (verticalBreakLine) {
        // Vertical break lines cut along the view's horizontal X axis
        return {centroid + bx * cMin, centroid + bx * cMax};
    }
    // Horizontal break lines cut along the view's vertical Y axis
    return {centroid + by * cMin, centroid + by * cMax};
}

TechDraw::DrawBrokenView* DraftingTools::createBrokenViewFrom2DCut(
    TechDraw::DrawPage* page,
    TechDraw::DrawViewPart* baseView,
    double cut1ViewMm,
    double cut2ViewMm,
    bool verticalBreakLine,
    double gapMm
)
{
    if (!page || !baseView || !page->getDocument()) {
        return nullptr;
    }
    if (std::abs(cut2ViewMm - cut1ViewMm) < 1e-3) {
        return nullptr;
    }

    App::Document* doc = page->getDocument();
    const auto pts3d = compute3DBreakPointsFrom2DViewCut(
        baseView,
        cut1ViewMm,
        cut2ViewMm,
        verticalBreakLine
    );

    // Create a hidden 3D Part::Feature edge defining the exact break span in 3D space
    // so TechDraw::DrawBrokenView executes natively and stays 100% compatible with upstream FreeCAD.
    auto* breakLineObj = doc->addObject<Part::Feature>("BreakSpan");
    if (!breakLineObj) {
        return nullptr;
    }
    const gp_Pnt p1(pts3d.first.x, pts3d.first.y, pts3d.first.z);
    const gp_Pnt p2(pts3d.second.x, pts3d.second.y, pts3d.second.z);
    breakLineObj->Shape.setValue(BRepBuilderAPI_MakeEdge(p1, p2).Edge());
    breakLineObj->Visibility.setValue(false);
    breakLineObj->recomputeFeature();

    // If baseView is already a DrawBrokenView, append the new break to it in place!
    if (auto* existingBroken = dynamic_cast<TechDraw::DrawBrokenView*>(baseView)) {
        auto breaks = existingBroken->Breaks.getValues();
        breaks.push_back(breakLineObj);
        existingBroken->Breaks.setValues(breaks);
        existingBroken->Gap.setValue(std::max(1.0, gapMm));
        existingBroken->recomputeFeature();
        return existingBroken;
    }

    auto* brokenView = doc->addObject<TechDraw::DrawBrokenView>("BrokenView");
    if (!brokenView) {
        return nullptr;
    }

    brokenView->Label.setValue(baseView->Label.getStrValue() + " (Broken)");
    brokenView->Source.setValues(baseView->Source.getValues());
    brokenView->XSource.setValues(baseView->XSource.getValues());
    brokenView->Direction.setValue(baseView->Direction.getValue());
    brokenView->XDirection.setValue(baseView->XDirection.getValue());
    brokenView->ScaleType.setValue(baseView->ScaleType.getValue());
    brokenView->Scale.setValue(baseView->Scale.getValue());
    brokenView->Rotation.setValue(baseView->Rotation.getValue());
    brokenView->X.setValue(baseView->X.getValue());
    brokenView->Y.setValue(baseView->Y.getValue());
    brokenView->Gap.setValue(std::max(1.0, gapMm));
    brokenView->Breaks.setValues({breakLineObj});

    // Record 2D break metadata as dynamic properties for inspection/editing
    if (auto* dirProp = ensureDynamicProp<App::PropertyString>(
            brokenView,
            "App::PropertyString",
            "BreakOrientation2D",
            "Broken view",
            "2D break line orientation (Vertical or Horizontal)"
        )) {
        dirProp->setValue(verticalBreakLine ? "Vertical" : "Horizontal");
    }
    if (auto* c1Prop = ensureDynamicProp<App::PropertyFloat>(
            brokenView,
            "App::PropertyFloat",
            "BreakCut1Mm",
            "Broken view",
            "First 2D break coordinate in unscaled view mm"
        )) {
        c1Prop->setValue(std::min(cut1ViewMm, cut2ViewMm));
    }
    if (auto* c2Prop = ensureDynamicProp<App::PropertyFloat>(
            brokenView,
            "App::PropertyFloat",
            "BreakCut2Mm",
            "Broken view",
            "Second 2D break coordinate in unscaled view mm"
        )) {
        c2Prop->setValue(std::max(cut1ViewMm, cut2ViewMm));
    }

    // Preserve any parent alignment on the replacement broken view
    if (TechDraw::DrawView* parentAligned = ProjectionMath::getParentAlignedView(baseView)) {
        const AlignmentKind mode = ProjectionMath::getViewAlignmentMode(baseView);
        Base::Vector3d alignVec(0.0, 0.0, 0.0);
        if (auto* vp = dynamic_cast<App::PropertyVector*>(baseView->getPropertyByName("AlignmentVector"))) {
            alignVec = vp->getValue();
        }
        ProjectionMath::attachViewAlignment(brokenView, parentAligned, mode, alignVec);
    }

    page->addView(brokenView, false);
    page->removeView(baseView);
    brokenView->recomputeFeature();
    return brokenView;
}

//===========================================================================
// In-Place Crop View Helpers
//===========================================================================

TechDraw::DrawViewClip* DraftingTools::cropViewInPlace(
    TechDraw::DrawPage* page,
    TechDraw::DrawViewPart* targetView,
    double centerPageX,
    double centerPageY,
    double widthMm,
    double heightMm,
    bool showFrame
)
{
    if (!page || !targetView || !page->getDocument()) {
        return nullptr;
    }

    App::Document* doc = page->getDocument();
    auto* clip = doc->addObject<TechDraw::DrawViewClip>("CropClip");
    if (!clip) {
        return nullptr;
    }

    const double cleanW = std::max(5.0, widthMm);
    const double cleanH = std::max(5.0, heightMm);
    const double offsetX = targetView->X.getValue() - centerPageX;
    const double offsetY = targetView->Y.getValue() - centerPageY;

    clip->Label.setValue(targetView->Label.getStrValue() + " (Crop)");
    clip->X.setValue(centerPageX);
    clip->Y.setValue(centerPageY);
    clip->ShowFrame.setValue(showFrame);

    page->addView(clip, false);
    clip->Width.setValue(cleanW);
    clip->Height.setValue(cleanH);
    clip->addView(targetView);

    // DrawViewClip positions child views relative to the clip center
    targetView->X.setValue(offsetX);
    targetView->Y.setValue(offsetY);

    clip->recomputeFeature();
    return clip;
}

//===========================================================================
// GD&T & Datum Symbol Builders
//===========================================================================

std::string DraftingTools::characteristicSymbolString(GdtCharacteristic ch)
{
    switch (ch) {
        case GdtCharacteristic::Straightness:
            return "⏤";
        case GdtCharacteristic::Flatness:
            return "⏥";
        case GdtCharacteristic::Circularity:
            return "○";
        case GdtCharacteristic::Cylindricity:
            return "⌭";
        case GdtCharacteristic::ProfileOfLine:
            return "⌒";
        case GdtCharacteristic::ProfileOfSurface:
            return "⌓";
        case GdtCharacteristic::Parallelism:
            return "∥";
        case GdtCharacteristic::Perpendicularity:
            return "⟂";
        case GdtCharacteristic::Angularity:
            return "∠";
        case GdtCharacteristic::Position:
            return "⌖";
        case GdtCharacteristic::Concentricity:
            return "◎";
        case GdtCharacteristic::Symmetry:
            return "⌯";
        case GdtCharacteristic::CircularRunout:
            return "↗";
        case GdtCharacteristic::TotalRunout:
            return "⇗";
    }
    return "⌖";
}

GdtCharacteristic DraftingTools::characteristicFromString(const std::string& name)
{
    std::string lower;
    lower.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            lower.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    if (lower == "straightness") {
        return GdtCharacteristic::Straightness;
    }
    if (lower == "flatness") {
        return GdtCharacteristic::Flatness;
    }
    if (lower == "circularity" || lower == "roundness") {
        return GdtCharacteristic::Circularity;
    }
    if (lower == "cylindricity") {
        return GdtCharacteristic::Cylindricity;
    }
    if (lower == "profileofline" || lower == "lineprofile") {
        return GdtCharacteristic::ProfileOfLine;
    }
    if (lower == "profileofsurface" || lower == "surfaceprofile") {
        return GdtCharacteristic::ProfileOfSurface;
    }
    if (lower == "parallelism") {
        return GdtCharacteristic::Parallelism;
    }
    if (lower == "perpendicularity") {
        return GdtCharacteristic::Perpendicularity;
    }
    if (lower == "angularity") {
        return GdtCharacteristic::Angularity;
    }
    if (lower == "concentricity" || lower == "coaxiality") {
        return GdtCharacteristic::Concentricity;
    }
    if (lower == "symmetry") {
        return GdtCharacteristic::Symmetry;
    }
    if (lower == "circularrunout" || lower == "runout") {
        return GdtCharacteristic::CircularRunout;
    }
    if (lower == "totalrunout") {
        return GdtCharacteristic::TotalRunout;
    }
    return GdtCharacteristic::Position;
}

std::string DraftingTools::materialModifierSymbolString(MaterialConditionModifier mod)
{
    switch (mod) {
        case MaterialConditionModifier::MMC:
            return "Ⓜ";
        case MaterialConditionModifier::LMC:
            return "Ⓛ";
        case MaterialConditionModifier::RFS:
            return "Ⓢ";
        case MaterialConditionModifier::None:
            return {};
    }
    return {};
}

MaterialConditionModifier DraftingTools::materialModifierFromString(const std::string& name)
{
    if (name == "MMC" || name == "M" || name == "Ⓜ") {
        return MaterialConditionModifier::MMC;
    }
    if (name == "LMC" || name == "L" || name == "Ⓛ") {
        return MaterialConditionModifier::LMC;
    }
    if (name == "RFS" || name == "S" || name == "Ⓢ") {
        return MaterialConditionModifier::RFS;
    }
    return MaterialConditionModifier::None;
}

std::string DraftingTools::buildDatumFeatureSvg(const DatumSymbolSpec& spec)
{
    const std::string letter = xmlEscape(spec.letter.empty() ? "A" : spec.letter);
    const double boxSize = 7.0;
    const double stem = std::max(3.0, spec.stemLengthMm);
    const double triBase = 4.0;
    const double triHeight = 3.5;
    const double totalW = boxSize + 4.0;
    const double totalH = boxSize + stem + triHeight + 2.0;
    const double cx = totalW * 0.5;
    const double boxTop = 1.0;
    const double boxBottom = boxTop + boxSize;
    const double triApexY = boxBottom + stem;
    const double triBaseY = triApexY + triHeight;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << totalW << "mm\" height=\""
        << totalH << "mm\" viewBox=\"0 0 " << totalW << " " << totalH << "\">\n"
        << "  <g stroke=\"#000000\" stroke-width=\"0.35\" fill=\"none\">\n"
        << "    <rect x=\"" << (cx - boxSize * 0.5) << "\" y=\"" << boxTop << "\" width=\""
        << boxSize << "\" height=\"" << boxSize << "\" fill=\"#ffffff\"/>\n"
        << "    <line x1=\"" << cx << "\" y1=\"" << boxBottom << "\" x2=\"" << cx << "\" y2=\""
        << triApexY << "\"/>\n"
        << "    <polygon points=\"" << cx << "," << triApexY << " " << (cx - triBase * 0.5) << ","
        << triBaseY << " " << (cx + triBase * 0.5) << "," << triBaseY << "\" fill=\""
        << (spec.filledTriangle ? "#000000" : "#ffffff") << "\"/>\n"
        << "    <line x1=\"" << (cx - triBase * 0.8) << "\" y1=\"" << triBaseY << "\" x2=\""
        << (cx + triBase * 0.8) << "\" y2=\"" << triBaseY << "\"/>\n"
        << "  </g>\n"
        << "  <text x=\"" << cx << "\" y=\"" << (boxTop + boxSize * 0.72)
        << "\" font-family=\"osifont, sans-serif\" font-size=\"4.2\" font-weight=\"bold\" "
           "text-anchor=\"middle\" fill=\"#000000\">"
        << letter << "</text>\n"
        << "</svg>\n";
    return svg.str();
}

std::string DraftingTools::buildFeatureControlFrameSvg(const FeatureControlFrameSpec& spec)
{
    const double rowH = 7.0;
    const double symCellW = 7.0;

    std::string tolText;
    if (spec.diameterZone) {
        tolText += "Ø";
    }
    tolText += (spec.toleranceValue.empty() ? "0.05" : spec.toleranceValue);
    const std::string matSym = materialModifierSymbolString(spec.materialModifier);
    if (!matSym.empty()) {
        tolText += " " + matSym;
    }

    const double tolCellW = std::max(14.0, static_cast<double>(tolText.size()) * 2.2 + 4.0);

    struct DatumCell
    {
        std::string text;
        double width;
    };
    std::vector<DatumCell> datums;
    auto pushDatum = [&datums](const std::string& d, MaterialConditionModifier mod) {
        if (d.empty()) {
            return;
        }
        std::string txt = d;
        const std::string modStr = materialModifierSymbolString(mod);
        if (!modStr.empty()) {
            txt += " " + modStr;
        }
        const double w = std::max(7.0, static_cast<double>(txt.size()) * 2.4 + 3.0);
        datums.push_back({txt, w});
    };
    pushDatum(spec.datumA, spec.datumAModifier);
    pushDatum(spec.datumB, spec.datumBModifier);
    pushDatum(spec.datumC, spec.datumCModifier);

    double totalW = symCellW + tolCellW;
    for (const auto& dc : datums) {
        totalW += dc.width;
    }

    const double pad = 1.0;
    const double svgW = totalW + 2.0 * pad;
    const double svgH = rowH + 2.0 * pad;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << svgW << "mm\" height=\"" << svgH
        << "mm\" viewBox=\"0 0 " << svgW << " " << svgH << "\">\n"
        << "  <g stroke=\"#000000\" stroke-width=\"0.35\" fill=\"none\">\n"
        << "    <rect x=\"" << pad << "\" y=\"" << pad << "\" width=\"" << totalW << "\" height=\""
        << rowH << "\" fill=\"#ffffff\"/>\n";

    double curX = pad + symCellW;
    svg << "    <line x1=\"" << curX << "\" y1=\"" << pad << "\" x2=\"" << curX << "\" y2=\""
        << (pad + rowH) << "\"/>\n";

    const double textY = pad + rowH * 0.72;
    svg << "  </g>\n"
        << "  <text x=\"" << (pad + symCellW * 0.5) << "\" y=\"" << textY
        << "\" font-family=\"osifont, sans-serif\" font-size=\"4.5\" text-anchor=\"middle\" "
           "fill=\"#000000\">"
        << xmlEscape(characteristicSymbolString(spec.characteristic)) << "</text>\n"
        << "  <text x=\"" << (curX + tolCellW * 0.5) << "\" y=\"" << textY
        << "\" font-family=\"osifont, sans-serif\" font-size=\"3.8\" text-anchor=\"middle\" "
           "fill=\"#000000\">"
        << xmlEscape(tolText) << "</text>\n";

    curX += tolCellW;
    for (const auto& dc : datums) {
        svg << "  <line x1=\"" << curX << "\" y1=\"" << pad << "\" x2=\"" << curX << "\" y2=\""
            << (pad + rowH) << "\" stroke=\"#000000\" stroke-width=\"0.35\"/>\n"
            << "  <text x=\"" << (curX + dc.width * 0.5) << "\" y=\"" << textY
            << "\" font-family=\"osifont, sans-serif\" font-size=\"3.8\" font-weight=\"bold\" "
               "text-anchor=\"middle\" fill=\"#000000\">"
            << xmlEscape(dc.text) << "</text>\n";
        curX += dc.width;
    }

    svg << "</svg>\n";
    return svg.str();
}

TechDraw::DrawViewSymbol* DraftingTools::createDatumSymbolOnPage(
    TechDraw::DrawPage* page,
    const DatumSymbolSpec& spec,
    double pageX,
    double pageY,
    App::DocumentObject* ownerView
)
{
    if (!page || !page->getDocument()) {
        return nullptr;
    }
    App::Document* doc = page->getDocument();
    auto* sym = doc->addObject<TechDraw::DrawViewSymbol>("DatumSymbol");
    if (!sym) {
        return nullptr;
    }

    const std::string letter = spec.letter.empty() ? "A" : spec.letter;
    sym->Label.setValue("Datum " + letter);
    sym->Symbol.setValue(buildDatumFeatureSvg(spec));
    sym->X.setValue(pageX);
    sym->Y.setValue(pageY);
    if (ownerView) {
        sym->Owner.setValue(ownerView);
    }

    if (auto* pLetter = ensureDynamicProp<App::PropertyString>(
            sym,
            "App::PropertyString",
            "DatumLetter",
            "Datum Symbol",
            "Datum feature identification letter"
        )) {
        pLetter->setValue(letter);
    }
    if (auto* pFilled = ensureDynamicProp<App::PropertyBool>(
            sym,
            "App::PropertyBool",
            "FilledTriangle",
            "Datum Symbol",
            "Filled datum triangle"
        )) {
        pFilled->setValue(spec.filledTriangle);
    }

    page->addView(sym, false);
    sym->recomputeFeature();
    return sym;
}

TechDraw::DrawViewSymbol* DraftingTools::createGdtFrameOnPage(
    TechDraw::DrawPage* page,
    const FeatureControlFrameSpec& spec,
    double pageX,
    double pageY,
    App::DocumentObject* ownerView
)
{
    if (!page || !page->getDocument()) {
        return nullptr;
    }
    App::Document* doc = page->getDocument();
    auto* sym = doc->addObject<TechDraw::DrawViewSymbol>("GDTFrame");
    if (!sym) {
        return nullptr;
    }

    sym->Label.setValue("GD&T " + characteristicSymbolString(spec.characteristic));
    sym->Symbol.setValue(buildFeatureControlFrameSvg(spec));
    sym->X.setValue(pageX);
    sym->Y.setValue(pageY);
    if (ownerView) {
        sym->Owner.setValue(ownerView);
    }

    if (auto* pChar = ensureDynamicProp<App::PropertyString>(
            sym,
            "App::PropertyString",
            "GdtCharacteristicSymbol",
            "GD&T Frame",
            "Geometric characteristic symbol"
        )) {
        pChar->setValue(characteristicSymbolString(spec.characteristic));
    }
    if (auto* pDia = ensureDynamicProp<App::PropertyBool>(
            sym,
            "App::PropertyBool",
            "DiameterZone",
            "GD&T Frame",
            "Cylindrical diameter tolerance zone"
        )) {
        pDia->setValue(spec.diameterZone);
    }
    if (auto* pTol = ensureDynamicProp<App::PropertyString>(
            sym,
            "App::PropertyString",
            "ToleranceValue",
            "GD&T Frame",
            "Tolerance zone width"
        )) {
        pTol->setValue(spec.toleranceValue);
    }
    if (auto* pDA = ensureDynamicProp<App::PropertyString>(
            sym,
            "App::PropertyString",
            "DatumA",
            "GD&T Frame",
            "Primary datum reference"
        )) {
        pDA->setValue(spec.datumA);
    }
    if (auto* pDB = ensureDynamicProp<App::PropertyString>(
            sym,
            "App::PropertyString",
            "DatumB",
            "GD&T Frame",
            "Secondary datum reference"
        )) {
        pDB->setValue(spec.datumB);
    }
    if (auto* pDC = ensureDynamicProp<App::PropertyString>(
            sym,
            "App::PropertyString",
            "DatumC",
            "GD&T Frame",
            "Tertiary datum reference"
        )) {
        pDC->setValue(spec.datumC);
    }

    page->addView(sym, false);
    sym->recomputeFeature();
    return sym;
}

//===========================================================================
// Parametric Hole & Thread Callout
//===========================================================================

std::string DraftingTools::formatHoleCalloutText(const HoleCalloutInfo& info)
{
    std::ostringstream out;
    if (info.count > 1) {
        out << info.count << "X ";
    }

    if (info.threaded && !info.threadDesignation.empty()) {
        out << info.threadDesignation;
    }
    else {
        out << "Ø" << formatCleanNumber(std::max(0.0, info.diameterMm));
    }

    if (info.throughAll) {
        out << " THRU";
    }
    else if (info.depthMm > 0.0) {
        out << " ↧ " << formatCleanNumber(info.depthMm);
    }

    if (info.hasCounterbore && info.counterboreDiameterMm > 0.0) {
        out << " | ⌴ Ø" << formatCleanNumber(info.counterboreDiameterMm);
        if (info.counterboreDepthMm > 0.0) {
            out << " ↧ " << formatCleanNumber(info.counterboreDepthMm);
        }
    }
    else if (info.hasCountersink && info.countersinkDiameterMm > 0.0) {
        out << " | ⌵ Ø" << formatCleanNumber(info.countersinkDiameterMm) << " × "
            << formatCleanNumber(info.countersinkAngleDeg, 0) << "°";
    }

    return out.str();
}

HoleCalloutInfo DraftingTools::extractHoleCalloutFromFeature(const App::DocumentObject* holeFeature)
{
    HoleCalloutInfo info;
    if (!holeFeature) {
        return info;
    }

    info.count = countHoleInstancesInProfile(holeFeature);
    info.diameterMm = getFloatProp(holeFeature, "Diameter", 6.0);
    info.threaded = getBoolProp(holeFeature, "Threaded", false);

    const std::string depthType = getEnumOrStringProp(holeFeature, "DepthType");
    info.throughAll = (depthType == "ThroughAll" || depthType.empty());
    info.depthMm = getFloatProp(holeFeature, "Depth", 0.0);

    if (info.threaded) {
        std::string threadSize = getEnumOrStringProp(holeFeature, "ThreadSize");
        const std::string threadClass = getEnumOrStringProp(holeFeature, "ThreadClass");
        if (threadSize.empty() || threadSize == "None") {
            const double td = getFloatProp(holeFeature, "ThreadDiameter", info.diameterMm);
            threadSize = "M" + formatCleanNumber(td);
        }
        if (!threadClass.empty() && threadClass != "None") {
            threadSize += " - " + threadClass;
        }
        info.threadDesignation = threadSize;

        const std::string threadDepthType = getEnumOrStringProp(holeFeature, "ThreadDepthType");
        if (threadDepthType == "Dimension") {
            info.threadDepthMm = getFloatProp(holeFeature, "ThreadDepth", info.depthMm);
        }
    }

    const std::string cutType = getEnumOrStringProp(holeFeature, "HoleCutType");
    if (cutType == "Counterbore") {
        info.hasCounterbore = true;
        info.counterboreDiameterMm = getFloatProp(holeFeature, "HoleCutDiameter", 0.0);
        info.counterboreDepthMm = getFloatProp(holeFeature, "HoleCutDepth", 0.0);
    }
    else if (cutType == "Countersink" || cutType == "Counterdrill") {
        info.hasCountersink = true;
        info.countersinkDiameterMm = getFloatProp(holeFeature, "HoleCutDiameter", 0.0);
        info.countersinkAngleDeg = getFloatProp(holeFeature, "HoleCutCountersinkAngle", 90.0);
    }

    info.formattedCallout = formatHoleCalloutText(info);
    return info;
}

HoleCalloutInfo DraftingTools::inspectViewForHoleCallout(
    const TechDraw::DrawViewPart* viewPart,
    const std::string& subElementName
)
{
    if (!viewPart) {
        return {};
    }

    // 1. Search the 3D source object hierarchy for a PartDesign::Hole feature
    std::set<const App::DocumentObject*> visited;
    for (const App::DocumentObject* src : viewPart->getAllSources()) {
        if (const App::DocumentObject* holeObj = findHoleFeatureRecursive(src, visited)) {
            return extractHoleCalloutFromFeature(holeObj);
        }
    }

    // 2. Fallback: inspect the clicked 2D circular/arc edge on the DrawViewPart
    HoleCalloutInfo info;
    info.throughAll = true;
    const double viewScale = std::max(1e-6, viewPart->getScale());

    TechDraw::BaseGeomPtr targetGeom;
    if (!subElementName.empty()) {
        targetGeom = const_cast<TechDraw::DrawViewPart*>(viewPart)->getEdge(subElementName);
    }
    if (!targetGeom) {
        for (const auto& geom : viewPart->getEdgeGeometry()) {
            if (geom && (geom->getGeomType() == TechDraw::GeomType::CIRCLE
                         || geom->getGeomType() == TechDraw::GeomType::ARCOFCIRCLE)) {
                targetGeom = geom;
                break;
            }
        }
    }

    if (auto circleGeom = std::dynamic_pointer_cast<TechDraw::Circle>(targetGeom)) {
        const double radiusModelMm = circleGeom->radius / viewScale;
        info.diameterMm = radiusModelMm * 2.0;

        // Count coaxial or identical-radius circles in the same view
        int sameRadiusCount = 0;
        for (const auto& geom : viewPart->getEdgeGeometry()) {
            if (auto c = std::dynamic_pointer_cast<TechDraw::Circle>(geom)) {
                if (std::abs(c->radius - circleGeom->radius) < 1e-2) {
                    ++sameRadiusCount;
                }
            }
        }
        info.count = std::max(1, sameRadiusCount);
    }
    else {
        info.diameterMm = 10.0;
    }

    info.formattedCallout = formatHoleCalloutText(info);
    return info;
}

TechDraw::DrawViewBalloon* DraftingTools::createHoleCalloutOnPage(
    TechDraw::DrawPage* page,
    TechDraw::DrawViewPart* viewPart,
    const std::string& subElementName,
    double originViewMmX,
    double originViewMmY,
    double bubbleViewMmX,
    double bubbleViewMmY
)
{
    if (!page || !viewPart || !page->getDocument()) {
        return nullptr;
    }

    const HoleCalloutInfo info = inspectViewForHoleCallout(viewPart, subElementName);
    App::Document* doc = page->getDocument();

    auto* callout = doc->addObject<TechDraw::DrawViewBalloon>("HoleCallout");
    if (!callout) {
        return nullptr;
    }

    callout->Label.setValue("Hole Callout");
    callout->SourceView.setValue(viewPart);
    callout->BubbleShape.setValue("None");
    callout->EndType.setValue("Filled Arrow");
    callout->OriginX.setValue(originViewMmX);
    callout->OriginY.setValue(originViewMmY);
    callout->X.setValue(bubbleViewMmX);
    callout->Y.setValue(bubbleViewMmY);
    callout->Text.setValue(info.formattedCallout);

    if (auto* pType = ensureDynamicProp<App::PropertyString>(
            callout,
            "App::PropertyString",
            "CalloutType",
            "Hole Callout",
            "Parametric callout type"
        )) {
        pType->setValue("HoleCallout");
    }

    page->addView(callout, false);
    callout->recomputeFeature();
    return callout;
}

//===========================================================================
// Assembly BOM Table, Linked Balloons & Magnetic Lines
//===========================================================================

std::vector<BomRowEntry> DraftingTools::collectBomEntries(
    App::Document* doc,
    App::DocumentObject* sourceOrBom
)
{
    std::vector<BomRowEntry> rows;
    if (!doc) {
        return rows;
    }

    // 1. Check if sourceOrBom (or an object in doc) is already a Spreadsheet::Sheet / Assembly::BomObject
    Spreadsheet::Sheet* bomSheet = dynamic_cast<Spreadsheet::Sheet*>(sourceOrBom);
    if (!bomSheet) {
        const Base::Type asmBomType = Base::Type::fromName("Assembly::BomObject");
        if (!asmBomType.isBad()) {
            auto bomObjs = doc->getObjectsOfType(asmBomType);
            if (!bomObjs.empty()) {
                bomSheet = dynamic_cast<Spreadsheet::Sheet*>(bomObjs.front());
            }
        }
    }

    if (bomSheet) {
        auto readCellText = [bomSheet](int row, int col) -> std::string {
            const Spreadsheet::Cell* cell = bomSheet->getCell(App::CellAddress(row, col));
            if (!cell) {
                return {};
            }
            std::string val;
            if (cell->getStringContent(val)) {
                if (!val.empty() && val.front() == '\'') {
                    val.erase(0, 1);
                }
                return val;
            }
            return {};
        };

        int idxCol = 0;
        int nameCol = 1;
        int descCol = 2;
        int qtyCol = -1;
        for (int c = 0; c < 10; ++c) {
            const std::string hdr = readCellText(0, c);
            if (hdr.empty()) {
                continue;
            }
            if (hdr == "Index" || hdr == "Item" || hdr == "ITEM") {
                idxCol = c;
            }
            else if (hdr == "Name" || hdr == "Part Number" || hdr == "PART NUMBER") {
                nameCol = c;
            }
            else if (hdr == "Description" || hdr == "DESCRIPTION") {
                descCol = c;
            }
            else if (hdr == "Quantity" || hdr == "QTY" || hdr == "Qty") {
                qtyCol = c;
            }
        }

        for (int r = 1; r < 200; ++r) {
            const std::string itemStr = readCellText(r, idxCol);
            const std::string nameStr = readCellText(r, nameCol);
            if (itemStr.empty() && nameStr.empty()) {
                break;
            }
            BomRowEntry entry;
            entry.itemNumber = itemStr.empty() ? std::to_string(r) : itemStr;
            entry.partNumber = nameStr;
            entry.description = (descCol >= 0) ? readCellText(r, descCol) : nameStr;
            if (qtyCol >= 0) {
                try {
                    entry.quantity = std::max(1, std::stoi(readCellText(r, qtyCol)));
                }
                catch (...) {
                    entry.quantity = 1;
                }
            }
            for (App::DocumentObject* obj : doc->getObjects()) {
                if (obj && (obj->Label.getStrValue() == nameStr || obj->getNameInDocument() == nameStr)) {
                    entry.sourceObject = obj;
                    entry.material = extractObjectMaterial(obj);
                    break;
                }
            }
            rows.push_back(std::move(entry));
        }
        if (!rows.empty()) {
            return rows;
        }
    }

    // 2. Otherwise inspect 3D hierarchy (Assembly container, App::Part, or top-level Part::Features)
    std::vector<App::DocumentObject*> candidates;
    if (sourceOrBom && !sourceOrBom->isDerivedFrom<TechDraw::DrawPage>()
        && !sourceOrBom->isDerivedFrom<TechDraw::DrawView>()) {
        if (auto* groupProp = dynamic_cast<App::PropertyLinkList*>(sourceOrBom->getPropertyByName("Group"))) {
            for (App::DocumentObject* child : groupProp->getValues()) {
                if (child && (child->isDerivedFrom<App::Part>() || child->isDerivedFrom<Part::Feature>()
                              || child->isDerivedFrom<App::Link>())) {
                    candidates.push_back(child);
                }
            }
        }
        if (candidates.empty()) {
            candidates.push_back(sourceOrBom);
        }
    }
    else {
        for (App::DocumentObject* obj : doc->getObjects()) {
            if (!obj || obj->isDerivedFrom<TechDraw::DrawPage>()
                || obj->isDerivedFrom<TechDraw::DrawView>()
                || obj->isDerivedFrom<TechDraw::DrawTemplate>()
                || obj->isDerivedFrom<Spreadsheet::Sheet>()) {
                continue;
            }
            if (obj->isDerivedFrom<App::Part>() || obj->isDerivedFrom<Part::Feature>()
                || obj->isDerivedFrom<App::Link>()) {
                // Skip internal helper BreakSpan lines
                if (std::string(obj->getNameInDocument()).rfind("BreakSpan", 0) == 0) {
                    continue;
                }
                candidates.push_back(obj);
            }
        }
    }

    for (App::DocumentObject* rawObj : candidates) {
        App::DocumentObject* target = unwrapLinkTarget(rawObj);
        if (!target) {
            continue;
        }
        auto it = std::find_if(rows.begin(), rows.end(), [target](const BomRowEntry& e) {
            return e.sourceObject == target;
        });
        if (it != rows.end()) {
            it->quantity += 1;
            continue;
        }

        BomRowEntry entry;
        entry.itemNumber = std::to_string(rows.size() + 1);
        entry.partNumber = target->Label.getStrValue();
        entry.description = extractObjectDescription(target);
        entry.material = extractObjectMaterial(target);
        entry.quantity = 1;
        entry.sourceObject = target;
        rows.push_back(std::move(entry));
    }

    return rows;
}

TechDraw::DrawViewSpreadsheet* DraftingTools::createOrUpdateBomTableOnPage(
    TechDraw::DrawPage* page,
    App::DocumentObject* sourceOrBom
)
{
    if (!page || !page->getDocument()) {
        return nullptr;
    }

    App::Document* doc = page->getDocument();
    const std::vector<BomRowEntry> rows = collectBomEntries(doc, sourceOrBom);

    // Find or create a backing Spreadsheet::Sheet for the BOM
    Spreadsheet::Sheet* bomSheet = dynamic_cast<Spreadsheet::Sheet*>(sourceOrBom);
    if (!bomSheet) {
        for (App::DocumentObject* obj : doc->getObjectsOfType(Spreadsheet::Sheet::getClassTypeId())) {
            if (obj && std::string(obj->getNameInDocument()) == "BOM_Spreadsheet") {
                bomSheet = dynamic_cast<Spreadsheet::Sheet*>(obj);
                break;
            }
        }
    }
    if (!bomSheet) {
        bomSheet = doc->addObject<Spreadsheet::Sheet>("BOM_Spreadsheet");
        if (!bomSheet) {
            return nullptr;
        }
        bomSheet->Label.setValue("Bill of Materials");
    }

    // Populate clean engineering BOM columns: ITEM | PART NUMBER | DESCRIPTION | MATERIAL | QTY
    bomSheet->clearAll();
    bomSheet->setCell(App::CellAddress(0, 0), "'ITEM");
    bomSheet->setCell(App::CellAddress(0, 1), "'PART NUMBER");
    bomSheet->setCell(App::CellAddress(0, 2), "'DESCRIPTION");
    bomSheet->setCell(App::CellAddress(0, 3), "'MATERIAL");
    bomSheet->setCell(App::CellAddress(0, 4), "'QTY");

    int lastRow = 1;
    if (rows.empty()) {
        bomSheet->setCell(App::CellAddress(1, 0), "'1");
        bomSheet->setCell(App::CellAddress(1, 1), ("'" + doc->Label.getStrValue()).c_str());
        bomSheet->setCell(App::CellAddress(1, 2), "'Part");
        bomSheet->setCell(App::CellAddress(1, 3), "'-");
        bomSheet->setCell(App::CellAddress(1, 4), "'1");
        lastRow = 2;
    }
    else {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const int r = static_cast<int>(i) + 1;
            const auto& row = rows[i];
            bomSheet->setCell(App::CellAddress(r, 0), ("'" + row.itemNumber).c_str());
            bomSheet->setCell(App::CellAddress(r, 1), ("'" + row.partNumber).c_str());
            bomSheet->setCell(
                App::CellAddress(r, 2),
                ("'" + (row.description.empty() ? row.partNumber : row.description)).c_str()
            );
            bomSheet->setCell(
                App::CellAddress(r, 3),
                ("'" + (row.material.empty() ? std::string("-") : row.material)).c_str()
            );
            bomSheet->setCell(App::CellAddress(r, 4), ("'" + std::to_string(row.quantity)).c_str());
            lastRow = r + 1;
        }
    }
    bomSheet->recomputeFeature();

    // Find or create the DrawViewSpreadsheet on this page
    TechDraw::DrawViewSpreadsheet* tableView = nullptr;
    for (App::DocumentObject* v : page->Views.getValues()) {
        if (auto* sv = dynamic_cast<TechDraw::DrawViewSpreadsheet*>(v)) {
            if (sv->Source.getValue() == bomSheet) {
                tableView = sv;
                break;
            }
        }
    }

    if (!tableView) {
        tableView = doc->addObject<TechDraw::DrawViewSpreadsheet>("BOMTable");
        if (!tableView) {
            return nullptr;
        }
        tableView->Label.setValue("Bill of Materials");
        tableView->Source.setValue(bomSheet);
        tableView->TextSize.setValue(3.5);
        tableView->LineWidth.setValue(0.35);

        // Snap to top-left or above-title-block corner of the sheet
        double pageW = 420.0;
        double pageH = 297.0;
        if (page->Template.getValue()) {
            pageW = page->getPageWidth();
            pageH = page->getPageHeight();
        }
        tableView->X.setValue(std::max(70.0, pageW - 110.0));
        tableView->Y.setValue(std::max(45.0, pageH - 35.0));
        page->addView(tableView, false);
    }

    tableView->CellStart.setValue("A1");
    tableView->CellEnd.setValue("E" + std::to_string(std::max(2, lastRow)));
    tableView->recomputeFeature();
    return tableView;
}

std::string DraftingTools::resolveBomItemNumber(
    App::Document* doc,
    const App::DocumentObject* componentOrView,
    const std::vector<BomRowEntry>& entries
)
{
    if (!doc || !componentOrView) {
        return "1";
    }

    const std::vector<BomRowEntry> bom = entries.empty() ? collectBomEntries(doc) : entries;
    if (bom.empty()) {
        return "1";
    }

    const App::DocumentObject* target = componentOrView;
    if (auto* dvp = dynamic_cast<const TechDraw::DrawViewPart*>(componentOrView)) {
        const auto srcs = dvp->getAllSources();
        if (!srcs.empty() && srcs.front()) {
            target = srcs.front();
        }
    }
    if (auto* link = dynamic_cast<const App::Link*>(target)) {
        if (const App::DocumentObject* linked = link->getLinkedObject(true)) {
            target = linked;
        }
    }

    for (const auto& row : bom) {
        if (row.sourceObject == target || row.partNumber == target->Label.getStrValue()
            || row.partNumber == target->getNameInDocument()) {
            return row.itemNumber;
        }
    }
    return bom.front().itemNumber;
}

std::vector<TechDraw::DrawViewBalloon*> DraftingTools::createAutoBalloonsForView(
    TechDraw::DrawPage* page,
    TechDraw::DrawViewPart* viewPart
)
{
    std::vector<TechDraw::DrawViewBalloon*> created;
    if (!page || !viewPart || !page->getDocument()) {
        return created;
    }

    App::Document* doc = page->getDocument();
    const auto sources = viewPart->getAllSources();
    App::DocumentObject* singleContainer = (sources.size() == 1) ? sources.front() : nullptr;

    // If a BOM_Spreadsheet or Assembly::BomObject already exists in doc, prefer it;
    // otherwise collect across all view sources (or single container hierarchy).
    Spreadsheet::Sheet* existingBom = nullptr;
    for (App::DocumentObject* obj : doc->getObjectsOfType(Spreadsheet::Sheet::getClassTypeId())) {
        if (obj && std::string(obj->getNameInDocument()) == "BOM_Spreadsheet") {
            existingBom = dynamic_cast<Spreadsheet::Sheet*>(obj);
            break;
        }
    }

    std::vector<BomRowEntry> bom = collectBomEntries(
        doc,
        existingBom ? static_cast<App::DocumentObject*>(existingBom) : singleContainer
    );
    if (bom.empty() && !sources.empty() && sources.front()) {
        BomRowEntry single;
        single.itemNumber = "1";
        single.partNumber = sources.front()->Label.getStrValue();
        single.sourceObject = sources.front();
        bom.push_back(single);
    }
    if (bom.empty()) {
        return created;
    }

    const double scale = std::max(1e-6, viewPart->getScale());
    const double halfW = std::max(12.0, (viewPart->getBoxX() / scale) * 0.5);
    const double halfH = std::max(12.0, (viewPart->getBoxY() / scale) * 0.5);
    const double margin = 14.0;

    // Collect candidate anchor vertices around the view perimeter
    const auto& verts = viewPart->getVertexGeometry();

    const std::size_t count = bom.size();
    for (std::size_t i = 0; i < count; ++i) {
        const double angle = (2.0 * std::numbers::pi * static_cast<double>(i))
            / static_cast<double>(std::max<std::size_t>(1, count))
            + (std::numbers::pi / 4.0);

        double originX = std::cos(angle) * (halfW * 0.55);
        double originY = std::sin(angle) * (halfH * 0.55);
        if (!verts.empty()) {
            const auto& v = verts[i % verts.size()];
            if (v) {
                originX = v->x() / scale;
                // TechDraw vertex geometry has inverted Y
                originY = -v->y() / scale;
            }
        }

        const double bubbleX = std::cos(angle) * (halfW + margin);
        const double bubbleY = std::sin(angle) * (halfH + margin);

        auto* balloon = doc->addObject<TechDraw::DrawViewBalloon>("Balloon");
        if (!balloon) {
            continue;
        }
        balloon->Label.setValue("Balloon " + bom[i].itemNumber);
        balloon->SourceView.setValue(viewPart);
        balloon->Text.setValue(bom[i].itemNumber);
        balloon->BubbleShape.setValue("Circular");
        balloon->OriginX.setValue(originX);
        balloon->OriginY.setValue(originY);
        balloon->X.setValue(bubbleX);
        balloon->Y.setValue(bubbleY);

        if (auto* pItem = ensureDynamicProp<App::PropertyString>(
                balloon,
                "App::PropertyString",
                "BomItemNumber",
                "Balloon",
                "Linked BOM Item Number"
            )) {
            pItem->setValue(bom[i].itemNumber);
        }

        page->addView(balloon, false);
        balloon->recomputeFeature();
        created.push_back(balloon);
    }

    if (created.size() > 1) {
        // Snap auto-balloons onto a neat horizontal magnetic line above the view
        alignBalloonsOnMagneticLine(
            created,
            Base::Vector3d(-halfW, halfH + margin, 0.0),
            Base::Vector3d(halfW, halfH + margin, 0.0)
        );
    }

    return created;
}

void DraftingTools::alignBalloonsOnMagneticLine(
    const std::vector<TechDraw::DrawViewBalloon*>& balloons,
    const Base::Vector3d& startViewMm,
    const Base::Vector3d& endViewMm
)
{
    if (balloons.empty()) {
        return;
    }

    if (balloons.size() == 1) {
        balloons.front()->X.setValue(startViewMm.x);
        balloons.front()->Y.setValue(startViewMm.y);
        return;
    }

    // Sort balloons along the magnetic line direction so leaders don't cross needlessly
    const Base::Vector3d lineVec = endViewMm - startViewMm;
    std::vector<TechDraw::DrawViewBalloon*> ordered = balloons;
    std::sort(
        ordered.begin(),
        ordered.end(),
        [&startViewMm, &lineVec](TechDraw::DrawViewBalloon* a, TechDraw::DrawViewBalloon* b) {
            if (!a || !b) {
                return a < b;
            }
            const Base::Vector3d pa(a->OriginX.getValue(), a->OriginY.getValue(), 0.0);
            const Base::Vector3d pb(b->OriginX.getValue(), b->OriginY.getValue(), 0.0);
            return ((pa - startViewMm) * lineVec) < ((pb - startViewMm) * lineVec);
        }
    );

    const std::size_t n = ordered.size();
    for (std::size_t i = 0; i < n; ++i) {
        TechDraw::DrawViewBalloon* b = ordered[i];
        if (!b) {
            continue;
        }
        const double t = static_cast<double>(i) / static_cast<double>(n - 1);
        const Base::Vector3d pos = startViewMm + lineVec * t;
        b->X.setValue(pos.x);
        b->Y.setValue(pos.y);

        if (auto* pStart = ensureDynamicProp<App::PropertyVector>(
                b,
                "App::PropertyVector",
                "MagneticLineStart",
                "Magnetic Line",
                "Start point of magnetic balloon alignment line (view mm)"
            )) {
            pStart->setValue(startViewMm);
        }
        if (auto* pEnd = ensureDynamicProp<App::PropertyVector>(
                b,
                "App::PropertyVector",
                "MagneticLineEnd",
                "Magnetic Line",
                "End point of magnetic balloon alignment line (view mm)"
            )) {
            pEnd->setValue(endViewMm);
        }
        b->recomputeFeature();
    }
}

}  // namespace Drawing
