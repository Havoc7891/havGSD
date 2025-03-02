/*
havGSD.hpp

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

#ifndef HAVGSD_HPP
#define HAVGSD_HPP

#include "clWorkspaceEvent.hpp"
#include "clFileSystemEvent.h"
#include "cl_command_event.h"
#include "clTabTogglerHelper.h"
#include "clThemedListCtrl.h"
#include "plugin.h"

#include <wx/filename.h>
#include <wx/panel.h>
#include <wx/string.h>

#include <memory>
#include <vector>

#include "havGSDSettings.hpp"

#ifdef WXC_FROM_DIP
#undef WXC_FROM_DIP
#endif
#if wxVERSION_NUMBER >= 3100
#define WXC_FROM_DIP(x) wxWindow::FromDIP(x, NULL)
#else
#define WXC_FROM_DIP(x) x
#endif

struct havGSDFileItem
{
    wxString mType;
    wxString mProjectName;
    wxString mFileName;
    wxString mFilePath;
    wxString mDescription;
    wxString mLine;
};

class havGSD : public IPlugin
{
public:
    explicit havGSD(IManager* manager);
    virtual ~havGSD();

    virtual void CreateToolBar(clToolBarGeneric* toolbar);
    virtual void CreatePluginMenu(wxMenu* pluginsMenu);
    virtual void UnPlug();

protected:
    void OnWorkspaceOpened(clWorkspaceEvent& event);
    void OnWorkspaceClosed(clWorkspaceEvent& event);
    void OnActiveProjectChanged(clProjectSettingsEvent& event);
    void OnProjectRenamed(clCommandEvent& event);
    void OnProjectRemoved(clCommandEvent& event);
    void OnFileSaved(clCommandEvent& event);
    void OnFileRenamed(clFileSystemEvent& event);
    void OnFileDeleted(clFileSystemEvent& event);
    void OnItemActived(wxDataViewEvent& event);

private:
    std::unique_ptr<havGSDSettings> mHavGSDSettings;

    wxString GetHavGSDSettingsFile();

    wxBorder get_border_simple_theme_aware_bit()
    {
#if wxVERSION_NUMBER >= 3300 && defined(__WXMSW__)
        return wxSystemSettings::GetAppearance().IsDark() ? wxBORDER_SIMPLE : wxBORDER_DEFAULT;
#else
        return wxBORDER_DEFAULT;
#endif
    }

    void SearchKeywordsInFiles(const std::vector<wxFileName>& files, const std::vector<wxString>& keywords, const wxString& projectName);

    void RefreshKeywordList();

    std::vector<havGSDFileItem> mFileItems;
    clTabTogglerHelper::Ptr_t mTabToggler;
    clThemedListCtrl* mThemedListCtrlForTasks;
    wxPanel* mHavGSDPanel;
    wxString mWorkspaceType;

    bool mShowAbsoluteFilePath;
};

#endif
