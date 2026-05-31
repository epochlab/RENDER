#pragma once

struct OsxMenuFlags {
    bool doCapture   = false;
    bool doSaveJson  = false;
    bool skyVisible  = false;
    bool showPanel   = true;
};

void initOsxMenuBar(OsxMenuFlags* flags);
void syncOsxMenuBar(const OsxMenuFlags& flags);
