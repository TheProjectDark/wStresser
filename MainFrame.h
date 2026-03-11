//
// Created by Vladlen Shapoval on 11.03.26.
//

#include <wx-3.3/wx/wx.h>

class MainFrame : public wxFrame {
    public:
        MainFrame(const wxString& title);

    private:
        void OnExit(wxCommandEvent& event);
        void OnAbout(wxCommandEvent& event);
};