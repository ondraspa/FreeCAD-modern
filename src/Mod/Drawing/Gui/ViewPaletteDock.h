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

#include <vector>

#include <QWidget>
#include <fastsignals/signal.h>

#include <Mod/Drawing/App/ProjectionMath.h>
#include <Mod/Drawing/DrawingGlobal.h>

class QCheckBox;
class QComboBox;
class QPushButton;

namespace App
{
class Document;
class DocumentObject;
}  // namespace App

namespace DrawingGui
{

/**
 * SolidWorks-inspired Right Dock ("View Palette") listing drawable Bodies, Parts,
 * and Assemblies in the active document alongside 7 standard orientation cards.
 * Clicking any orientation launches HandlerInsertView on the active drawing sheet.
 */
class DrawingGuiExport ViewPaletteDock: public QWidget
{
    Q_OBJECT

public:
    explicit ViewPaletteDock(QWidget* parent = nullptr);
    ~ViewPaletteDock() override;

    App::DocumentObject* selectedSource() const;
    bool autoStartProjectedEnabled() const;

public Q_SLOTS:
    void refreshSources();

private:
    void startInsertOrientation(Drawing::StandardOrientation orientation);

    QComboBox* m_sourceCombo {nullptr};
    QCheckBox* m_autoProjectedCheck {nullptr};
    QCheckBox* m_autoCentermarksCheck {nullptr};
    std::vector<App::DocumentObject*> m_sources;

    fastsignals::scoped_connection m_connNewObj;
    fastsignals::scoped_connection m_connDelObj;
    fastsignals::scoped_connection m_connActiveDoc;
};

}  // namespace DrawingGui
