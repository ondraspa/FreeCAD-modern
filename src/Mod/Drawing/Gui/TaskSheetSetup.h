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

#include <QDialog>

#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/Drawing/DrawingGlobal.h>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QRadioButton;

namespace App
{
class Document;
class DocumentObject;
}  // namespace App

namespace TechDraw
{
class DrawPage;
}  // namespace TechDraw

namespace DrawingGui
{

struct SheetSetupResult
{
    std::string sheetName;
    Drawing::SheetPreset preset;
    bool thirdAngle {false};
    double scale {1.0};
    bool autoScale {true};
    bool createStandardFourViews {false};
    App::DocumentObject* sourceObject {nullptr};
};

class DrawingGuiExport SheetSetupDialog: public QDialog
{
    Q_OBJECT

public:
    explicit SheetSetupDialog(
        App::Document* doc,
        TechDraw::DrawPage* existingPage = nullptr,
        App::DocumentObject* defaultSource = nullptr,
        QWidget* parent = nullptr
    );

    SheetSetupResult result() const;

    /// Discover all drawable 3D solids/bodies/parts/assemblies in doc.
    static std::vector<App::DocumentObject*> collectDrawableSources(App::Document* doc);

    /// Create a brand-new TechDraw::DrawPage + SVG template + optional standard 4 views.
    static TechDraw::DrawPage* createSheet(App::Document* doc, const SheetSetupResult& setup);

    /// Apply edited sheet properties to an existing DrawPage and refresh title block.
    static void applyPropertiesToSheet(TechDraw::DrawPage* page, const SheetSetupResult& setup);

private Q_SLOTS:
    void onPresetChanged(int index);
    void onScalePresetChanged(int index);

private:
    void populatePresets(TechDraw::DrawPage* existingPage);
    void populateSources(App::Document* doc, App::DocumentObject* defaultSource);

    App::Document* m_doc {nullptr};
    bool m_editingExisting {false};
    std::vector<Drawing::SheetPreset> m_presets;
    std::vector<App::DocumentObject*> m_sources;

    QLineEdit* m_nameEdit {nullptr};
    QComboBox* m_presetCombo {nullptr};
    QComboBox* m_projectionCombo {nullptr};
    QComboBox* m_scalePresetCombo {nullptr};
    QDoubleSpinBox* m_customScaleSpin {nullptr};
    QComboBox* m_sourceCombo {nullptr};
    QRadioButton* m_blankModeRadio {nullptr};
    QRadioButton* m_fourViewModeRadio {nullptr};
};

}  // namespace DrawingGui
