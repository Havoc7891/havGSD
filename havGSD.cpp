/*
havGSD.cpp

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

#include "havGSD.hpp"

#include "havGSDSettingsDialog.hpp"

#include "event_notifier.h"
#include "ieditor.h"
#include "imanager.h"
#include "project.h"
#include "workspace.h"

#include <wx/colour.h>
#include <wx/filefn.h>
#include <wx/regex.h>
#include <wx/txtstrm.h>
#include <wx/sizer.h>
#include <wx/variant.h>
#include <wx/vector.h>
#include <wx/wfstream.h>
#include <wx/xrc/xmlres.h>

#include <unordered_map>
#include <unordered_set>

CL_PLUGIN_API IPlugin* CreatePlugin(IManager* manager) { return new havGSD(manager); }

CL_PLUGIN_API PluginInfo* GetPluginInfo()
{
    static PluginInfo info;
    info.SetAuthor(wxT("Ren\u00e9 \"Havoc\" Nicolaus"));
    info.SetName(wxT("havGSD"));
    info.SetDescription(_("A task list for C++ projects in CodeLite"));
    info.SetVersion(wxT("v0.1"));
    return &info;
}

CL_PLUGIN_API int GetPluginInterfaceVersion() { return PLUGIN_INTERFACE_VERSION; }

havGSD::havGSD(IManager* manager) : IPlugin(manager)
{
    m_longName = _("havGSD for CodeLite");
    m_shortName = wxT("havGSD");

    EventNotifier::Get()->Bind(wxEVT_WORKSPACE_LOADED, &havGSD::OnWorkspaceOpened, this);
    EventNotifier::Get()->Bind(wxEVT_WORKSPACE_CLOSED, &havGSD::OnWorkspaceClosed, this);
    EventNotifier::Get()->Bind(wxEVT_ACTIVE_PROJECT_CHANGED, &havGSD::OnActiveProjectChanged, this);
    EventNotifier::Get()->Bind(wxEVT_PROJ_RENAMED, &havGSD::OnProjectRenamed, this);
    EventNotifier::Get()->Bind(wxEVT_PROJ_REMOVED, &havGSD::OnProjectRemoved, this);
    EventNotifier::Get()->Bind(wxEVT_FILE_SAVED, &havGSD::OnFileSaved, this);
    EventNotifier::Get()->Bind(wxEVT_FILE_RENAMED, &havGSD::OnFileRenamed, this);
    EventNotifier::Get()->Bind(wxEVT_FILE_DELETED, &havGSD::OnFileDeleted, this);

    mHavGSDPanel = new wxPanel(m_mgr->GetMainPanel(), wxID_ANY, wxDefaultPosition,
                               wxDefaultSize, wxTAB_TRAVERSAL, m_mgr->GetMainPanel()->GetName());

    wxBoxSizer* boxSizer = new wxBoxSizer(wxHORIZONTAL);
    mHavGSDPanel->SetSizer(boxSizer);

    mThemedListCtrlForTasks = new clThemedListCtrl(mHavGSDPanel, wxID_ANY, wxDefaultPosition,
                                                   wxDLG_UNIT(mHavGSDPanel, wxSize(-1, -1)),
                                                   wxDV_COLUMN_WIDTH_NEVER_SHRINKS | wxDV_ROW_LINES | wxDV_SINGLE | get_border_simple_theme_aware_bit());

    mThemedListCtrlForTasks->Bind(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED, &havGSD::OnItemActived, this);

    boxSizer->Add(mThemedListCtrlForTasks, 1, wxALL | wxEXPAND, WXC_FROM_DIP(5));

    mThemedListCtrlForTasks->AddHeader(_("Type"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);
    mThemedListCtrlForTasks->AddHeader(_("Description"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);
    mThemedListCtrlForTasks->AddHeader(_("Project"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);
    mThemedListCtrlForTasks->AddHeader(_("File"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);
    mThemedListCtrlForTasks->AddHeader(_("Line"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);

    mTabToggler.reset(new clTabTogglerHelper(_("havGSD"), mHavGSDPanel, _("havGSD"), NULL));

    mHavGSDSettings = std::make_unique<havGSDSettings>();
    mHavGSDSettings->Load(GetHavGSDSettingsFile());

    havGSDSettingsObject& settingsObject = mHavGSDSettings->GetSettingsObject();
    mShowAbsoluteFilePath = settingsObject.mShowAbsoluteFilePath;
}

havGSD::~havGSD() {}

void havGSD::CreateToolBar(clToolBarGeneric* toolbar) { wxUnusedVar(toolbar); }

void havGSD::CreatePluginMenu(wxMenu* pluginsMenu)
{
    wxMenu* menu = new wxMenu();
    menu->Append(XRCID("ID_HAVGSD_SETTINGS"), _("Settings..."));
    pluginsMenu->Append(wxID_ANY, _("havGSD"), menu);
    menu->Bind(wxEVT_MENU,
        [&](wxCommandEvent& event) {
            std::unordered_map<wxString, wxColour> keywordColors;

            havGSDSettingsObject& settingsObject = mHavGSDSettings->GetSettingsObject();

            for (const auto& settingEntry : settingsObject.mSettingEntries)
            {
                keywordColors.emplace(settingEntry.second.mKeyword, wxColour(settingEntry.second.mColor));
            }

            bool showAbsoluteFilePath = settingsObject.mShowAbsoluteFilePath;

            havGSDSettingsDialog settingsDialog(EventNotifier::Get()->TopFrame(), mHavGSDSettings->GetDefaultKeywordsWithColors(), keywordColors, showAbsoluteFilePath);
            if (settingsDialog.ShowModal() == wxID_OK)
            {
                settingsObject.mShowAbsoluteFilePath = showAbsoluteFilePath;

                settingsObject.mSettingEntries.clear();

                for (const auto& [keyword, color] : keywordColors)
                {
                    settingsObject.mSettingEntries.emplace(keyword, havGSDSettingEntry(keyword, color.GetAsString(wxC2S_HTML_SYNTAX)));
                }

                mShowAbsoluteFilePath = showAbsoluteFilePath;

                mHavGSDSettings->Save(GetHavGSDSettingsFile());

                RefreshKeywordList();
            }
        },
        XRCID("ID_HAVGSD_SETTINGS"));
}

void havGSD::UnPlug()
{
    EventNotifier::Get()->Unbind(wxEVT_WORKSPACE_LOADED, &havGSD::OnWorkspaceOpened, this);
    EventNotifier::Get()->Unbind(wxEVT_WORKSPACE_CLOSED, &havGSD::OnWorkspaceClosed, this);
    EventNotifier::Get()->Unbind(wxEVT_ACTIVE_PROJECT_CHANGED, &havGSD::OnActiveProjectChanged, this);
    EventNotifier::Get()->Unbind(wxEVT_PROJ_RENAMED, &havGSD::OnProjectRenamed, this);
    EventNotifier::Get()->Unbind(wxEVT_PROJ_REMOVED, &havGSD::OnProjectRemoved, this);
    EventNotifier::Get()->Unbind(wxEVT_FILE_SAVED, &havGSD::OnFileSaved, this);
    EventNotifier::Get()->Unbind(wxEVT_FILE_RENAMED, &havGSD::OnFileRenamed, this);
    EventNotifier::Get()->Unbind(wxEVT_FILE_DELETED, &havGSD::OnFileDeleted, this);

    mThemedListCtrlForTasks->Unbind(wxEVT_COMMAND_DATAVIEW_ITEM_ACTIVATED, &havGSD::OnItemActived, this);

    mTabToggler.reset();
}

void havGSD::OnWorkspaceOpened(clWorkspaceEvent& event)
{
    mWorkspaceType = event.GetWorkspaceType();

    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnWorkspaceClosed(clWorkspaceEvent& event)
{
    mThemedListCtrlForTasks->DeleteAllItems();

    event.Skip(true);
}

void havGSD::OnActiveProjectChanged(clProjectSettingsEvent& event)
{
    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnProjectRenamed(clCommandEvent& event)
{
    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnProjectRemoved(clCommandEvent& event)
{
    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnFileSaved(clCommandEvent& event)
{
    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnFileRenamed(clFileSystemEvent& event)
{
    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnFileDeleted(clFileSystemEvent& event)
{
    RefreshKeywordList();

    event.Skip(true);
}

void havGSD::OnItemActived(wxDataViewEvent& event)
{
    wxDataViewItem item = event.GetItem();
    CHECK_ITEM_RET(item);

    wxFileName fileName = mFileItems[mThemedListCtrlForTasks->ItemToRow(item)].mFilePath;

    if (fileName.Exists())
    {
        ProjectPtr project = m_mgr->GetWorkspace()->GetActiveProject();

        m_mgr->OpenFile(fileName.GetFullPath(), project->GetName(), wxAtoi(mFileItems[mThemedListCtrlForTasks->ItemToRow(item)].mLine) - 1);
    }
}

wxString havGSD::GetHavGSDSettingsFile()
{
    wxFileName filename(clStandardPaths::Get().GetUserDataDir(), "havgsd.conf");
    filename.AppendDir("config");
    return filename.GetFullPath();
}

void havGSD::SearchKeywordsInFiles(const std::vector<wxFileName>& files, const std::vector<wxString>& keywords, const wxString& projectName)
{
    // Reset stored file items
    mFileItems.clear();

    if (keywords.empty())
    {
        // Keyword list is empty
        return;
    }

    // Build regex to match keywords in comments
    wxString regexPattern = "\\b(" + wxJoin(keywords, '|') + ")\\b";

    // Case-insensitive regex
    wxRegEx regex;
    if (!regex.Compile(regexPattern, wxRE_ICASE))
    {
        // Invalid generated regex
        return;
    }

    for (const auto& file : files)
    {
        if (!file.FileExists())
        {
            // File doesn't exist
            continue;
        }

        wxFileInputStream fileStream(file.GetFullPath());
        if (!fileStream.IsOk())
        {
            // Couldn't open file
            continue;
        }

        wxTextInputStream textStream(fileStream);
        wxString line;
        std::size_t lineNumber = 0;
        bool inBlockComment = false;
        wxString commentBlock;
        std::vector<std::pair<std::size_t, wxString>> blockLines; // Store line numbers and contents

        // Track which keywords have already been processed for each line
        std::unordered_map<std::size_t, std::unordered_set<wxString>> matchedKeywordsPerLine;

        while (!fileStream.Eof())
        {
            line = textStream.ReadLine();
            ++lineNumber;

            // Check for single-line comments
            if (line.Contains("//"))
            {
                if (regex.Matches(line))
                {
                    // Capture all matches in the line
                    for (std::size_t index = 0; index < regex.GetMatchCount(); ++index)
                    {
                        wxString matchedText = regex.GetMatch(line, index);

                        // Check if the matched word is actually in the keyword list
                        if (std::find(keywords.begin(), keywords.end(), matchedText) == keywords.end())
                        {
                            // Ignore unknown matches
                            continue;
                        }

                        // Ensure we store multiple occurrences but avoid redundant re-processing
                        if (matchedKeywordsPerLine[lineNumber].count(matchedText) == 0)
                        {
                            matchedKeywordsPerLine[lineNumber].insert(matchedText);

                            havGSDFileItem fileItem;
                            fileItem.mType = matchedText.Upper().Trim(false).Trim();
                            fileItem.mProjectName = projectName;
                            fileItem.mFileName = file.GetFullName();
                            fileItem.mFilePath = file.GetFullPath();
                            fileItem.mDescription = line.Trim(false).Trim();
                            fileItem.mLine = wxString::Format("%zu", lineNumber);

                            mFileItems.push_back(fileItem);
                        }
                    }
                }

                // Skip further processing for single-line comments
                continue;
            }

            // Detect start of a multi-line block comment (/* ... */)
            if (line.Contains("/*"))
            {
                inBlockComment = true;
                commentBlock = line + "\n"; // Start collecting comment block
                blockLines.clear(); // Reset stored block lines
                blockLines.emplace_back(lineNumber, line); // Store first line

                continue;
            }

            // If inside a block comment, collect lines and store line numbers
            if (inBlockComment)
            {
                commentBlock += line + "\n"; // Append current line
                blockLines.emplace_back(lineNumber, line); // Store line with number

                // Detect end of block comment (*/)
                if (line.Contains("*/"))
                {
                    inBlockComment = false;

                    // Search for keywords inside each stored line
                    for (const auto& [commentLineNumber, commentLine] : blockLines)
                    {
                        if (regex.Matches(commentLine))
                        {
                            for (std::size_t index = 0; index < regex.GetMatchCount(); ++index)
                            {
                                wxString matchedText = regex.GetMatch(commentLine, index);

                                // Check if the match is known
                                if (std::find(keywords.begin(), keywords.end(), matchedText) == keywords.end())
                                {
                                    // Skip unknown matches
                                    continue;
                                }

                                // Ensure we capture multiple occurrences but avoid duplicate entries from multi-line processing
                                if (matchedKeywordsPerLine[commentLineNumber].count(matchedText) == 0)
                                {
                                    matchedKeywordsPerLine[commentLineNumber].insert(matchedText);

                                    wxString tempCommentLine = commentLine;

                                    havGSDFileItem fileItem;
                                    fileItem.mType = matchedText.Upper().Trim(false).Trim();
                                    fileItem.mProjectName = projectName;
                                    fileItem.mFileName = file.GetFullName();
                                    fileItem.mFilePath = file.GetFullPath();
                                    fileItem.mDescription = tempCommentLine.Trim(false).Trim();
                                    fileItem.mLine = wxString::Format("%zu", commentLineNumber);

                                    mFileItems.push_back(fileItem);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void havGSD::RefreshKeywordList()
{
    mThemedListCtrlForTasks->DeleteAllItems();

    if (!m_mgr->GetWorkspace()->IsOpen() ||
        mWorkspaceType != "C++")
    {
        return;
    }

    ProjectPtr project = m_mgr->GetWorkspace()->GetActiveProject();

    if (project)
    {
        wxVector<wxVariant> items;
        std::vector<wxFileName> files;

        project->GetFilesAsVectorOfFileName(files, true);

        if (!files.empty())
        {
            havGSDSettingsObject& settingsObject = mHavGSDSettings->GetSettingsObject();

            std::vector<wxString> keywords;

            for (const auto& settingEntry : settingsObject.mSettingEntries)
            {
                keywords.push_back(settingEntry.second.mKeyword);
            }

            SearchKeywordsInFiles(files, keywords, project->GetName());

            if (!mFileItems.empty())
            {
                for (std::vector<havGSDFileItem>::size_type index = 0; index < mFileItems.size(); ++index)
                {
                    havGSDFileItem fileItem = mFileItems[index];

                    items.clear();

                    items.push_back(fileItem.mType);
                    items.push_back(fileItem.mDescription);
                    items.push_back(fileItem.mProjectName);
                    items.push_back((mShowAbsoluteFilePath == true) ? fileItem.mFilePath : fileItem.mFileName);
                    items.push_back(fileItem.mLine);

                    mThemedListCtrlForTasks->AppendItem(items);

                    wxColour columnColor(128, 0, 128);

                    auto foundSettingEntry = settingsObject.mSettingEntries.find(fileItem.mType);

                    if (foundSettingEntry != settingsObject.mSettingEntries.end())
                    {
                        columnColor = foundSettingEntry->second.mColor;
                    }

                    if (columnColor != wxColour(128, 0, 128))
                    {
                        mThemedListCtrlForTasks->SetItemTextColour(mThemedListCtrlForTasks->RowToItem(index), columnColor, 0);
                    }
                }
            }
        }
    }
}
