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

#pragma once

#include <string>
#include <vector>

#include <Base/Vector3D.h>
#include <Mod/Drawing/DrawingGlobal.h>

namespace TechDraw
{
class DrawPage;
class DrawView;
class DrawViewPart;
}  // namespace TechDraw

namespace Drawing
{

enum class StandardOrientation
{
    Front,
    Back,
    Top,
    Bottom,
    Left,
    Right,
    Isometric,
};

enum class ProjectionDirection8
{
    East,
    NorthEast,
    North,
    NorthWest,
    West,
    SouthWest,
    South,
    SouthEast,
};

enum class AlignmentKind
{
    None,
    Horizontal,
    Vertical,
    Vector,
    Broken,
};

struct ViewOrientation
{
    Base::Vector3d direction;
    Base::Vector3d xDirection;
    std::string label;
};

struct ProjectedViewSpec
{
    Base::Vector3d direction;
    Base::Vector3d xDirection;
    AlignmentKind alignment {AlignmentKind::None};
    Base::Vector3d alignmentVector;  // Page-space unit vector (+Y up)
    std::string label;
};

struct SheetPreset
{
    std::string id;
    std::string displayName;
    std::string standard;          // "ISO5457" or "ASME Y14.1"
    std::string templateRelPath;   // Relative to Mod/TechDraw/Templates/
    bool thirdAngle {false};
    double widthMm {420.0};
    double heightMm {297.0};
};

class DrawingExport ProjectionMath
{
public:
    static ViewOrientation standardOrientation(StandardOrientation orient);

    /// Classify a 2D page-space drag offset (pageDx right+, pageDy up+) into 8 compass sectors.
    static ProjectionDirection8 classifyDirection8(double pageDx, double pageDy);

    /// Compute 3D projection direction, XDirection, and 2D alignment constraint for a child view.
    static ProjectedViewSpec computeProjectedView(
        const Base::Vector3d& parentDir,
        const Base::Vector3d& parentXDir,
        ProjectionDirection8 dir8,
        bool thirdAngle
    );

    /// Clamp a candidate page-space point (pageX, pageY) onto the alignment line through parentPos.
    static Base::Vector3d clampPointToAlignment(
        const Base::Vector3d& parentPagePos,
        const Base::Vector3d& candidatePagePos,
        AlignmentKind mode,
        const Base::Vector3d& alignmentVector
    );

    /// Compute 3D projection direction, XDirection, and vector alignment for an auxiliary view folded perpendicular to a 2D slanted edge.
    static ProjectedViewSpec computeAuxiliaryView(
        const Base::Vector3d& parentDir,
        const Base::Vector3d& parentXDir,
        const Base::Vector3d& edgeTangent2DPage,
        const Base::Vector3d& cursorOffsetFromParent2DPage,
        bool thirdAngle,
        const std::string& auxSymbol = "A"
    );

    /// Convert AlignmentKind to/from the dynamic property string stored on DrawView objects.
    static std::string alignmentKindToString(AlignmentKind mode);
    static AlignmentKind alignmentKindFromString(const std::string& str);

    /// Attach or update standalone dynamic alignment properties (100% .FCStd compatible).
    static void attachViewAlignment(
        TechDraw::DrawView* child,
        TechDraw::DrawView* parent,
        AlignmentKind mode,
        const Base::Vector3d& alignmentVec = Base::Vector3d(0.0, 0.0, 0.0)
    );

    /// Break active alignment on a standalone projected/section view while remembering previous mode.
    static bool breakViewAlignment(TechDraw::DrawView* child);

    /// Restore previously broken alignment and snap the child view back onto its parent's alignment line.
    static bool restoreViewAlignment(TechDraw::DrawView* child);

    static AlignmentKind getViewAlignmentMode(const TechDraw::DrawView* view);
    static TechDraw::DrawView* getParentAlignedView(const TechDraw::DrawView* view);

    /// Unit-aware sheet template presets.
    static bool isImperialUnitSystem();
    static std::vector<SheetPreset> availableSheetPresets();
    static SheetPreset defaultSheetPresetForUnits(bool imperial);
    static std::string resolveTemplatePath(const SheetPreset& preset);

    /// Compute a clean standard drawing scale (e.g., 1:1, 1:2, 1:5, 2:1) fitting modelSize into targetSize.
    static double suggestStandardScale(double modelWidthMm, double modelHeightMm, double sheetWidthMm, double sheetHeightMm);
};

}  // namespace Drawing
