#pragma once

#define DEBUG_ENABLE_FUNC_TIMER 0


#define DEF_WIN_W 1500
#define DEF_WIN_H 1000

#define FONT_FAMILY_REGULAR     "FreeMono"
#define FONT_FAMILY_BOLD        "FreeMono"
#define FONT_FAMILY_ITALIC      "FreeMono"
#define FONT_FAMILY_BOLDITALIC  "FreeMono"
#define FONT_SIZE_PX 18

#define BG_COLOR RGBColor{0.1f, 0.1f, 0.1f}

// Line number bar width in characters
#define LINEN_BAR_WIDTH 4
#define LINEN_FONT_COLOR RGBColor{0.56f, 0.71f, 0.07f}
#define LINEN_BG_COLOR RGBColor{0.2f, 0.2f, 0.2f}
#define LINEN_ACTIVE_FONT_COLOR RGBColor{100/255.f, 210/255.f, 200/255.f}

#define BUFFER_WRAP_LINES true
#define BUFFER_DRAW_LINE_NUMS true

#define CURSOR_COLOR RGBColor{1.0f, 0.0f, 0.0f}
#define CURSOR_DRAW_BLOCK 0
// The number of frames to wait before blinking one
// Set to -1 to disable blinking
#define CURSOR_BLINK_FRAMES 30

#define SCROLL_SPEED_MULTIPLIER 40

#define STATUSBAR_BG_COLOR RGBColor{70/255.0f, 70/255.0f, 90/255.0f}

// Number of spaces to insert when pressing tab, set to 0 to insert the tab character
#define TAB_SPACE_COUNT 4
