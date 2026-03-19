/*
 * wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "memLeakTest.h"
#include <wx/wx.h>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>

wxBEGIN_EVENT_TABLE(memLeakTestFrame, wxFrame)
    EVT_BUTTON(wxID_ANY, memLeakTestFrame::OnExit)
    EVT_CLOSE(memLeakTestFrame::OnClose)
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
}

void memLeakTestFrame::StartStressThread(size_t chunkSizeMB, int intervalMs) {
    m_running = true;
    m_stressThread = std::thread([this, chunkSizeMB, intervalMs]() {
        stressTestMemoryLeak(chunkSizeMB, intervalMs);
    });
}

void memLeakTestFrame::stressTestMemoryLeak(size_t chunkSizeMB, int intervalMs) {
    const size_t chunkSizeBytes = chunkSizeMB * 1024 * 1024;
    size_t totalLeaked = 0;

    while (m_running) {
        char* chunk = new char[chunkSizeBytes];
        std::fill(chunk, chunk + chunkSizeBytes, 0xAB);

        {
            std::lock_guard<std::mutex> lock(m_chunksMutex);
            m_chunks.push_back(chunk);
        }

        totalLeaked += chunkSizeMB;

        wxString msg = wxString::Format("Leaked: %zu MB total", totalLeaked);
        wxTheApp->CallAfter([this, msg]() {
            //null check prevents dangling pointer after frame closes
            if (m_statusLabel)
                m_statusLabel->SetLabel(msg);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

void memLeakTestFrame::freeMemory() {
    std::lock_guard<std::mutex> lock(m_chunksMutex);
    for (char* ptr : m_chunks)
        delete[] ptr;
    m_chunks.clear();
}

void memLeakTestFrame::OnClose(wxCloseEvent& event) {
    m_running = false;

    //must join before freeing — thread may still be writing to m_chunks
    if (m_stressThread.joinable())
        m_stressThread.join();

    freeMemory();

    m_statusLabel = nullptr;
    event.Skip();
}

void memLeakTestFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}