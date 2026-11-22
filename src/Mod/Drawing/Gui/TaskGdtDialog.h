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

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include <Mod/Drawing/App/DraftingTools.h>
#include <Mod/Drawing/DrawingGlobal.h>

namespace DrawingGui
{

class DrawingGuiExport DatumSymbolDialog: public QDialog
{
    Q_OBJECT

public:
    explicit DatumSymbolDialog(QWidget* parent = nullptr);
    ~DatumSymbolDialog() override = default;

    Drawing::DatumSymbolSpec resultSpec() const;

private:
    QLineEdit* m_letterEdit {nullptr};
    QCheckBox* m_filledCheck {nullptr};
    QDoubleSpinBox* m_stemSpin {nullptr};
};

class DrawingGuiExport GdtFrameDialog: public QDialog
{
    Q_OBJECT

public:
    explicit GdtFrameDialog(QWidget* parent = nullptr);
    ~GdtFrameDialog() override = default;

    Drawing::FeatureControlFrameSpec resultSpec() const;

private:
    QComboBox* m_charCombo {nullptr};
    QCheckBox* m_diameterCheck {nullptr};
    QLineEdit* m_tolEdit {nullptr};
    QComboBox* m_matModCombo {nullptr};
    QLineEdit* m_datumAEdit {nullptr};
    QComboBox* m_datumAModCombo {nullptr};
    QLineEdit* m_datumBEdit {nullptr};
    QComboBox* m_datumBModCombo {nullptr};
    QLineEdit* m_datumCEdit {nullptr};
    QComboBox* m_datumCModCombo {nullptr};
};

}  // namespace DrawingGui
