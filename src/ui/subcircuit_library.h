#pragma once
// B: ordinary Schematic JSON fragments shown in the component palette.
#include <wx/string.h>
#include <vector>
#include "contract/data_model.h"

class SubcircuitLibrary {
public:
    struct Item { wxString name; editor::Schematic schematic; };
    explicit SubcircuitLibrary(const wxString& directory);
    size_t Reload(); // Number of unreadable/incompatible files skipped.
    bool Save(wxString name, const editor::Schematic& fragment, bool overwrite, wxString* error);
    const std::vector<Item>& Items() const { return m_items; }
    const wxString& Directory() const { return m_directory; }
    static bool Validate(const editor::Schematic& fragment, wxString* error);

private:
    wxString m_directory;
    std::vector<Item> m_items;
};
