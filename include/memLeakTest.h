/*
* wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#include <wx/wx.h>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>

class memLeakTestFrame : public wxFrame {
public:
    memLeakTestFrame(const wxString& title);
    ~memLeakTestFrame();

private:
    std::thread             m_stressThread;
    std::atomic<bool>       m_running{false};
    wxStaticText*           m_statusLabel   = nullptr;
    std::vector<char*>      m_chunks;
    std::mutex              m_chunksMutex;

    void StartStressThread(size_t chunkSizeMB, int intervalMs);
    void stressTestMemoryLeak(size_t chunkSizeMB, int intervalMs);
    void freeMemory();           //frees all allocated chunks
    void OnExit(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    wxDECLARE_EVENT_TABLE();
};