#import <Cocoa/Cocoa.h>
#include "menu_osx.hpp"

@interface AppMenuDelegate : NSObject
@property (nonatomic, assign) OsxMenuFlags* flags;
- (void)actionCapture:(id)sender;
- (void)actionCapturePNG:(id)sender;
- (void)actionSaveJson:(id)sender;
- (void)actionPanelToggle:(id)sender;
@end

@implementation AppMenuDelegate
- (void)actionCapture:(id)sender     { _flags->doCaptureEXR = true; }
- (void)actionCapturePNG:(id)sender  { _flags->doCapturePNG = true; }
- (void)actionSaveJson:(id)sender    { _flags->doSaveJson = true; }
- (void)actionPanelToggle:(id)sender { _flags->showPanel  = !_flags->showPanel;  }
@end

static AppMenuDelegate* g_delegate  = nil;
static NSMenuItem*       g_panelItem = nil;

void initOsxMenuBar(OsxMenuFlags* flags) {
    g_delegate       = [[AppMenuDelegate alloc] init];
    g_delegate.flags = flags;

    NSMenu* mainMenu = [[NSMenu alloc] init];

    // ── App menu ──────────────────────────────────────────────
    NSMenuItem* appItem = [[NSMenuItem alloc] init];
    NSMenu* appMenu = [[NSMenu alloc] init];
    [appMenu addItemWithTitle:@"Quit"
                      action:@selector(terminate:)
               keyEquivalent:@"q"];
    appItem.submenu = appMenu;
    [mainMenu addItem:appItem];

    // ── View menu ─────────────────────────────────────────────
    NSMenuItem* viewItem = [[NSMenuItem alloc] initWithTitle:@"View"
                                                      action:nil
                                               keyEquivalent:@""];
    NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];

    NSMenuItem* captureItem = [[NSMenuItem alloc] initWithTitle:@"Export OpenEXR"
                                                         action:@selector(actionCapture:)
                                                  keyEquivalent:@""];
    captureItem.target = g_delegate;
    [viewMenu addItem:captureItem];

    NSMenuItem* capturePngItem = [[NSMenuItem alloc] initWithTitle:@"Export PNG"
                                                            action:@selector(actionCapturePNG:)
                                                     keyEquivalent:@""];
    capturePngItem.target = g_delegate;
    [viewMenu addItem:capturePngItem];

    NSMenuItem* jsonItem = [[NSMenuItem alloc] initWithTitle:@"Set JSON"
                                                      action:@selector(actionSaveJson:)
                                               keyEquivalent:@""];
    jsonItem.target = g_delegate;
    [viewMenu addItem:jsonItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    g_panelItem = [[NSMenuItem alloc] initWithTitle:@"Show/Hide HUD"
                                             action:@selector(actionPanelToggle:)
                                      keyEquivalent:@""];
    g_panelItem.target = g_delegate;
    g_panelItem.state  = flags->showPanel ? NSControlStateValueOn : NSControlStateValueOff;
    [viewMenu addItem:g_panelItem];

    viewItem.submenu = viewMenu;
    [mainMenu addItem:viewItem];

    [NSApp setMainMenu:mainMenu];
}

void syncOsxMenuBar(const OsxMenuFlags& flags) {
    if (g_panelItem)
        g_panelItem.state = flags.showPanel ? NSControlStateValueOn : NSControlStateValueOff;
}
