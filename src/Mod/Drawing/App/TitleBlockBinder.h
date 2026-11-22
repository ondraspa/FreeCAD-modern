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

#include <Mod/Drawing/DrawingGlobal.h>

namespace App
{
class Document;
class DocumentObject;
}  // namespace App

namespace TechDraw
{
class DrawPage;
}  // namespace TechDraw

namespace Drawing
{

struct TitleBlockMetadata
{
    std::string title;
    std::string subtitle;
    std::string author;
    std::string company;
    std::string partNumber;
    std::string material;
    std::string mass;
    std::string scale;
    std::string sheetIndex;
    std::string date;
    std::string projection;
};

class DrawingExport TitleBlockBinder
{
public:
    /// Format a floating-point drawing scale into clean drafting ratio syntax (e.g. "1:2", "2:1", "1:1").
    static std::string formatScaleRatio(double scale);

    /// Collect document, sheet, and primary part/assembly metadata.
    static TitleBlockMetadata collectMetadata(
        App::Document* doc,
        TechDraw::DrawPage* page,
        App::DocumentObject* primarySource = nullptr
    );

    /// Populate matching EditableTexts fields on page's DrawTemplate while preserving manual user edits.
    /// Returns the number of template fields updated.
    static int bindTitleBlock(
        TechDraw::DrawPage* page,
        App::DocumentObject* primarySource = nullptr,
        bool forceOverwrite = false
    );

    /// Refresh sheet numbering and scale across all DrawPages in a document.
    static void refreshAllPagesInDocument(App::Document* doc);
};

}  // namespace Drawing
