/*
* wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#include <wx-3.3/wx/wx.h>

class MainFrame;

class memLeakTestFrame : public wxFrame {
 public:
 memLeakTestFrame(const wxString& title);

private:
 void onExit(wxCommandEvent &event);

};