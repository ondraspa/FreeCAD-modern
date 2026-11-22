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
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/PropertyStandard.h>
#include <Mod/Part/App/PartFeature.h>
#include <Mod/TechDraw/App/DrawPage.h>
#include <Mod/TechDraw/App/DrawTemplate.h>
#include <Mod/TechDraw/App/DrawViewPart.h>

#include "TitleBlockBinder.h"

namespace Drawing
{

namespace
{

constexpr const char* kPropAutoBoundFields = "AutoBoundFields";

std::string normalizeFieldKey(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

std::string currentIsoDate()
{
    const std::time_t now = std::time(nullptr);
    std::tm tmBuf {};
#if defined(_WIN32)
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d");
    return oss.str();
}

bool looksLikeTemplatePlaceholder(const std::string& keyNorm, const std::string& value)
{
    if (value.empty()) {
        return true;
    }
    const std::string valNorm = normalizeFieldKey(value);
    if (valNorm == keyNorm) {
        return true;
    }
    // Common FreeCAD ISO5457 / ASME Y14.1 placeholder strings
    return value == "Title" || value == "TITLE" || value == "Subtitle" || value == "SUBTITLE"
        || value == "ISO 7200 titleblock" || value == "ST1" || value == "ST2"
        || value == "Author" || value == "Creator" || value == "DrawnBy" || value == "AUTHOR NAME"
        || value == "A. Nemesis" || value == "Company" || value == "CompanyName"
        || value == "Legal Owner" || value == "YYYY-MM-DD" || value == "MM/DD/YYYY"
        || value == "DATE" || value == "1:1" || value == "1 : 1" || value == "1 / 1"
        || value == "1/1" || value == "0 / 1" || value == "0/1" || value == "X / Y"
        || value == "Material" || value == "Weight" || value == "WEIGHT" || value == "Part Number"
        || value == "Drawing Number" || value == "000-000" || value == "DN"
        || value.find("FreeCAD") != std::string::npos;
}

App::DocumentObject* findPrimarySourceOnPage(TechDraw::DrawPage* page)
{
    if (!page) {
        return nullptr;
    }
    for (App::DocumentObject* viewObj : page->Views.getValues()) {
        auto* dvp = dynamic_cast<TechDraw::DrawViewPart*>(viewObj);
        if (!dvp) {
            continue;
        }
        const auto sources = dvp->getAllSources();
        if (!sources.empty() && sources.front()) {
            return sources.front();
        }
    }
    return nullptr;
}

std::string extractMaterialName(App::DocumentObject* obj)
{
    if (!obj) {
        return {};
    }
    if (auto* strProp = dynamic_cast<App::PropertyString*>(obj->getPropertyByName("MaterialName"))) {
        if (!strProp->getStrValue().empty()) {
            return strProp->getStrValue();
        }
    }
    if (auto* mapProp = dynamic_cast<App::PropertyMap*>(obj->getPropertyByName("Material"))) {
        const auto& vals = mapProp->getValues();
        if (auto it = vals.find("Name"); it != vals.end() && !it->second.empty()) {
            return it->second;
        }
        if (auto it = vals.find("CardName"); it != vals.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return {};
}

std::string computeVolumeOrMassText(App::DocumentObject* obj)
{
    auto* partFeat = dynamic_cast<Part::Feature*>(obj);
    if (!partFeat) {
        return {};
    }
    const TopoDS_Shape shape = partFeat->Shape.getValue();
    if (shape.IsNull()) {
        return {};
    }
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    const double volumeMm3 = std::abs(props.Mass());
    if (volumeMm3 <= 1e-6) {
        return {};
    }
    // Check if density (kg/m^3) is available in a Material map; otherwise report volume in cm^3.
    if (auto* mapProp = dynamic_cast<App::PropertyMap*>(obj->getPropertyByName("Material"))) {
        const auto& vals = mapProp->getValues();
        if (auto it = vals.find("Density"); it != vals.end() && !it->second.empty()) {
            try {
                const double densityKgM3 = std::stod(it->second);
                if (densityKgM3 > 0.0) {
                    const double massKg = (volumeMm3 * 1e-9) * densityKgM3;
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << massKg << " kg";
                    return oss.str();
                }
            }
            catch (...) {
                // Fall through to volume reporting.
            }
        }
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (volumeMm3 / 1000.0) << " cm3";
    return oss.str();
}

std::string valueForNormalizedKey(const std::string& normKey, const TitleBlockMetadata& meta)
{
    if (normKey == "title" || normKey == "drawingtitle1" || normKey == "drawingtitle"
        || normKey == "fc_title") {
        return meta.title;
    }
    if (normKey == "supplementarytitle1" || normKey == "subtitle" || normKey == "drawingtitle2") {
        return meta.subtitle;
    }
    if (normKey == "creator" || normKey == "drawnby" || normKey == "authorname"
        || normKey == "author" || normKey == "designedby") {
        return meta.author;
    }
    if (normKey == "companyname" || normKey == "company" || normKey == "legalowner1"
        || normKey == "organization" || normKey == "organisation" || normKey == "owner") {
        return meta.company;
    }
    if (normKey == "drawingnumber" || normKey == "partnumber" || normKey == "number") {
        return meta.partNumber;
    }
    if (normKey == "partmaterial" || normKey == "material") {
        return meta.material;
    }
    if (normKey == "weight" || normKey == "mass") {
        return meta.mass;
    }
    if (normKey == "scale") {
        return meta.scale;
    }
    if (normKey == "sheetnumber" || normKey == "sheet") {
        return meta.sheetIndex;
    }
    if (normKey == "dateofissue" || normKey == "creationdate" || normKey == "date") {
        return meta.date;
    }
    if (normKey == "projection") {
        return meta.projection;
    }
    return {};
}

}  // namespace

std::string TitleBlockBinder::formatScaleRatio(double scale)
{
    if (scale <= 1e-6) {
        return "1:1";
    }
    if (scale >= 0.999) {
        const double rounded = std::round(scale * 100.0) / 100.0;
        if (std::abs(rounded - std::round(rounded)) < 1e-3) {
            return std::to_string(static_cast<int>(std::round(rounded))) + ":1";
        }
        std::ostringstream oss;
        oss << std::setprecision(3) << rounded << ":1";
        return oss.str();
    }
    const double inv = 1.0 / scale;
    const double roundedInv = std::round(inv * 100.0) / 100.0;
    if (std::abs(roundedInv - std::round(roundedInv)) < 1e-3) {
        return "1:" + std::to_string(static_cast<int>(std::round(roundedInv)));
    }
    std::ostringstream oss;
    oss << "1:" << std::setprecision(3) << roundedInv;
    return oss.str();
}

TitleBlockMetadata TitleBlockBinder::collectMetadata(
    App::Document* doc,
    TechDraw::DrawPage* page,
    App::DocumentObject* primarySource
)
{
    TitleBlockMetadata meta;
    if (!doc && page) {
        doc = page->getDocument();
    }
    if (!primarySource && page) {
        primarySource = findPrimarySourceOnPage(page);
    }

    if (primarySource) {
        meta.title = primarySource->Label.getStrValue();
        meta.partNumber = primarySource->getNameInDocument();
        meta.material = extractMaterialName(primarySource);
        meta.mass = computeVolumeOrMassText(primarySource);
    }

    if (doc) {
        if (meta.title.empty()) {
            meta.title = doc->Label.getStrValue();
        }
        meta.author = doc->CreatedBy.getStrValue();
        meta.company = doc->Company.getStrValue();
        const std::string lastMod = doc->LastModifiedDate.getStrValue();
        if (lastMod.size() >= 10) {
            meta.date = lastMod.substr(0, 10);
        }
        else {
            meta.date = currentIsoDate();
        }

        const auto pages = doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId());
        int pageIdx = 1;
        const int totalPages = std::max(1, static_cast<int>(pages.size()));
        if (page) {
            for (std::size_t i = 0; i < pages.size(); ++i) {
                if (pages[i] == page) {
                    pageIdx = static_cast<int>(i) + 1;
                    break;
                }
            }
        }
        meta.sheetIndex = std::to_string(pageIdx) + " / " + std::to_string(totalPages);
    }
    else {
        meta.date = currentIsoDate();
        meta.sheetIndex = "1 / 1";
    }

    if (page) {
        meta.scale = formatScaleRatio(page->Scale.getValue());
        meta.subtitle = page->Label.getStrValue();
        meta.projection = (page->ProjectionType.getValue() == 1) ? "Third angle" : "First angle";
    }
    else {
        meta.scale = "1:1";
    }

    return meta;
}

int TitleBlockBinder::bindTitleBlock(
    TechDraw::DrawPage* page,
    App::DocumentObject* primarySource,
    bool forceOverwrite
)
{
    if (!page) {
        return 0;
    }
    auto* tmpl = dynamic_cast<TechDraw::DrawTemplate*>(page->Template.getValue());
    if (!tmpl) {
        return 0;
    }

    const TitleBlockMetadata meta = collectMetadata(page->getDocument(), page, primarySource);

    auto* autoBoundProp
        = dynamic_cast<App::PropertyMap*>(tmpl->getPropertyByName(kPropAutoBoundFields));
    if (!autoBoundProp) {
        autoBoundProp = dynamic_cast<App::PropertyMap*>(tmpl->addDynamicProperty(
            "App::PropertyMap",
            kPropAutoBoundFields,
            "Drawing",
            "Tracks previously auto-bound title block field values to preserve manual overrides"
        ));
    }

    std::map<std::string, std::string> texts = tmpl->EditableTexts.getValues();
    std::map<std::string, std::string> prevAuto = autoBoundProp ? autoBoundProp->getValues()
                                                                : std::map<std::string, std::string> {};

    int updatedCount = 0;
    for (auto& [key, currentVal] : texts) {
        const std::string normKey = normalizeFieldKey(key);
        const std::string desired = valueForNormalizedKey(normKey, meta);
        if (desired.empty()) {
            continue;
        }

        const auto prevIt = prevAuto.find(key);
        const bool matchesPrevAuto = (prevIt != prevAuto.end() && prevIt->second == currentVal);
        const bool canUpdate
            = forceOverwrite || matchesPrevAuto || looksLikeTemplatePlaceholder(normKey, currentVal);

        if (canUpdate && currentVal != desired) {
            currentVal = desired;
            prevAuto[key] = desired;
            ++updatedCount;
        }
        else if (canUpdate) {
            prevAuto[key] = desired;
        }
    }

    if (autoBoundProp) {
        autoBoundProp->setValues(prevAuto);
    }
    if (updatedCount > 0) {
        tmpl->EditableTexts.setValues(texts);
    }
    return updatedCount;
}

void TitleBlockBinder::refreshAllPagesInDocument(App::Document* doc)
{
    if (!doc) {
        return;
    }
    for (App::DocumentObject* obj : doc->getObjectsOfType(TechDraw::DrawPage::getClassTypeId())) {
        if (auto* page = dynamic_cast<TechDraw::DrawPage*>(obj)) {
            bindTitleBlock(page, nullptr, false);
        }
    }
}

}  // namespace Drawing
