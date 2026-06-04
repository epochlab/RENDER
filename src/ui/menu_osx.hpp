#pragma once

struct OsxMenuFlags {
    bool doCaptureEXR = false;
    bool doCapturePNG = false;
    bool doSaveJson = false;
    bool showPanel  = true;
};

void initOsxMenuBar(OsxMenuFlags* flags);
void syncOsxMenuBar(const OsxMenuFlags& flags);
