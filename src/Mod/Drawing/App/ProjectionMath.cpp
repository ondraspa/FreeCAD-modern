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
#include <filesystem>
#include <numbers>

#include <App/Application.h>
#include <App/PropertyGeo.h>
#include <App/PropertyLinks.h>
#include <App/PropertyStandard.h>
#include <Base/UnitsApi.h>
#include <Mod/TechDraw/App/DrawView.h>

#include "ProjectionMath.h"

namespace Drawing
{

namespace
{

constexpr const char* kAlignmentGroup = "Alignment";
constexpr const char* kPropParentView = "ParentView";
constexpr const char* kPropAlignmentMode = "AlignmentMode";
constexpr const char* kPropSavedAlignmentMode = "SavedAlignmentMode";
constexpr const char* kPropAlignmentVector = "AlignmentVector";

Base::Vector3d safeNormalize(const Base::Vector3d& v, const Base::Vector3d& fallback)
{
    const double len = v.Length();
    if (len < 1e-9) {
        return fallback;
    }
    return v / len;
}

void orthonormalizeBasis(
    const Base::Vector3d& rawDir,
    const Base::Vector3d& rawX,
    Base::Vector3d& outD,
    Base::Vector3d& outX,
    Base::Vector3d& outY
)
{
    outD = safeNormalize(rawDir, Base::Vector3d(0.0, -1.0, 0.0));

    // Remove component of rawX along outD so outX is strictly perpendicular to outD.
    Base::Vector3d xProj = rawX - outD * (rawX * outD);
    if (xProj.Length() < 1e-6) {
        const Base::Vector3d ref = (std::abs(outD.z) < 0.9) ? Base::Vector3d(0.0, 0.0, 1.0)
                                                             : Base::Vector3d(1.0, 0.0, 0.0);
        xProj = ref % outD;
    }
    outX = safeNormalize(xProj, Base::Vector3d(1.0, 0.0, 0.0));
    // Matches OCCT gp_Ax2(org, D, X) right-handed up vector: Y = D x X.
    outY = safeNormalize(outD % outX, Base::Vector3d(0.0, 0.0, 1.0));
}

template<typename PropT>
PropT* ensureDynamicProperty(
    TechDraw::DrawView* view,
    const char* typeName,
    const char* propName,
    const char* doc
)
{
    if (!view) {
        return nullptr;
    }
    if (auto* existing = dynamic_cast<PropT*>(view->getPropertyByName(propName))) {
        return existing;
    }
    App::Property* created = view->addDynamicProperty(typeName, propName, kAlignmentGroup, doc);
    return dynamic_cast<PropT*>(created);
}

}  // namespace

ViewOrientation ProjectionMath::standardOrientation(StandardOrientation orient)
{
    switch (orient) {
        case StandardOrientation::Front:
            return {Base::Vector3d(0.0, -1.0, 0.0), Base::Vector3d(1.0, 0.0, 0.0), "Front"};
        case StandardOrientation::Back:
            return {Base::Vector3d(0.0, 1.0, 0.0), Base::Vector3d(-1.0, 0.0, 0.0), "Back"};
        case StandardOrientation::Top:
            return {Base::Vector3d(0.0, 0.0, 1.0), Base::Vector3d(1.0, 0.0, 0.0), "Top"};
        case StandardOrientation::Bottom:
            return {Base::Vector3d(0.0, 0.0, -1.0), Base::Vector3d(1.0, 0.0, 0.0), "Bottom"};
        case StandardOrientation::Left:
            return {Base::Vector3d(-1.0, 0.0, 0.0), Base::Vector3d(0.0, -1.0, 0.0), "Left"};
        case StandardOrientation::Right:
            return {Base::Vector3d(1.0, 0.0, 0.0), Base::Vector3d(0.0, 1.0, 0.0), "Right"};
        case StandardOrientation::Isometric: {
            const Base::Vector3d d = safeNormalize(Base::Vector3d(1.0, -1.0, 1.0), Base::Vector3d(1.0, -1.0, 1.0));
            const Base::Vector3d x = safeNormalize(Base::Vector3d(1.0, 1.0, 0.0), Base::Vector3d(1.0, 0.0, 0.0));
            return {d, x, "Isometric"};
        }
    }
    return {Base::Vector3d(0.0, -1.0, 0.0), Base::Vector3d(1.0, 0.0, 0.0), "Front"};
}

ProjectionDirection8 ProjectionMath::classifyDirection8(double pageDx, double pageDy)
{
    // atan2 returns radians in [-pi, pi], 0 = East, +pi/2 = North (+Y up).
    double angleDeg = std::atan2(pageDy, pageDx) * 180.0 / std::numbers::pi;
    if (angleDeg < 0.0) {
        angleDeg += 360.0;
    }

    const int sector = static_cast<int>(std::floor((angleDeg + 22.5) / 45.0)) % 8;
    switch (sector) {
        case 0:
            return ProjectionDirection8::East;
        case 1:
            return ProjectionDirection8::NorthEast;
        case 2:
            return ProjectionDirection8::North;
        case 3:
            return ProjectionDirection8::NorthWest;
        case 4:
            return ProjectionDirection8::West;
        case 5:
            return ProjectionDirection8::SouthWest;
        case 6:
            return ProjectionDirection8::South;
        case 7:
            return ProjectionDirection8::SouthEast;
        default:
            return ProjectionDirection8::East;
    }
}

ProjectedViewSpec ProjectionMath::computeProjectedView(
    const Base::Vector3d& parentDir,
    const Base::Vector3d& parentXDir,
    ProjectionDirection8 dir8,
    bool thirdAngle
)
{
    Base::Vector3d d;
    Base::Vector3d x;
    Base::Vector3d y;
    orthonormalizeBasis(parentDir, parentXDir, d, x, y);

    // Map drag direction to logical projection side depending on 3rd-angle (ASME) vs 1st-angle (ISO).
    // In Third Angle, dragging right unfolds the Right side view; in First Angle, dragging right projects the Left side view.
    const double s = thirdAngle ? 1.0 : -1.0;

    constexpr double kInvSqrt2 = 0.70710678118654752440;
    ProjectedViewSpec spec;

    switch (dir8) {
        case ProjectionDirection8::East:
            spec.direction = x * s;
            spec.xDirection = d * (-s);
            spec.alignment = AlignmentKind::Horizontal;
            spec.alignmentVector = Base::Vector3d(1.0, 0.0, 0.0);
            spec.label = thirdAngle ? "Right" : "Left";
            break;

        case ProjectionDirection8::West:
            spec.direction = x * (-s);
            spec.xDirection = d * s;
            spec.alignment = AlignmentKind::Horizontal;
            spec.alignmentVector = Base::Vector3d(-1.0, 0.0, 0.0);
            spec.label = thirdAngle ? "Left" : "Right";
            break;

        case ProjectionDirection8::North:
            spec.direction = y * s;
            spec.xDirection = x;
            spec.alignment = AlignmentKind::Vertical;
            spec.alignmentVector = Base::Vector3d(0.0, 1.0, 0.0);
            spec.label = thirdAngle ? "Top" : "Bottom";
            break;

        case ProjectionDirection8::South:
            spec.direction = y * (-s);
            spec.xDirection = x;
            spec.alignment = AlignmentKind::Vertical;
            spec.alignmentVector = Base::Vector3d(0.0, -1.0, 0.0);
            spec.label = thirdAngle ? "Bottom" : "Top";
            break;

        case ProjectionDirection8::NorthEast:
            spec.direction = safeNormalize(d + x * s + y * s, d);
            spec.xDirection = safeNormalize(x - d * s, x);
            spec.alignment = AlignmentKind::Vector;
            spec.alignmentVector = Base::Vector3d(kInvSqrt2, kInvSqrt2, 0.0);
            spec.label = "Iso NE";
            break;

        case ProjectionDirection8::NorthWest:
            spec.direction = safeNormalize(d - x * s + y * s, d);
            spec.xDirection = safeNormalize(x + d * s, x);
            spec.alignment = AlignmentKind::Vector;
            spec.alignmentVector = Base::Vector3d(-kInvSqrt2, kInvSqrt2, 0.0);
            spec.label = "Iso NW";
            break;

        case ProjectionDirection8::SouthWest:
            spec.direction = safeNormalize(d - x * s - y * s, d);
            spec.xDirection = safeNormalize(x + d * s, x);
            spec.alignment = AlignmentKind::Vector;
            spec.alignmentVector = Base::Vector3d(-kInvSqrt2, -kInvSqrt2, 0.0);
            spec.label = "Iso SW";
            break;

        case ProjectionDirection8::SouthEast:
            spec.direction = safeNormalize(d + x * s - y * s, d);
            spec.xDirection = safeNormalize(x - d * s, x);
            spec.alignment = AlignmentKind::Vector;
            spec.alignmentVector = Base::Vector3d(kInvSqrt2, -kInvSqrt2, 0.0);
            spec.label = "Iso SE";
            break;
    }

    // Ensure XDirection is strictly orthogonal to direction.
    Base::Vector3d cleanD;
    Base::Vector3d cleanX;
    Base::Vector3d cleanY;
    orthonormalizeBasis(spec.direction, spec.xDirection, cleanD, cleanX, cleanY);
    spec.direction = cleanD;
    spec.xDirection = cleanX;
    return spec;
}

Base::Vector3d ProjectionMath::clampPointToAlignment(
    const Base::Vector3d& parentPagePos,
    const Base::Vector3d& candidatePagePos,
    AlignmentKind mode,
    const Base::Vector3d& alignmentVector
)
{
    switch (mode) {
        case AlignmentKind::Horizontal:
            return Base::Vector3d(candidatePagePos.x, parentPagePos.y, 0.0);
        case AlignmentKind::Vertical:
            return Base::Vector3d(parentPagePos.x, candidatePagePos.y, 0.0);
        case AlignmentKind::Vector: {
            const double len = std::hypot(alignmentVector.x, alignmentVector.y);
            if (len < 1e-6) {
                return candidatePagePos;
            }
            const double ux = alignmentVector.x / len;
            const double uy = alignmentVector.y / len;
            const double rx = candidatePagePos.x - parentPagePos.x;
            const double ry = candidatePagePos.y - parentPagePos.y;
            const double proj = rx * ux + ry * uy;
            return Base::Vector3d(parentPagePos.x + ux * proj, parentPagePos.y + uy * proj, 0.0);
        }
        case AlignmentKind::None:
        case AlignmentKind::Broken:
            return candidatePagePos;
    }
    return candidatePagePos;
}

ProjectedViewSpec ProjectionMath::computeAuxiliaryView(
    const Base::Vector3d& parentDir,
    const Base::Vector3d& parentXDir,
    const Base::Vector3d& edgeTangent2DPage,
    const Base::Vector3d& cursorOffsetFromParent2DPage,
    bool thirdAngle,
    const std::string& auxSymbol
)
{
    Base::Vector3d d;
    Base::Vector3d x;
    Base::Vector3d y;
    orthonormalizeBasis(parentDir, parentXDir, d, x, y);

    Base::Vector3d t2d(edgeTangent2DPage.x, edgeTangent2DPage.y, 0.0);
    if (t2d.Length() < 1e-6) {
        t2d = Base::Vector3d(1.0, 0.0, 0.0);
    }
    else {
        t2d.Normalize();
    }

    // Two candidate 2D unit normals perpendicular to the slanted edge in page space (+Y up)
    Base::Vector3d n2d(-t2d.y, t2d.x, 0.0);
    const double dotCursor
        = n2d.x * cursorOffsetFromParent2DPage.x + n2d.y * cursorOffsetFromParent2DPage.y;
    if (dotCursor < 0.0) {
        n2d = n2d * (-1.0);
    }

    // Choose tangent sign so (t2d, n2d) forms a right-handed 2D frame (t2d x n2d = +Z)
    if ((t2d.x * n2d.y - t2d.y * n2d.x) < 0.0) {
        t2d = t2d * (-1.0);
    }

    const Base::Vector3d n3d = safeNormalize(x * n2d.x + y * n2d.y, x);
    const Base::Vector3d t3d = safeNormalize(x * t2d.x + y * t2d.y, y);

    // Third Angle: unfolding towards n2d looks along +n3d; First Angle looks along -n3d
    const double s = thirdAngle ? 1.0 : -1.0;
    ProjectedViewSpec spec;
    spec.direction = n3d * s;
    spec.xDirection = t3d;
    spec.alignment = AlignmentKind::Vector;
    spec.alignmentVector = n2d;
    spec.label = "Auxiliary " + (auxSymbol.empty() ? std::string("A") : auxSymbol);

    Base::Vector3d cleanD;
    Base::Vector3d cleanX;
    Base::Vector3d cleanY;
    orthonormalizeBasis(spec.direction, spec.xDirection, cleanD, cleanX, cleanY);
    spec.direction = cleanD;
    spec.xDirection = cleanX;
    return spec;
}

std::string ProjectionMath::alignmentKindToString(AlignmentKind mode)
{
    switch (mode) {
        case AlignmentKind::Horizontal:
            return "Horizontal";
        case AlignmentKind::Vertical:
            return "Vertical";
        case AlignmentKind::Vector:
            return "Vector";
        case AlignmentKind::Broken:
            return "Broken";
        case AlignmentKind::None:
            return "None";
    }
    return "None";
}

AlignmentKind ProjectionMath::alignmentKindFromString(const std::string& str)
{
    if (str == "Horizontal") {
        return AlignmentKind::Horizontal;
    }
    if (str == "Vertical") {
        return AlignmentKind::Vertical;
    }
    if (str == "Vector") {
        return AlignmentKind::Vector;
    }
    if (str == "Broken") {
        return AlignmentKind::Broken;
    }
    return AlignmentKind::None;
}

void ProjectionMath::attachViewAlignment(
    TechDraw::DrawView* child,
    TechDraw::DrawView* parent,
    AlignmentKind mode,
    const Base::Vector3d& alignmentVec
)
{
    if (!child || !parent || child == parent) {
        return;
    }

    auto* parentProp = ensureDynamicProperty<App::PropertyLink>(
        child,
        "App::PropertyLink",
        kPropParentView,
        "Parent drawing view that drives this view's alignment"
    );
    auto* modeProp = ensureDynamicProperty<App::PropertyString>(
        child,
        "App::PropertyString",
        kPropAlignmentMode,
        "Alignment constraint mode (Horizontal, Vertical, Vector, Broken, None)"
    );
    auto* savedModeProp = ensureDynamicProperty<App::PropertyString>(
        child,
        "App::PropertyString",
        kPropSavedAlignmentMode,
        "Last active alignment mode prior to Break Alignment"
    );
    auto* vecProp = ensureDynamicProperty<App::PropertyVector>(
        child,
        "App::PropertyVector",
        kPropAlignmentVector,
        "Unit 2D page-space alignment axis vector (+Y up)"
    );

    if (parentProp) {
        parentProp->setValue(parent);
    }
    const std::string modeStr = alignmentKindToString(mode);
    if (modeProp) {
        modeProp->setValue(modeStr);
    }
    if (savedModeProp && mode != AlignmentKind::Broken && mode != AlignmentKind::None) {
        savedModeProp->setValue(modeStr);
    }
    if (vecProp) {
        vecProp->setValue(alignmentVec);
    }
}

bool ProjectionMath::breakViewAlignment(TechDraw::DrawView* child)
{
    if (!child) {
        return false;
    }
    auto* modeProp = dynamic_cast<App::PropertyString*>(child->getPropertyByName(kPropAlignmentMode));
    if (!modeProp) {
        return false;
    }
    const AlignmentKind current = alignmentKindFromString(modeProp->getValue());
    if (current == AlignmentKind::None || current == AlignmentKind::Broken) {
        return false;
    }

    if (auto* savedProp = ensureDynamicProperty<App::PropertyString>(
            child,
            "App::PropertyString",
            kPropSavedAlignmentMode,
            "Last active alignment mode prior to Break Alignment"
        )) {
        savedProp->setValue(modeProp->getValue());
    }
    modeProp->setValue(alignmentKindToString(AlignmentKind::Broken));
    return true;
}

bool ProjectionMath::restoreViewAlignment(TechDraw::DrawView* child)
{
    if (!child) {
        return false;
    }
    TechDraw::DrawView* parent = getParentAlignedView(child);
    if (!parent) {
        return false;
    }
    auto* modeProp = dynamic_cast<App::PropertyString*>(child->getPropertyByName(kPropAlignmentMode));
    if (!modeProp) {
        return false;
    }

    AlignmentKind restoreMode = AlignmentKind::None;
    if (auto* savedProp
        = dynamic_cast<App::PropertyString*>(child->getPropertyByName(kPropSavedAlignmentMode))) {
        restoreMode = alignmentKindFromString(savedProp->getValue());
    }
    if (restoreMode == AlignmentKind::None || restoreMode == AlignmentKind::Broken) {
        // Infer Horizontal vs Vertical from relative offset to parent.
        const double dx = std::abs(child->X.getValue() - parent->X.getValue());
        const double dy = std::abs(child->Y.getValue() - parent->Y.getValue());
        restoreMode = (dx >= dy) ? AlignmentKind::Horizontal : AlignmentKind::Vertical;
    }

    Base::Vector3d alignVec(0.0, 0.0, 0.0);
    if (auto* vecProp
        = dynamic_cast<App::PropertyVector*>(child->getPropertyByName(kPropAlignmentVector))) {
        alignVec = vecProp->getValue();
    }

    modeProp->setValue(alignmentKindToString(restoreMode));

    const Base::Vector3d snapped = clampPointToAlignment(
        Base::Vector3d(parent->X.getValue(), parent->Y.getValue(), 0.0),
        Base::Vector3d(child->X.getValue(), child->Y.getValue(), 0.0),
        restoreMode,
        alignVec
    );
    child->X.setValue(snapped.x);
    child->Y.setValue(snapped.y);
    return true;
}

AlignmentKind ProjectionMath::getViewAlignmentMode(const TechDraw::DrawView* view)
{
    if (!view) {
        return AlignmentKind::None;
    }
    auto* modeProp = dynamic_cast<App::PropertyString*>(view->getPropertyByName(kPropAlignmentMode));
    if (!modeProp) {
        return AlignmentKind::None;
    }
    return alignmentKindFromString(modeProp->getValue());
}

TechDraw::DrawView* ProjectionMath::getParentAlignedView(const TechDraw::DrawView* view)
{
    if (!view) {
        return nullptr;
    }
    auto* parentProp = dynamic_cast<App::PropertyLink*>(view->getPropertyByName(kPropParentView));
    if (!parentProp) {
        return nullptr;
    }
    return dynamic_cast<TechDraw::DrawView*>(parentProp->getValue());
}

bool ProjectionMath::isImperialUnitSystem()
{
    const std::string unit = Base::UnitsApi::getBasicLengthUnit();
    return unit == "in" || unit == "ft" || unit == "yd" || unit == "thou";
}

std::vector<SheetPreset> ProjectionMath::availableSheetPresets()
{
    return {
        {"ISO_A4_L", "ISO A4 Landscape (297 x 210 mm)", "ISO5457", "ISO5457/A4/landscape/A4_Landscape_ISO5457_advanced.svg", false, 297.0, 210.0},
        {"ISO_A4_P", "ISO A4 Portrait (210 x 297 mm)", "ISO5457", "ISO5457/A4/portrait/A4_Portrait_ISO5457_advanced.svg", false, 210.0, 297.0},
        {"ISO_A3_L", "ISO A3 Landscape (420 x 297 mm)", "ISO5457", "ISO5457/A3/landscape/A3_Landscape_ISO5457_advanced.svg", false, 420.0, 297.0},
        {"ISO_A2_L", "ISO A2 Landscape (594 x 420 mm)", "ISO5457", "ISO5457/A2/landscape/A2_Landscape_ISO5457_advanced.svg", false, 594.0, 420.0},
        {"ISO_A1_L", "ISO A1 Landscape (841 x 594 mm)", "ISO5457", "ISO5457/A1/landscape/A1_Landscape_ISO5457_advanced.svg", false, 841.0, 594.0},
        {"ISO_A0_L", "ISO A0 Landscape (1189 x 841 mm)", "ISO5457", "ISO5457/A0/landscape/A0_Landscape_ISO5457_advanced.svg", false, 1189.0, 841.0},
        {"ASME_A_L", "ANSI A Landscape (11 x 8.5 in)", "ASME Y14.1", "ASME Y14.1/A/landscape/ANSIA_Landscape.svg", true, 279.4, 215.9},
        {"ASME_A_P", "ANSI A Portrait (8.5 x 11 in)", "ASME Y14.1", "ASME Y14.1/A/portrait/ANSIA_Portrait.svg", true, 215.9, 279.4},
        {"ASME_B_L", "ANSI B Landscape (17 x 11 in)", "ASME Y14.1", "ASME Y14.1/B/landscape/ANSIB_Landscape.svg", true, 431.8, 279.4},
        {"ASME_C_L", "ANSI C Landscape (22 x 17 in)", "ASME Y14.1", "ASME Y14.1/C/landscape/ANSIC_Landscape.svg", true, 558.8, 431.8},
        {"ASME_D_L", "ANSI D Landscape (34 x 22 in)", "ASME Y14.1", "ASME Y14.1/D/landscape/ANSID_Landscape.svg", true, 863.6, 558.8},
        {"ASME_E_L", "ANSI E Landscape (44 x 34 in)", "ASME Y14.1", "ASME Y14.1/E/landscape/ANSIE_Landscape.svg", true, 1117.6, 863.6},
    };
}

SheetPreset ProjectionMath::defaultSheetPresetForUnits(bool imperial)
{
    const auto presets = availableSheetPresets();
    const std::string targetId = imperial ? "ASME_B_L" : "ISO_A3_L";
    for (const auto& preset : presets) {
        if (preset.id == targetId) {
            return preset;
        }
    }
    return presets.front();
}

std::string ProjectionMath::resolveTemplatePath(const SheetPreset& preset)
{
    namespace fs = std::filesystem;
    const fs::path base = fs::path(App::Application::getResourceDir()) / "Mod" / "TechDraw"
        / "Templates" / preset.templateRelPath;
    if (fs::exists(base)) {
        return base.string();
    }
    // Fallback for in-tree test runs before resource copy
    const fs::path srcFallback = fs::path(App::Application::getHomePath()) / "share" / "Mod"
        / "TechDraw" / "Templates" / preset.templateRelPath;
    if (fs::exists(srcFallback)) {
        return srcFallback.string();
    }
    return base.string();
}

double ProjectionMath::suggestStandardScale(
    double modelWidthMm,
    double modelHeightMm,
    double sheetWidthMm,
    double sheetHeightMm
)
{
    if (modelWidthMm <= 1e-6 || modelHeightMm <= 1e-6 || sheetWidthMm <= 1e-6
        || sheetHeightMm <= 1e-6) {
        return 1.0;
    }

    // Reserve generous sheet margin + title block + space for multiple orthogonal views.
    const double targetW = sheetWidthMm * 0.32;
    const double targetH = sheetHeightMm * 0.30;
    const double rawScale = std::min(targetW / modelWidthMm, targetH / modelHeightMm);

    static constexpr double kStandardScales[] = {
        0.01, 0.02, 0.05, 0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 2.5, 5.0, 10.0, 20.0, 50.0,
    };

    double chosen = kStandardScales[0];
    for (double candidate : kStandardScales) {
        if (candidate <= rawScale * 1.05) {
            chosen = candidate;
        }
    }
    return chosen;
}

}  // namespace Drawing
