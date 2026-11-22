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

#include <QFrame>
#include <QPoint>

#include <Mod/Drawing/DrawingGlobal.h>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace TechDraw
{
class DrawViewDimension;
}  // namespace TechDraw

namespace DrawingGui
{

/**
 * SolidWorks-inspired floating Dimension Palette shown near a selected DrawViewDimension.
 * Provides 1-click symbol insertion (Ø, R, □, ±, °, ⌴, ⌵, ↧, 2X), prefix/suffix editing,
 * Reference parentheses (), precision, tolerance type/values, flip arrowheads, and Show Units.
 */
class DrawingGuiExport DimensionPalettePopup: public QFrame
{
    Q_OBJECT

public:
    explicit DimensionPalettePopup(QWidget* parent = nullptr);

    void showForDimension(TechDraw::DrawViewDimension* dim, const QPoint& globalAnchor);
    TechDraw::DrawViewDimension* currentDimension() const
    {
        return m_dim;
    }

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void applyChanges();
    void insertPrefixSymbol(const QString& symbol);
    void onToleranceModeChanged(int index);

private:
    void loadFromDimension();
    void positionNear(const QPoint& globalAnchor);
    static void splitFormatSpec(
        const QString& formatSpec,
        QString& outPrefix,
        int& outPrecision,
        QString& outSuffix,
        bool& outIsRef
    );
    static QString buildFormatSpec(
        const QString& prefix,
        int precision,
        const QString& suffix,
        bool isReference
    );

    TechDraw::DrawViewDimension* m_dim {nullptr};
    bool m_loading {false};

    QLineEdit* m_prefixEdit {nullptr};
    QLineEdit* m_suffixEdit {nullptr};
    QSpinBox* m_precisionSpin {nullptr};
    QCheckBox* m_referenceCheck {nullptr};
    QCheckBox* m_theoreticalCheck {nullptr};
    QCheckBox* m_flipArrowsCheck {nullptr};
    QCheckBox* m_showUnitsCheck {nullptr};
    QComboBox* m_tolModeCombo {nullptr};
    QDoubleSpinBox* m_upperTolSpin {nullptr};
    QDoubleSpinBox* m_lowerTolSpin {nullptr};
};

}  // namespace DrawingGui
