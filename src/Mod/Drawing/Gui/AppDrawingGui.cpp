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

#include <memory>

#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <Base/PyObjectBase.h>
#include <CXX/Extensions.hxx>
#include <Gui/Application.h>
#include <Gui/WorkbenchManipulator.h>

#include "DrawingUpdateController.h"
#include "Workbench.h"
#include "WorkbenchManipulator.h"

void CreateDrawingCommands();

namespace DrawingGui
{

class Module: public Py::ExtensionModule<Module>
{
public:
    Module()
        : Py::ExtensionModule<Module>("DrawingGui")
    {
        initialize("Modernized Drawing GUI workbench backed by TechDraw.");
    }

    ~Module() override = default;
};

static PyObject* initModule()
{
    return Base::Interpreter().addModule(new Module);
}

}  // namespace DrawingGui

static void loadDrawingResources()
{
    Q_INIT_RESOURCE(Drawing);
}

PyMOD_INIT_FUNC(DrawingGui)
{
    if (!Gui::Application::Instance) {
        PyErr_SetString(PyExc_ImportError, "Cannot load DrawingGui module in console application.");
        PyMOD_Return(nullptr);
    }

    try {
        Base::Interpreter().loadModule("Drawing");
        Base::Interpreter().loadModule("TechDraw");
        Base::Interpreter().loadModule("TechDrawGui");
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        PyMOD_Return(nullptr);
    }

    PyObject* mod = DrawingGui::initModule();
    Base::Console().log("Loading DrawingGui module... done\n");

    loadDrawingResources();
    CreateDrawingCommands();
    DrawingGui::Workbench::init();
    DrawingGui::Workbench::registerModernUiIntegrations();
    Gui::WorkbenchManipulator::installManipulator(
        std::make_shared<DrawingGui::WorkbenchManipulator>()
    );
    DrawingGui::DrawingUpdateController::instance()->install();

    PyMOD_Return(mod);
}
