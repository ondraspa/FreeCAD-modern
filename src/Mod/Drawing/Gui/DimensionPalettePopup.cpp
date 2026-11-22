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

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <App/Document.h>
#include <Gui/Application.h>
#include <Mod/TechDraw/App/DrawViewDimension.h>
#include <Mod/TechDraw/Gui/ViewProviderDimension.h>

#include "DimensionPalettePopup.h"

namespace DrawingGui
{

namespace
{

constexpr int kOffsetX = 18;
constexpr int kOffsetY = 16;

}  // namespace

DimensionPalettePopup::DimensionPalettePopup(QWidget* parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("DimensionPalettePopup"));
    setFrameShape(QFrame::StyledPanel);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 8);
    root->setSpacing(6);

    // Header row
    auto* headerRow = new QHBoxLayout();
    auto* titleLbl = new QLabel(tr("Dimension Palette"), this);
    titleLbl->setStyleSheet(QStringLiteral("font-weight: 600;"));
    headerRow->addWidget(titleLbl);
    headerRow->addStretch(1);

    auto* closeBtn = new QToolButton(this);
    closeBtn->setText(QStringLiteral("✕"));
    closeBtn->setAutoRaise(true);
    closeBtn->setToolTip(tr("Close palette"));
    connect(closeBtn, &QToolButton::clicked, this, &QWidget::hide);
    headerRow->addWidget(closeBtn);
    root->addLayout(headerRow);

    // Quick drafting symbol buttons
    auto* symRow = new QHBoxLayout();
    symRow->setSpacing(2);
    struct SymBtn
    {
        const char* glyph;
        const char* tip;
    };
    const SymBtn symbols[] = {
        {"Ø", "Diameter"},
        {"R", "Radius"},
        {"□", "Square"},
        {"±", "Plus/Minus"},
        {"°", "Degree"},
        {"⌴", "Counterbore"},
        {"⌵", "Countersink"},
        {"↧", "Depth"},
        {"2X ", "2X Repetition"},
    };
    for (const auto& s : symbols) {
        auto* btn = new QToolButton(this);
        const QString glyph = QString::fromUtf8(s.glyph);
        btn->setText(glyph.trimmed());
        btn->setToolTip(tr(s.tip));
        btn->setMinimumSize(24, 24);
        connect(btn, &QToolButton::clicked, this, [this, glyph]() { insertPrefixSymbol(glyph); });
        symRow->addWidget(btn);
    }
    root->addLayout(symRow);

    // Prefix / Value / Suffix row
    auto* textRow = new QHBoxLayout();
    m_prefixEdit = new QLineEdit(this);
    m_prefixEdit->setPlaceholderText(tr("Prefix"));
    m_prefixEdit->setMaximumWidth(86);
    connect(m_prefixEdit, &QLineEdit::editingFinished, this, &DimensionPalettePopup::applyChanges);

    auto* valLbl = new QLabel(QStringLiteral("<val>"), this);
    valLbl->setAlignment(Qt::AlignCenter);

    m_suffixEdit = new QLineEdit(this);
    m_suffixEdit->setPlaceholderText(tr("Suffix"));
    m_suffixEdit->setMaximumWidth(86);
    connect(m_suffixEdit, &QLineEdit::editingFinished, this, &DimensionPalettePopup::applyChanges);

    textRow->addWidget(m_prefixEdit);
    textRow->addWidget(valLbl);
    textRow->addWidget(m_suffixEdit);
    root->addLayout(textRow);

    // Precision & Flags row
    auto* precRow = new QHBoxLayout();
    precRow->addWidget(new QLabel(tr("Decimals:"), this));
    m_precisionSpin = new QSpinBox(this);
    m_precisionSpin->setRange(0, 6);
    connect(
        m_precisionSpin,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        &DimensionPalettePopup::applyChanges
    );
    precRow->addWidget(m_precisionSpin);

    m_referenceCheck = new QCheckBox(tr("(Ref)"), this);
    connect(m_referenceCheck, &QCheckBox::toggled, this, &DimensionPalettePopup::applyChanges);
    precRow->addWidget(m_referenceCheck);

    m_theoreticalCheck = new QCheckBox(tr("[Basic]"), this);
    connect(m_theoreticalCheck, &QCheckBox::toggled, this, &DimensionPalettePopup::applyChanges);
    precRow->addWidget(m_theoreticalCheck);
    root->addLayout(precRow);

    // Tolerance row
    auto* tolRow = new QHBoxLayout();
    m_tolModeCombo = new QComboBox(this);
    m_tolModeCombo->addItem(tr("No Tolerance"), 0);
    m_tolModeCombo->addItem(tr("Symmetric ±"), 1);
    m_tolModeCombo->addItem(tr("Deviation +/-"), 2);
    connect(
        m_tolModeCombo,
        &QComboBox::currentIndexChanged,
        this,
        &DimensionPalettePopup::onToleranceModeChanged
    );

    m_upperTolSpin = new QDoubleSpinBox(this);
    m_upperTolSpin->setRange(-1000.0, 1000.0);
    m_upperTolSpin->setDecimals(3);
    m_upperTolSpin->setSingleStep(0.05);
    m_upperTolSpin->setPrefix(QStringLiteral("+"));
    connect(
        m_upperTolSpin,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        &DimensionPalettePopup::applyChanges
    );

    m_lowerTolSpin = new QDoubleSpinBox(this);
    m_lowerTolSpin->setRange(-1000.0, 1000.0);
    m_lowerTolSpin->setDecimals(3);
    m_lowerTolSpin->setSingleStep(0.05);
    connect(
        m_lowerTolSpin,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        &DimensionPalettePopup::applyChanges
    );

    tolRow->addWidget(m_tolModeCombo);
    tolRow->addWidget(m_upperTolSpin);
    tolRow->addWidget(m_lowerTolSpin);
    root->addLayout(tolRow);

    // Flip Arrows & Show Units row
    auto* optRow = new QHBoxLayout();
    m_flipArrowsCheck = new QCheckBox(tr("Flip Arrows"), this);
    connect(m_flipArrowsCheck, &QCheckBox::toggled, this, &DimensionPalettePopup::applyChanges);
    m_showUnitsCheck = new QCheckBox(tr("Show Units"), this);
    connect(m_showUnitsCheck, &QCheckBox::toggled, this, &DimensionPalettePopup::applyChanges);
    optRow->addWidget(m_flipArrowsCheck);
    optRow->addWidget(m_showUnitsCheck);
    optRow->addStretch(1);
    root->addLayout(optRow);

    hide();
}

void DimensionPalettePopup::showForDimension(
    TechDraw::DrawViewDimension* dim,
    const QPoint& globalAnchor
)
{
    m_dim = dim;
    if (!m_dim) {
        hide();
        return;
    }

    loadFromDimension();
    adjustSize();
    positionNear(globalAnchor);
    show();
    raise();
}

void DimensionPalettePopup::keyPressEvent(QKeyEvent* event)
{
    if (event && event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void DimensionPalettePopup::splitFormatSpec(
    const QString& formatSpec,
    QString& outPrefix,
    int& outPrecision,
    QString& outSuffix,
    bool& outIsRef
)
{
    QString working = formatSpec;
    outIsRef = false;
    if (working.startsWith(QLatin1Char('(')) && working.endsWith(QLatin1Char(')'))
        && working.size() >= 2) {
        outIsRef = true;
        working = working.mid(1, working.size() - 2);
    }

    static const QRegularExpression kSpecRe(QStringLiteral("^(.*?)%(?:\\.(\\d+))?[fFgGw](.*)$"));
    const QRegularExpressionMatch match = kSpecRe.match(working);
    if (match.hasMatch()) {
        outPrefix = match.captured(1);
        outPrecision = match.captured(2).isEmpty() ? 2 : match.captured(2).toInt();
        outSuffix = match.captured(3);
    }
    else {
        outPrefix.clear();
        outPrecision = 2;
        outSuffix.clear();
    }
}

QString DimensionPalettePopup::buildFormatSpec(
    const QString& prefix,
    int precision,
    const QString& suffix,
    bool isReference
)
{
    QString core = QStringLiteral("%1%.%2f%3").arg(prefix).arg(precision).arg(suffix);
    if (isReference) {
        return QStringLiteral("(%1)").arg(core);
    }
    return core;
}

void DimensionPalettePopup::loadFromDimension()
{
    if (!m_dim) {
        return;
    }
    m_loading = true;

    QString prefix;
    int precision = 2;
    QString suffix;
    bool isRef = false;
    splitFormatSpec(QString::fromUtf8(m_dim->FormatSpec.getValue()), prefix, precision, suffix, isRef);

    m_prefixEdit->setText(prefix);
    m_precisionSpin->setValue(precision);
    m_suffixEdit->setText(suffix);
    m_referenceCheck->setChecked(isRef);
    m_theoreticalCheck->setChecked(m_dim->TheoreticalExact.getValue());
    m_showUnitsCheck->setChecked(m_dim->ShowUnits.getValue());

    const double over = m_dim->OverTolerance.getValue();
    const double under = m_dim->UnderTolerance.getValue();
    const bool equalTol = m_dim->EqualTolerance.getValue();
    if (std::abs(over) < 1e-9 && std::abs(under) < 1e-9) {
        m_tolModeCombo->setCurrentIndex(0);
    }
    else if (equalTol) {
        m_tolModeCombo->setCurrentIndex(1);
    }
    else {
        m_tolModeCombo->setCurrentIndex(2);
    }
    m_upperTolSpin->setValue(over);
    m_lowerTolSpin->setValue(under);
    m_upperTolSpin->setEnabled(m_tolModeCombo->currentIndex() != 0);
    m_lowerTolSpin->setEnabled(m_tolModeCombo->currentIndex() == 2);

    bool flipArrows = false;
    if (Gui::Application::Instance) {
        if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderDimension*>(
                Gui::Application::Instance->getViewProvider(m_dim)
            )) {
            flipArrows = vp->FlipArrowheads.getValue();
        }
    }
    m_flipArrowsCheck->setChecked(flipArrows);

    m_loading = false;
}

void DimensionPalettePopup::insertPrefixSymbol(const QString& symbol)
{
    m_prefixEdit->setText(m_prefixEdit->text() + symbol);
    applyChanges();
}

void DimensionPalettePopup::onToleranceModeChanged(int index)
{
    m_upperTolSpin->setEnabled(index != 0);
    m_lowerTolSpin->setEnabled(index == 2);
    if (!m_loading) {
        if (index == 1 && std::abs(m_upperTolSpin->value()) < 1e-9) {
            m_upperTolSpin->setValue(0.10);
        }
        else if (index == 2 && std::abs(m_upperTolSpin->value()) < 1e-9
                 && std::abs(m_lowerTolSpin->value()) < 1e-9) {
            m_upperTolSpin->setValue(0.10);
            m_lowerTolSpin->setValue(-0.05);
        }
        applyChanges();
    }
}

void DimensionPalettePopup::applyChanges()
{
    if (m_loading || !m_dim || !m_dim->getDocument()) {
        return;
    }

    App::Document* doc = m_dim->getDocument();
    doc->openTransaction("Format Dimension");

    const QString newSpec = buildFormatSpec(
        m_prefixEdit->text(),
        m_precisionSpin->value(),
        m_suffixEdit->text(),
        m_referenceCheck->isChecked()
    );
    m_dim->FormatSpec.setValue(newSpec.toUtf8().constData());
    m_dim->TheoreticalExact.setValue(m_theoreticalCheck->isChecked());
    m_dim->ShowUnits.setValue(m_showUnitsCheck->isChecked());

    const int tolMode = m_tolModeCombo->currentIndex();
    if (tolMode == 0) {
        m_dim->EqualTolerance.setValue(false);
        m_dim->OverTolerance.setValue(0.0);
        m_dim->UnderTolerance.setValue(0.0);
    }
    else if (tolMode == 1) {
        m_dim->EqualTolerance.setValue(true);
        m_dim->OverTolerance.setValue(std::abs(m_upperTolSpin->value()));
        m_dim->UnderTolerance.setValue(-std::abs(m_upperTolSpin->value()));
    }
    else {
        m_dim->EqualTolerance.setValue(false);
        m_dim->OverTolerance.setValue(m_upperTolSpin->value());
        m_dim->UnderTolerance.setValue(m_lowerTolSpin->value());
    }

    if (Gui::Application::Instance) {
        if (auto* vp = dynamic_cast<TechDrawGui::ViewProviderDimension*>(
                Gui::Application::Instance->getViewProvider(m_dim)
            )) {
            vp->FlipArrowheads.setValue(m_flipArrowsCheck->isChecked());
        }
    }

    m_dim->recomputeFeature();
    doc->commitTransaction();
}

void DimensionPalettePopup::positionNear(const QPoint& globalAnchor)
{
    QPoint target(globalAnchor.x() + kOffsetX, globalAnchor.y() - height() - kOffsetY);
    if (const QScreen* screen = QGuiApplication::screenAt(globalAnchor)) {
        const QRect avail = screen->availableGeometry();
        if (target.y() < avail.top()) {
            target.setY(globalAnchor.y() + kOffsetY);
        }
        const int clampedX = std::clamp(
            target.x(),
            avail.left() + 6,
            std::max(avail.left() + 6, avail.right() - width() - 6)
        );
        const int clampedY = std::clamp(
            target.y(),
            avail.top() + 6,
            std::max(avail.top() + 6, avail.bottom() - height() - 6)
        );
        target = QPoint(clampedX, clampedY);
    }
    move(target);
}

}  // namespace DrawingGui
