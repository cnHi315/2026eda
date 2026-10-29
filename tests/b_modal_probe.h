// Test-only hook injected while building a copy of tools/ui_verify.cpp.
// Acknowledge EOF prompts without relying on desktop input injection.
#include <wx/modalhook.h>
#include <wx/msgdlg.h>
#include <cstdio>
class BModalProbe : public wxModalDialogHook {
public:
    BModalProbe() { Register(); }
    int Enter(wxDialog* dialog) override {
        if (dynamic_cast<wxMessageDialog*>(dialog)) {
            std::printf("B_MODAL_PROMPT: %d\n", ++prompts);
            std::fflush(stdout);
            return wxID_OK;
        }
        return wxID_NONE;
    }
private:
    int prompts = 0;
};
inline BModalProbe bModalProbe;
