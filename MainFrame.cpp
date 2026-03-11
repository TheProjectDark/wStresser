#include "MainFrame.h"

class App : public wxApp
{
public:
    bool OnInit();
};

MainFrame::MainFrame(const wxString& title)
        : wxFrame(nullptr, wxID_ANY, title)
{
    wxPanel* panel = new wxPanel(this);

    wxMenuBar* menuBar = new wxMenuBar;
    wxMenu *menuFile = new wxMenu;
    wxMenu *menuHelp = new wxMenu;
    menuFile->Append(wxID_EXIT);
    menuHelp->Append(wxID_ABOUT);
    menuBar->Append (menuFile, "&File");
    menuBar->Append (menuHelp, "&Help");
    SetMenuBar(menuBar);


    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
}

wxIMPLEMENT_APP(App);

//show main frame
bool App::OnInit() {
    SetExitOnFrameDelete(true);

    MainFrame* mainFrame = new MainFrame("wStresser");
    mainFrame->SetClientSize(mainFrame->FromDIP(wxSize(300, 400)));
    mainFrame->Show();
    return true;
}

void MainFrame::OnAbout(wxCommandEvent& event) {
    wxMessageBox("wStresser is free and open source C++ PC stresser",
                "wStresser alpha v1.0", wxOK | wxICON_INFORMATION);
}

void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}
