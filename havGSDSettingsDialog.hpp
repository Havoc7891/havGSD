/*
havGSDSettingsDialog.hpp

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

#ifndef HAVGSDSETTINGSDIALOG_HPP
#define HAVGSDSETTINGSDIALOG_HPP

#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/listctrl.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <unordered_map>

#include "clThemedListCtrl.h"

#ifdef WXC_FROM_DIP
#undef WXC_FROM_DIP
#endif
#if wxVERSION_NUMBER >= 3100
#define WXC_FROM_DIP(x) wxWindow::FromDIP(x, NULL)
#else
#define WXC_FROM_DIP(x) x
#endif

class havGSDSettingsDialog : public wxDialog
{
public:
    havGSDSettingsDialog(wxWindow* parent, const std::unordered_map<wxString, wxColour>& defaultKeywordsWithColors, std::unordered_map<wxString, wxColour>& keywordsWithColors, bool& showAbsoluteFilePath)
        : wxDialog(parent, wxID_ANY, _("havGSD Settings"), wxDefaultPosition, wxSize(400, 400)),
          mDefaultKeywordsWithColors(defaultKeywordsWithColors), mKeywordsWithColors(keywordsWithColors), mShowAbsoluteFilePath(showAbsoluteFilePath)
    {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        // Keyword List with Colors
        mKeywordList = new clThemedListCtrl(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1, -1)),
                                            wxDV_COLUMN_WIDTH_NEVER_SHRINKS | wxDV_ROW_LINES | wxDV_SINGLE | get_border_simple_theme_aware_bit());

        mKeywordList->AddHeader(_("Keyword"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);
        mKeywordList->AddHeader(_("Color"), wxNullBitmap, wxCOL_WIDTH_AUTOSIZE);

        // Populate Keyword List
        for (const auto& [keyword, color] : mKeywordsWithColors)
        {
            AddKeywordToList(keyword, color);
        }

        mainSizer->Add(mKeywordList, 1, wxEXPAND | wxALL, WXC_FROM_DIP(5));

        // Add Keyword Input and Color Picker
        wxBoxSizer* keywordSizer = new wxBoxSizer(wxHORIZONTAL);
        mKeywordInput = new wxTextCtrl(this, wxID_ANY);
        mKeywordInput->SetHint(_("Enter keyword..."));
        keywordSizer->Add(mKeywordInput, 1, wxEXPAND | wxALL, WXC_FROM_DIP(5));

        mColorPicker = new wxColourPickerCtrl(this, wxID_ANY);
        keywordSizer->Add(mColorPicker, 0, wxALL, WXC_FROM_DIP(5));

        wxButton* addKeywordBtn = new wxButton(this, wxID_ANY, _("Add Keyword"));
        keywordSizer->Add(addKeywordBtn, 0, wxALL, WXC_FROM_DIP(5));
        mainSizer->Add(keywordSizer, 0, wxEXPAND);

        // Remove Keyword Button
        wxButton* removeKeywordBtn = new wxButton(this, wxID_ANY, _("Remove Selected Keyword"));
        mainSizer->Add(removeKeywordBtn, 0, wxEXPAND | wxALL, WXC_FROM_DIP(5));

        // Checkbox for Show Absolute File Path Display Option
        mAbsoluteFilePathCheckbox = new wxCheckBox(this, wxID_ANY, _("Show Absolute File Path"));
        mAbsoluteFilePathCheckbox->SetValue(mShowAbsoluteFilePath);
        mainSizer->Add(mAbsoluteFilePathCheckbox, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, WXC_FROM_DIP(5));

        // Restore Default Settings Button
        wxButton* restoreDefaultSettingsBtn = new wxButton(this, wxID_ANY, _("Restore Default Settings"));
        mainSizer->Add(restoreDefaultSettingsBtn, 0, wxEXPAND | wxALL, WXC_FROM_DIP(5));

        // Save / Cancel Buttons
        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* saveBtn = new wxButton(this, wxID_OK, _("Save"));
        wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, _("Cancel"));
        buttonSizer->Add(saveBtn, 1, wxEXPAND | wxALL, WXC_FROM_DIP(5));
        buttonSizer->Add(cancelBtn, 1, wxEXPAND | wxALL, WXC_FROM_DIP(5));
        mainSizer->Add(buttonSizer, 0, wxEXPAND);

        SetSizer(mainSizer);

        // Event Bindings
        addKeywordBtn->Bind(wxEVT_BUTTON, &havGSDSettingsDialog::OnAddKeyword, this);
        removeKeywordBtn->Bind(wxEVT_BUTTON, &havGSDSettingsDialog::OnRemoveKeyword, this);
        restoreDefaultSettingsBtn->Bind(wxEVT_BUTTON, &havGSDSettingsDialog::OnRestoreDefaultSettings, this);

        // Center Dialog relative to Parent Window
        Center();
    }

    void OnAddKeyword(wxCommandEvent&)
    {
        wxString newKeyword = mKeywordInput->GetValue();
        if (!newKeyword.IsEmpty() && mKeywordsWithColors.find(newKeyword) == mKeywordsWithColors.end())
        {
            wxColour selectedColor = mColorPicker->GetColour();
            mKeywordsWithColors[newKeyword] = selectedColor;
            AddKeywordToList(newKeyword, selectedColor);
            mKeywordInput->Clear();
        }
    }

    void OnRemoveKeyword(wxCommandEvent&)
    {
        wxDataViewItem item = mKeywordList->GetSelection();
        if (item.IsOk())
        {
            wxString keyword = mKeywordList->GetItemText(item);
            mKeywordsWithColors.erase(keyword);
            mKeywordList->DeleteItem(mKeywordList->ItemToRow(item));
        }
    }

    void OnRestoreDefaultSettings(wxCommandEvent&)
    {
        // Reset Keyword List
        mKeywordsWithColors = mDefaultKeywordsWithColors;

        mKeywordList->DeleteAllItems();

        // Populate Keyword List
        for (const auto& [keyword, color] : mKeywordsWithColors)
        {
            AddKeywordToList(keyword, color);
        }

        // Reset Show Absolute File Path Option
        mShowAbsoluteFilePath = false;
        mAbsoluteFilePathCheckbox->SetValue(mShowAbsoluteFilePath);
    }

    bool TransferDataFromWindow() override
    {
        mShowAbsoluteFilePath = mAbsoluteFilePathCheckbox->GetValue();
        return true;
    }

private:
    clThemedListCtrl* mKeywordList;
    wxTextCtrl* mKeywordInput;
    wxColourPickerCtrl* mColorPicker;
    wxCheckBox* mAbsoluteFilePathCheckbox;

    std::unordered_map<wxString, wxColour> mDefaultKeywordsWithColors;
    std::unordered_map<wxString, wxColour>& mKeywordsWithColors;
    bool& mShowAbsoluteFilePath;

    wxBorder get_border_simple_theme_aware_bit()
    {
#if wxVERSION_NUMBER >= 3300 && defined(__WXMSW__)
        return wxSystemSettings::GetAppearance().IsDark() ? wxBORDER_SIMPLE : wxBORDER_DEFAULT;
#else
        return wxBORDER_DEFAULT;
#endif
    }

    void AddKeywordToList(const wxString& keyword, const wxColour& color)
    {
        wxDataViewItem item = mKeywordList->AppendItem(keyword);
        if (item.IsOk())
        {
            mKeywordList->SetItemText(item, color.GetAsString(wxC2S_HTML_SYNTAX), 1); // Display HEX color
            mKeywordList->SetItemTextColour(item, color, 0);
            mKeywordList->SetItemTextColour(item, color, 1);
        }
    }
};

#endif
