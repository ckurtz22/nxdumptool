#pragma once

#ifndef __UI_H__
#define __UI_H__

#define FB_WIDTH                1280
#define FB_HEIGHT               720

#define CHAR_PT_SIZE            12
#define SCREEN_DPI_CNT          96

#define BG_COLOR_RGB            50

#define HIGHLIGHT_BG_COLOR_R    33
#define HIGHLIGHT_BG_COLOR_G    34
#define HIGHLIGHT_BG_COLOR_B    39

#define HIGHLIGHT_FONT_COLOR_R  0
#define HIGHLIGHT_FONT_COLOR_G  255
#define HIGHLIGHT_FONT_COLOR_B  197

// UTF-8
#define UPWARDS_ARROW           "\xE2\x86\x91"
#define DOWNWARDS_ARROW         "\xE2\x86\x93"

#define COMMON_MAX_ELEMENTS     8
#define HFS0_MAX_ELEMENTS       14
#define ROMFS_MAX_ELEMENTS      12
#define SDCARD_MAX_ELEMENTS     3
#define ORPHAN_MAX_ELEMENTS     12

#define OPTIONS_X_START_POS     (35 * CHAR_PT_SIZE)
#define OPTIONS_X_END_POS       (OPTIONS_X_START_POS + (6 * CHAR_PT_SIZE))
#define OPTIONS_X_END_POS_NSP   (FB_WIDTH - (4 * CHAR_PT_SIZE))

#define TAB_WIDTH               4

#define BROWSER_ICON_DIMENSION  16

// UTF-16
#define NINTENDO_FONT_A         "\xE0\xA0"
#define NINTENDO_FONT_B         "\xE0\xA1"
#define NINTENDO_FONT_Y         "\xE0\xA3"
#define NINTENDO_FONT_L         "\xE0\xA4"
#define NINTENDO_FONT_R         "\xE0\xA5"
#define NINTENDO_FONT_ZL        "\xE0\xA6"
#define NINTENDO_FONT_ZR        "\xE0\xA7"
#define NINTENDO_FONT_DPAD      "\xE0\xAA"
#define NINTENDO_FONT_PLUS      "\xE0\xB5"
#define NINTENDO_FONT_HOME      "\xE0\xB9"
#define NINTENDO_FONT_LSTICK    "\xE0\xC1"
#define NINTENDO_FONT_RSTICK    "\xE0\xC2"

typedef enum {
    resultNone,
    resultShowMainMenu,
    resultShowGameCardMenu,
    resultShowXciDumpMenu,
    resultDumpXci,
    resultShowNspDumpMenu,
    resultShowNspAppDumpMenu,
    resultShowNspPatchDumpMenu,
    resultShowNspAddOnDumpMenu,
    resultDumpNsp,
    resultShowHfs0Menu,
    resultShowRawHfs0PartitionDumpMenu,
    resultDumpRawHfs0Partition,
    resultShowHfs0PartitionDataDumpMenu,
    resultDumpHfs0PartitionData,
    resultShowHfs0BrowserMenu,
    resultHfs0BrowserGetList,
    resultShowHfs0Browser,
    resultHfs0BrowserCopyFile,
    resultShowExeFsMenu,
    resultShowExeFsSectionDataDumpMenu,
    resultDumpExeFsSectionData,
    resultShowExeFsSectionBrowserMenu,
    resultExeFsSectionBrowserGetList,
    resultShowExeFsSectionBrowser,
    resultExeFsSectionBrowserCopyFile,
    resultShowRomFsMenu,
    resultShowRomFsSectionDataDumpMenu,
    resultDumpRomFsSectionData,
    resultShowRomFsSectionBrowserMenu,
    resultRomFsSectionBrowserGetEntries,
    resultShowRomFsSectionBrowser,
    resultRomFsSectionBrowserChangeDir,
    resultRomFsSectionBrowserCopyFile,
    resultDumpGameCardCertificate,
    resultShowSdCardEmmcMenu,
    resultShowSdCardEmmcTitleMenu,
    resultShowSdCardEmmcOrphanPatchAddOnMenu,
    resultShowUpdateMenu,
    resultUpdateNSWDBXml,
    resultUpdateApplication,
    resultExit
} UIResult;

typedef enum {
    stateMainMenu,
    stateGameCardMenu,
    stateXciDumpMenu,
    stateDumpXci,
    stateNspDumpMenu,
    stateNspAppDumpMenu,
    stateNspPatchDumpMenu,
    stateNspAddOnDumpMenu,
    stateDumpNsp,
    stateHfs0Menu,
    stateRawHfs0PartitionDumpMenu,
    stateDumpRawHfs0Partition,
    stateHfs0PartitionDataDumpMenu,
    stateDumpHfs0PartitionData,
    stateHfs0BrowserMenu,
    stateHfs0BrowserGetList,
    stateHfs0Browser,
    stateHfs0BrowserCopyFile,
    stateExeFsMenu,
    stateExeFsSectionDataDumpMenu,
    stateDumpExeFsSectionData,
    stateExeFsSectionBrowserMenu,
    stateExeFsSectionBrowserGetList,
    stateExeFsSectionBrowser,
    stateExeFsSectionBrowserCopyFile,
    stateRomFsMenu,
    stateRomFsSectionDataDumpMenu,
    stateDumpRomFsSectionData,
    stateRomFsSectionBrowserMenu,
    stateRomFsSectionBrowserGetEntries,
    stateRomFsSectionBrowser,
    stateRomFsSectionBrowserChangeDir,
    stateRomFsSectionBrowserCopyFile,
    stateDumpGameCardCertificate,
    stateSdCardEmmcMenu,
    stateSdCardEmmcTitleMenu,
    stateSdCardEmmcOrphanPatchAddOnMenu,
    stateUpdateMenu,
    stateUpdateNSWDBXml,
    stateUpdateApplication
} UIState;

typedef enum {
    MENUTYPE_MAIN = 0,
    MENUTYPE_GAMECARD,
    MENUTYPE_SDCARD_EMMC
} curMenuType;

void uiFill(int x, int y, int width, int height, u8 r, u8 g, u8 b);

void uiDrawIcon(const u8 *icon, int width, int height, int x, int y);

bool uiLoadJpgFromMem(u8 *rawJpg, size_t rawJpgSize, int expectedWidth, int expectedHeight, int desiredWidth, int desiredHeight, u8 **outBuf);

bool uiLoadJpgFromFile(const char *filename, int expectedWidth, int expectedHeight, int desiredWidth, int desiredHeight, u8 **outBuf);

void uiDrawString(const char *string, int x, int y, u8 r, u8 g, u8 b);

u32 uiGetStrWidth(const char *string);

void uiRefreshDisplay();

void uiStatusMsg(const char *fmt, ...);

void uiUpdateStatusMsg();

void uiPleaseWait(u8 wait);

void uiUpdateFreeSpace();

void uiClearScreen();

void uiPrintHeadline();

void uiDeinit();

int uiInit();

void uiSetState(UIState state);

UIState uiGetState();

UIResult uiProcess();

#endif
