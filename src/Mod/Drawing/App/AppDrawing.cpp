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

#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <Base/PyObjectBase.h>
#include <CXX/Extensions.hxx>

namespace Drawing
{

class Module: public Py::ExtensionModule<Module>
{
public:
    Module()
        : Py::ExtensionModule<Module>("Drawing")
    {
        initialize("Modernized Drawing application helpers backed by TechDraw.");
    }

    ~Module() override = default;
};

static PyObject* initModule()
{
    return Base::Interpreter().addModule(new Module);
}

}  // namespace Drawing

#include "DraftingTools.h"

PyMOD_INIT_FUNC(Drawing)
{
    try {
        Base::Interpreter().loadModule("Part");
        Base::Interpreter().loadModule("Spreadsheet");
        Base::Interpreter().loadModule("TechDraw");
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        PyMOD_Return(nullptr);
    }

    PyObject* mod = Drawing::initModule();
    Base::Console().log("Loading Drawing module... done\n");

    Drawing::DrawViewDatum::init();
    Drawing::DrawViewGDT::init();

    PyMOD_Return(mod);
}
