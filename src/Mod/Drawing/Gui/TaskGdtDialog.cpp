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

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "TaskGdtDialog.h"

namespace DrawingGui
{

namespace
{

void populateModifierCombo(QComboBox* combo)
{
    if (!combo) {
        return;
    }
    combo->addItem(QObject::tr("None"), static_cast<int>(Drawing::MaterialConditionModifier::None));
    combo->addItem(QStringLiteral("Ⓜ MMC"), static_cast<int>(Drawing::MaterialConditionModifier::MMC));
    combo->addItem(QStringLiteral("Ⓛ LMC"), static_cast<int>(Drawing::MaterialConditionModifier::LMC));
    combo->addItem(QStringLiteral("Ⓢ RFS"), static_cast<int>(Drawing::MaterialConditionModifier::RFS));
}

}  // namespace

DatumSymbolDialog::DatumSymbolDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Insert Datum Feature Symbol"));
    setMinimumWidth(340);

    auto* mainLayout = new QVBoxLayout(this);
    auto* formGroup = new QGroupBox(tr("Datum Feature (ISO 5459 / ASME Y14.5)"), this);
    auto* form = new QFormLayout(formGroup);

    m_letterEdit = new QLineEdit(QStringLiteral("A"), formGroup);
    m_letterEdit->setMaxLength(4);
    form->addRow(tr("Datum letter:"), m_letterEdit);

    m_filledCheck = new QCheckBox(tr("Filled datum triangle"), formGroup);
    m_filledCheck->setChecked(true);
    form->addRow(QString(), m_filledCheck);

    m_stemSpin = new QDoubleSpinBox(formGroup);
    m_stemSpin->setRange(3.0, 40.0);
    m_stemSpin->setValue(8.0);
    m_stemSpin->setSuffix(QStringLiteral(" mm"));
    form->addRow(tr("Stem length:"), m_stemSpin);

    mainLayout->addWidget(formGroup);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

Drawing::DatumSymbolSpec DatumSymbolDialog::resultSpec() const
{
    Drawing::DatumSymbolSpec spec;
    const QString txt = m_letterEdit ? m_letterEdit->text().trimmed().toUpper() : QStringLiteral("A");
    spec.letter = txt.isEmpty() ? "A" : txt.toStdString();
    spec.filledTriangle = !m_filledCheck || m_filledCheck->isChecked();
    spec.stemLengthMm = m_stemSpin ? m_stemSpin->value() : 8.0;
    return spec;
}

GdtFrameDialog::GdtFrameDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Geometric Tolerance (GD&T Feature Control Frame)"));
    setMinimumWidth(440);

    auto* mainLayout = new QVBoxLayout(this);
    auto* tolGroup = new QGroupBox(tr("Tolerance Zone"), this);
    auto* tolForm = new QFormLayout(tolGroup);

    m_charCombo = new QComboBox(tolGroup);
    const struct
    {
        Drawing::GdtCharacteristic ch;
        const char* label;
    } items[] = {
        {Drawing::GdtCharacteristic::Position, "⌖ Position"},
        {Drawing::GdtCharacteristic::Flatness, "⏥ Flatness"},
        {Drawing::GdtCharacteristic::Straightness, "⏤ Straightness"},
        {Drawing::GdtCharacteristic::Circularity, "○ Circularity"},
        {Drawing::GdtCharacteristic::Cylindricity, "⌭ Cylindricity"},
        {Drawing::GdtCharacteristic::Parallelism, "∥ Parallelism"},
        {Drawing::GdtCharacteristic::Perpendicularity, "⟂ Perpendicularity"},
        {Drawing::GdtCharacteristic::Angularity, "∠ Angularity"},
        {Drawing::GdtCharacteristic::Concentricity, "◎ Concentricity"},
        {Drawing::GdtCharacteristic::Symmetry, "⌯ Symmetry"},
        {Drawing::GdtCharacteristic::ProfileOfSurface, "⌓ Profile of a Surface"},
        {Drawing::GdtCharacteristic::ProfileOfLine, "⌒ Profile of a Line"},
        {Drawing::GdtCharacteristic::CircularRunout, "↗ Circular Runout"},
        {Drawing::GdtCharacteristic::TotalRunout, "⇗ Total Runout"},
    };
    for (const auto& item : items) {
        m_charCombo->addItem(QString::fromUtf8(item.label), static_cast<int>(item.ch));
    }
    tolForm->addRow(tr("Characteristic:"), m_charCombo);

    auto* zoneRow = new QHBoxLayout();
    m_diameterCheck = new QCheckBox(QStringLiteral("Ø"), tolGroup);
    m_diameterCheck->setChecked(true);
    m_tolEdit = new QLineEdit(QStringLiteral("0.05"), tolGroup);
    m_matModCombo = new QComboBox(tolGroup);
    populateModifierCombo(m_matModCombo);
    zoneRow->addWidget(m_diameterCheck);
    zoneRow->addWidget(m_tolEdit, 1);
    zoneRow->addWidget(m_matModCombo);
    tolForm->addRow(tr("Tolerance:"), zoneRow);

    mainLayout->addWidget(tolGroup);

    auto* datumGroup = new QGroupBox(tr("Datum References"), this);
    auto* datumForm = new QFormLayout(datumGroup);

    auto makeDatumRow = [datumGroup](QLineEdit*& edit, QComboBox*& modCombo, const QString& defVal) {
        auto* row = new QHBoxLayout();
        edit = new QLineEdit(defVal, datumGroup);
        edit->setMaxLength(4);
        modCombo = new QComboBox(datumGroup);
        populateModifierCombo(modCombo);
        row->addWidget(edit, 1);
        row->addWidget(modCombo);
        return row;
    };

    datumForm->addRow(
        tr("Primary datum:"),
        makeDatumRow(m_datumAEdit, m_datumAModCombo, QStringLiteral("A"))
    );
    datumForm->addRow(tr("Secondary datum:"), makeDatumRow(m_datumBEdit, m_datumBModCombo, QString()));
    datumForm->addRow(tr("Tertiary datum:"), makeDatumRow(m_datumCEdit, m_datumCModCombo, QString()));

    mainLayout->addWidget(datumGroup);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

Drawing::FeatureControlFrameSpec GdtFrameDialog::resultSpec() const
{
    Drawing::FeatureControlFrameSpec spec;
    if (m_charCombo) {
        spec.characteristic = static_cast<Drawing::GdtCharacteristic>(
            m_charCombo->currentData().toInt()
        );
    }
    spec.diameterZone = !m_diameterCheck || m_diameterCheck->isChecked();
    spec.toleranceValue
        = (m_tolEdit && !m_tolEdit->text().trimmed().isEmpty()) ? m_tolEdit->text().trimmed().toStdString()
                                                                : "0.05";
    if (m_matModCombo) {
        spec.materialModifier = static_cast<Drawing::MaterialConditionModifier>(
            m_matModCombo->currentData().toInt()
        );
    }
    if (m_datumAEdit) {
        spec.datumA = m_datumAEdit->text().trimmed().toUpper().toStdString();
    }
    if (m_datumAModCombo) {
        spec.datumAModifier = static_cast<Drawing::MaterialConditionModifier>(
            m_datumAModCombo->currentData().toInt()
        );
    }
    if (m_datumBEdit) {
        spec.datumB = m_datumBEdit->text().trimmed().toUpper().toStdString();
    }
    if (m_datumBModCombo) {
        spec.datumBModifier = static_cast<Drawing::MaterialConditionModifier>(
            m_datumBModCombo->currentData().toInt()
        );
    }
    if (m_datumCEdit) {
        spec.datumC = m_datumCEdit->text().trimmed().toUpper().toStdString();
    }
    if (m_datumCModCombo) {
        spec.datumCModifier = static_cast<Drawing::MaterialConditionModifier>(
            m_datumCModCombo->currentData().toInt()
        );
    }
    return spec;
}

}  // namespace DrawingGui
