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

#include <Gui/WorkbenchManipulator.h>
#include <Mod/Drawing/DrawingGlobal.h>

namespace DrawingGui
{

/**
 * Injects "Make Drawing from Part/Assembly" into the File menu across PartDesign, Part,
 * and Assembly workbenches without creating compile-time coupling.
 */
class DrawingGuiExport WorkbenchManipulator: public Gui::WorkbenchManipulator
{
protected:
    void modifyMenuBar(Gui::MenuItem* menuBar) override;
};

}  // namespace DrawingGui
