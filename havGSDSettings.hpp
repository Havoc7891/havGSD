/*
havGSDSettings.hpp

ABOUT

Havoc's Task List Plugin for C++ Projects in CodeLite.

TODO

- Improve error handling.
- Add support for externally modified files.

REVISION HISTORY

v0.1 (2025-03-02) - First release.

LICENSE

MIT License

Copyright (c) 2025 René Nicolaus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef HAVGSDSETTINGS_HPP
#define HAVGSDSETTINGS_HPP

#include <wx/colour.h>
#include <wx/filename.h>
#include <wx/string.h>

#include <unordered_map>

#include "fileutils.h"
#include "JSON.h"

struct havGSDSettingEntry
{
    wxString mKeyword;
    wxString mColor;

    havGSDSettingEntry() = default;

    havGSDSettingEntry(const wxString& keyword, const wxString& color) : mKeyword(keyword), mColor(color) {}

    JSONItem To() const
    {
        JSONItem object = JSONItem::createObject();
        object.addProperty("Keyword", mKeyword);
        object.addProperty("Color", mColor);
        return object;
    }

    void From(const JSONItem& object)
    {
        mKeyword = object["Keyword"].toString();
        mColor = object["Color"].toString();
    }
};

struct havGSDSettingsObject
{
    std::unordered_map<wxString, havGSDSettingEntry> mSettingEntries;
    bool mShowAbsoluteFilePath;
};

class havGSDSettings
{
public:
    havGSDSettings() = default;
    ~havGSDSettings() = default;

    void Load(const wxFileName& fileName)
    {
        // Reset settings
        mSettingsObject.mShowAbsoluteFilePath = false;
        mSettingsObject.mSettingEntries.clear();

        // Create configuration file with default settings, if configuration file doesn't exist
        if (!fileName.Exists())
        {
            wxFileName::Mkdir(fileName.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

            JSON root(cJSON_Object);

            root.toElement().addProperty("ShowAbsoluteFilePath", false);

            JSONItem array = root.toElement().AddArray("Entries");

            for (const auto& [keyword, color] : mDefaultKeywordsWithColors)
            {
                JSONItem value = JSONItem::createObject();
                value.addProperty("Keyword", keyword);
                value.addProperty("Color", color);
                array.arrayAppend(value);
            }

            root.save(fileName);
        }

        // Try to load configuration file
        JSON root(fileName);
        if (!root.isOk())
        {
            return;
        }

        JSONItem rootItem = root.toElement();

        mSettingsObject.mShowAbsoluteFilePath = rootItem["ShowAbsoluteFilePath"].toBool();

        int arraySize = rootItem["Entries"].arraySize();

        for (int index = 0; index < arraySize; ++index)
        {
            havGSDSettingEntry settingEntry;
            settingEntry.From(rootItem["Entries"][index]);
            mSettingsObject.mSettingEntries.insert({ settingEntry.mKeyword, settingEntry });
        }
    }

    void Save(const wxFileName& fileName)
    {
        if (!fileName.IsOk())
        {
            return;
        }

        // {
        JSON root(cJSON_Object);

        // "ShowAbsoluteFilePath": false,
        root.toElement().addProperty("ShowAbsoluteFilePath", mSettingsObject.mShowAbsoluteFilePath);

        // "Entries": [
        JSONItem array = root.toElement().AddArray("Entries");
        for (const auto& settingEntry : mSettingsObject.mSettingEntries)
        {
            // { "Keyword": "TODO", "Color": "#0094FF" }
            array.arrayAppend(settingEntry.second.To());
        }
        // ]

        // }
        root.save(fileName);
    }

    havGSDSettingsObject& GetSettingsObject() { return mSettingsObject; }

    const std::unordered_map<wxString, wxColour>& GetDefaultKeywordsWithColors() const { return mDefaultKeywordsWithColors; }

private:
    havGSDSettingsObject mSettingsObject;

    std::unordered_map<wxString, wxColour> mDefaultKeywordsWithColors =
    {
        { "TODO", "#3498DB" },
        { "FIXME", "#E74C3C" },
        { "HACK", "#E67E22" },
        { "NOTE", "#F1C40F" },
        { "OPTIMIZE", "#2ECC71" },
        { "BUG", "#9B59B6" },
        { "ATTN", "#00BCD4" }
    };
};

#endif
