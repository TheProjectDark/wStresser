/*
* wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "MainFrame.h"
#include "Functions/memLeakTest.h"

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
    wxMenu* menuFile = new wxMenu;
    wxMenu* menuHelp = new wxMenu;
    menuFile->Append(wxID_EXIT);
    menuHelp->Append(wxID_ABOUT);
    menuBar->Append (menuFile, "&File");
    menuBar->Append (menuHelp, "&Help");
    SetMenuBar(menuBar);

    wxButton* memLeakTest = new wxButton(panel, wxID_ANY, "Memory leak test");

    //sizers
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    //button sizers
    mainSizer->Add(memLeakTest, 0, wxALL, 10);

    panel->SetSizer(mainSizer);

    memLeakTest->Bind(wxEVT_BUTTON, &MainFrame::OnMemLeak, this);
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

void MainFrame::OnMemLeak(wxCommandEvent& event) {
    memLeakTestFrame* memLeakFrame = new memLeakTestFrame("Memory leak test");
    memLeakFrame->Show();
}

void MainFrame::OnAbout(wxCommandEvent& event) {
    wxMessageBox("wStresser is free and open source C++ PC stresser",
                "wStresser alpha v1.0", wxOK | wxICON_INFORMATION);
}

void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}
