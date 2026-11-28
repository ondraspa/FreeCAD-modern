# SPDX-License-Identifier: LGPL-2.1-or-later

import FreeCAD
import FreeCADGui as Gui

try:
    import DrawingGui  # Registers Drawing_MakeDrawingFromPart and cross-workbench integrations at startup
except Exception:
    pass


class DrawingWorkbench(Workbench):
    """Modern SolidWorks/Onshape-inspired technical drawing workbench."""

    def __init__(self):
        self.__class__.Icon = (
            FreeCAD.getResourceDir()
            + "Mod/Drawing/Resources/icons/DrawingWorkbench.svg"
        )
        self.__class__.MenuText = "Drawing"
        self.__class__.ToolTip = "Modern technical drawing workbench"

    def Initialize(self):
        import DrawingGui  # noqa: F401

    def GetClassName(self):
        return "DrawingGui::Workbench"


Gui.addWorkbench(DrawingWorkbench())
