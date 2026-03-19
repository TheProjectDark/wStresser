/*
 * wEditor
 * Copyright (C) 2026 TheProjectDark
 * ...
 */

#include "memLeakTest.h"
#include <wx/wx.h>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>

wxBEGIN_EVENT_TABLE(memLeakTestFrame, wxFrame)
    EVT_BUTTON(wxID_ANY, memLeakTestFrame::OnExit)
wxEND_EVENT_TABLE()

memLeakTestFrame::memLeakTestFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(300, 150))
{
    wxPanel* mPanel = new wxPanel(this);

    wxButton* stopButton = new wxButton(mPanel, wxID_ANY, "Stop");
    m_statusLabel = new wxStaticText(mPanel, wxID_ANY, "Starting...");

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_statusLabel, 0, wxALL | wxEXPAND, 10);
    mainSizer->Add(stopButton,    0, wxALL | wxEXPAND, 10);

    mPanel->SetSizer(mainSizer);

    StartStressThread(100, 500);
}

memLeakTestFrame::~memLeakTestFrame() {
    m_running = false;
    if (m_stressThread.joinable())
        m_stressThread.join();
}

void memLeakTestFrame::StartStressThread(size_t chunkSizeMB, int intervalMs) {
    m_running = true;
    m_stressThread = std::thread([this, chunkSizeMB, intervalMs]() {
        stressTestMemoryLeak(chunkSizeMB, intervalMs);
    });
}

void memLeakTestFrame::stressTestMemoryLeak(size_t chunkSizeMB, int intervalMs) {
    std::vector<char*> leakedChunks;
    const size_t chunkSizeBytes = chunkSizeMB * 1024 * 1024;
    size_t totalLeaked = 0;

    while (m_running) {
        char* chunk = new char[chunkSizeBytes];
        std::fill(chunk, chunk + chunkSizeBytes, 0xAB);

        leakedChunks.push_back(chunk);
        totalLeaked += chunkSizeMB;

        wxString msg = wxString::Format("Leaked: %zu MB total", totalLeaked);
        wxTheApp->CallAfter([this, msg]() {
            if (m_statusLabel)
                m_statusLabel->SetLabel(msg);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }

    // Intentional: chunks in leakedChunks are never freed (that's the test)
}

void memLeakTestFrame::OnExit(wxCommandEvent& event) {
    m_running = false;
    Close(true);
}