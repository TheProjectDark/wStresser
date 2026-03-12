/*
* wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "memLeakTest.h"

memLeakTestFrame::memLeakTestFrame(const wxString &title) {
 wxFrame::Create(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(200, 200));
}
