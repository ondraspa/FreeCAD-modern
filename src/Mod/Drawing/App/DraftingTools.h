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
#include <utility>
#include <vector>

#include <Base/Vector3D.h>
#include <Mod/Drawing/DrawingGlobal.h>
#include <Mod/TechDraw/App/DrawViewBalloon.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/DrawViewSpreadsheet.h>
#include <Mod/TechDraw/App/DrawViewSymbol.h>

namespace App
{
class Document;
class DocumentObject;
}  // namespace App

namespace Part
{
class Feature;
}  // namespace Part

namespace Spreadsheet
{
class Sheet;
}  // namespace Spreadsheet

namespace TechDraw
{
class DrawBrokenView;
class DrawPage;
class DrawView;
class DrawViewClip;
}  // namespace TechDraw

namespace Drawing
{

/// Geometric characteristic symbols per ASME Y14.5 / ISO 1101.
enum class GdtCharacteristic
{
    Straightness,       // ⏤
    Flatness,           // ⏥
    Circularity,        // ○
    Cylindricity,       // ⌭
    ProfileOfLine,      // ⌒
    ProfileOfSurface,   // ⌓
    Parallelism,        // ∥
    Perpendicularity,   // ⟂
    Angularity,         // ∠
    Position,           // ⌖
    Concentricity,      // ◎
    Symmetry,           // ⌯
    CircularRunout,     // ↗
    TotalRunout,        // ⇗
};

enum class MaterialConditionModifier
{
    None,
    MMC,  // Ⓜ Maximum Material Condition
    LMC,  // Ⓛ Least Material Condition
    RFS,  // Ⓢ Regardless of Feature Size
};

struct FeatureControlFrameSpec
{
    GdtCharacteristic characteristic {GdtCharacteristic::Position};
    bool diameterZone {true};
    std::string toleranceValue {"0.05"};
    MaterialConditionModifier materialModifier {MaterialConditionModifier::None};
    std::string datumA;
    MaterialConditionModifier datumAModifier {MaterialConditionModifier::None};
    std::string datumB;
    MaterialConditionModifier datumBModifier {MaterialConditionModifier::None};
    std::string datumC;
    MaterialConditionModifier datumCModifier {MaterialConditionModifier::None};
};

struct DatumSymbolSpec
{
    std::string letter {"A"};
    bool filledTriangle {true};
    double stemLengthMm {8.0};
};

struct HoleCalloutInfo
{
    int count {1};
    double diameterMm {0.0};
    bool throughAll {true};
    double depthMm {0.0};
    bool threaded {false};
    std::string threadDesignation;
    double threadDepthMm {0.0};
    bool hasCounterbore {false};
    double counterboreDiameterMm {0.0};
    double counterboreDepthMm {0.0};
    bool hasCountersink {false};
    double countersinkDiameterMm {0.0};
    double countersinkAngleDeg {90.0};
    std::string formattedCallout;
};

struct BomRowEntry
{
    std::string itemNumber;
    std::string partNumber;
    std::string description;
    std::string material;
    int quantity {1};
    App::DocumentObject* sourceObject {nullptr};
};

/// Native C++ Datum Feature symbol view (subclassing DrawViewSymbol for 100% .FCStd compatibility).
class DrawingExport DrawViewDatum: public TechDraw::DrawViewSymbol
{
    PROPERTY_HEADER_WITH_OVERRIDE(Drawing::DrawViewDatum);

public:
    DrawViewDatum();
    ~DrawViewDatum() override = default;

    App::PropertyString DatumLetter;
    App::PropertyBool FilledTriangle;
    App::PropertyFloat StemLength;

    App::DocumentObjectExecReturn* execute() override;
    short mustExecute() const override;
    void refreshSymbolSvg();
};

/// Native C++ GD&T Feature Control Frame view (subclassing DrawViewSymbol for 100% .FCStd compatibility).
class DrawingExport DrawViewGDT: public TechDraw::DrawViewSymbol
{
    PROPERTY_HEADER_WITH_OVERRIDE(Drawing::DrawViewGDT);

public:
    DrawViewGDT();
    ~DrawViewGDT() override = default;

    App::PropertyString Characteristic;
    App::PropertyBool DiameterZone;
    App::PropertyString ToleranceValue;
    App::PropertyString MaterialModifier;
    App::PropertyString DatumPrimary;
    App::PropertyString DatumSecondary;
    App::PropertyString DatumTertiary;

    App::DocumentObjectExecReturn* execute() override;
    short mustExecute() const override;
    void refreshSymbolSvg();
};

/// Pure-C++ drafting helper for Milestone 3: 2D breaks, in-place crop, GD&T/Datum symbols,
/// parametric Hole/Thread callouts, Assembly BOM tables, BOM-linked Balloons, and Magnetic Lines.
class DrawingExport DraftingTools
{
public:
    // --- 2D Break View Helpers ---
    /// Map two 2D view-local cut coordinates (in unscaled model mm along X or Y) into a 3D break line
    /// feature and create/update a TechDraw::DrawBrokenView on the sheet without manual 3D sketches.
    static std::pair<Base::Vector3d, Base::Vector3d> compute3DBreakPointsFrom2DViewCut(
        const TechDraw::DrawViewPart* baseView,
        double cut1ViewMm,
        double cut2ViewMm,
        bool verticalBreakLine
    );

    static TechDraw::DrawBrokenView* createBrokenViewFrom2DCut(
        TechDraw::DrawPage* page,
        TechDraw::DrawViewPart* baseView,
        double cut1ViewMm,
        double cut2ViewMm,
        bool verticalBreakLine,
        double gapMm = 10.0
    );

    // --- In-Place Crop View Helpers ---
    /// Crop an existing DrawViewPart in-place on its DrawPage using a TechDraw::DrawViewClip frame.
    static TechDraw::DrawViewClip* cropViewInPlace(
        TechDraw::DrawPage* page,
        TechDraw::DrawViewPart* targetView,
        double centerPageX,
        double centerPageY,
        double widthMm,
        double heightMm,
        bool showFrame = false
    );

    // --- GD&T & Datum Symbol Builders ---
    static std::string characteristicSymbolString(GdtCharacteristic ch);
    static GdtCharacteristic characteristicFromString(const std::string& name);
    static std::string materialModifierSymbolString(MaterialConditionModifier mod);
    static MaterialConditionModifier materialModifierFromString(const std::string& name);

    static std::string buildDatumFeatureSvg(const DatumSymbolSpec& spec);
    static std::string buildFeatureControlFrameSvg(const FeatureControlFrameSpec& spec);

    /// Create a datum symbol on the page (backed by TechDraw::DrawViewSymbol with dynamic metadata
    /// properties so upstream FreeCAD opens it without needing Drawing.so loaded).
    static TechDraw::DrawViewSymbol* createDatumSymbolOnPage(
        TechDraw::DrawPage* page,
        const DatumSymbolSpec& spec,
        double pageX,
        double pageY,
        App::DocumentObject* ownerView = nullptr
    );

    /// Create a GD&T Feature Control Frame on the page (backed by TechDraw::DrawViewSymbol with
    /// structured dynamic properties so it can be re-edited and opens in upstream FreeCAD).
    static TechDraw::DrawViewSymbol* createGdtFrameOnPage(
        TechDraw::DrawPage* page,
        const FeatureControlFrameSpec& spec,
        double pageX,
        double pageY,
        App::DocumentObject* ownerView = nullptr
    );

    // --- Parametric Hole & Thread Callout ---
    /// Extract hole/thread parameters from a PartDesign::Hole feature (or any object with Hole properties)
    /// and format an ISO/ASME standard callout (e.g. "4X Ø6.5 THRU ⌴ Ø11.0 ↧ 6.0" or "2X M8x1.25 - 6H ↧ 16.0").
    static HoleCalloutInfo extractHoleCalloutFromFeature(const App::DocumentObject* holeFeature);

    /// Inspect a DrawViewPart (and optional 2D sub-element name like "Edge1") to locate an underlying
    /// PartDesign::Hole feature or circular edge diameter and produce a ready-to-place callout info.
    static HoleCalloutInfo inspectViewForHoleCallout(
        const TechDraw::DrawViewPart* viewPart,
        const std::string& subElementName = {}
    );

    static std::string formatHoleCalloutText(const HoleCalloutInfo& info);

    /// Place a parametric Hole Callout balloon/leader on a DrawPage attached to a DrawViewPart.
    static TechDraw::DrawViewBalloon* createHoleCalloutOnPage(
        TechDraw::DrawPage* page,
        TechDraw::DrawViewPart* viewPart,
        const std::string& subElementName,
        double originViewMmX,
        double originViewMmY,
        double bubbleViewMmX,
        double bubbleViewMmY
    );

    // --- Assembly BOM Table, Linked Balloons & Magnetic Lines ---
    /// Collect BOM rows from an Assembly::BomObject, Spreadsheet::Sheet, or directly from a 3D source hierarchy.
    static std::vector<BomRowEntry> collectBomEntries(
        App::Document* doc,
        App::DocumentObject* sourceOrBom = nullptr
    );

    /// Ensure a Spreadsheet::Sheet (or existing Assembly::BomObject) is populated with BOM rows and
    /// create a styled TechDraw::DrawViewSpreadsheet snapped to the sheet's title-block corner.
    static TechDraw::DrawViewSpreadsheet* createOrUpdateBomTableOnPage(
        TechDraw::DrawPage* page,
        App::DocumentObject* sourceOrBom = nullptr
    );

    /// Resolve the BOM item number ("1", "2", ...) for a target component or a DrawViewPart referencing it.
    static std::string resolveBomItemNumber(
        App::Document* doc,
        const App::DocumentObject* componentOrView,
        const std::vector<BomRowEntry>& entries = {}
    );

    /// Automatically place BOM-linked balloons around a DrawViewPart on a clean circular/magnetic layout.
    static std::vector<TechDraw::DrawViewBalloon*> createAutoBalloonsForView(
        TechDraw::DrawPage* page,
        TechDraw::DrawViewPart* viewPart
    );

    /// Align a set of DrawViewBalloons evenly along a 2D magnetic line segment (in view-local mm coordinates)
    /// and attach dynamic magnetic-group metadata.
    static void alignBalloonsOnMagneticLine(
        const std::vector<TechDraw::DrawViewBalloon*>& balloons,
        const Base::Vector3d& startViewMm,
        const Base::Vector3d& endViewMm
    );
};

}  // namespace Drawing
