#pragma once

#define DEBUG_ENABLE_FUNC_TIMER 0


#define DEF_WIN_W 1500
#define DEF_WIN_H 1000

#define FONT_FAMILY_REGULAR     "FreeMono"
#define FONT_FAMILY_BOLD        "FreeMono"
#define FONT_FAMILY_ITALIC      "FreeMono"
#define FONT_FAMILY_BOLDITALIC  "FreeMono"
#define DEF_FONT_SIZE_PX 18

#define DEF_BG RGBColor{0.15f, 0.15f, 0.15f}

// Line number bar width in characters
#define LINEN_BAR_WIDTH 4
#define LINEN_FONT_COLOR RGBColor{0.56f, 0.71f, 0.07f}
#define LINEN_BG_COLOR RGBColor{0.2f, 0.2f, 0.2f}
#define LINEN_ACTIVE_FONT_COLOR RGBColor{100/255.f, 210/255.f, 200/255.f}
#define LINEN_DRAW_RELATIVE true
#define LINEN_ALIGN_NONCURR_RIGHT false

#define BUFFER_WRAP_LINES true
#define BUFFER_DRAW_LINE_NUMS true

#define CURSOR_LINE_COLOR RGBAColor{0.3f, 0.3f, 0.3f, 0.5f}
// The number of frames to wait before blinking one
// Set to -1 to disable blinking
#define CURSOR_BLINK_FRAMES 30

#define SCROLL_SPEED_MULTIPLIER 40

#define STATUSBAR_BG_COLOR RGBColor{70/255.0f, 70/255.0f, 90/255.0f}

// Number of spaces to insert when pressing tab, set to 0 to insert the tab character
#define TAB_SPACE_COUNT 4

#define DRAW_INDENT_RAINBOW true

#define TABLINE_HEIGHT_PX 20
#define TABLINE_BG_COLOR RGBColor{25/255.0f, 30/255.0f, 50/255.0f}
#define TABLINE_TAB_WIDTH_PX 200
#define TABLINE_TAB_COLOR RGBColor{40/255.0f, 40/255.0f, 90/255.0f}
#define TABLINE_ACTIVE_TAB_COLOR RGBColor{70/255.0f, 70/255.0f, 130/255.0f}
#define TABLINE_TAB_MAX_TEXT_LEN 18

#define FILE_DIALOG_ICON_SIZE_PX 32

#define ENABLE_SYNTAX_HIGHLIGHTING true

#define ICON_FILE_PATH "../img/logo.png"

/*
 * Valid keys:
 * - BUILD_DATE
 * - BUILD_TYPE
 * - BUILD_IS_OPTIMIZED
 * - BUILD_IS_ASAN_ON
 * - COMPILER_NAME
 * - GL_VENDOR
 * - GL_RENDERER
 * - FONT_FAMILY_REGULAR
 * - FONT_FAMILY_BOLD
 * - FONT_FAMILY_ITALIC
 * - FONT_FAMILY_BOLDITALIC
 */
#define WELCOME_MSG "\033[1m\033[97mWelcome to \033[92mHaxorEdit\033[0m\n\n" \
                    "Built on %BUILD_DATE% (Build t.: %BUILD_TYPE%, Optim.: %BUILD_IS_OPTIMIZED%, " \
                    "ASAN: %BUILD_IS_ASAN_ON%)\nCompiler: \033[36m%COMPILER_NAME%\033[0m\n" \
                    "OpenGL Vendor: \033[36m%GL_VENDOR%\033[0m | Renderer: \033[36m%GL_RENDERER%\033[0m\n" \
                    "Font family: R: %FONT_FAMILY_REGULAR%, B: \033[1m%FONT_FAMILY_BOLD%\033[0m, " \
                    "I: \033[3m%FONT_FAMILY_ITALIC%\033[0m, \033[1m\033[3mBI: %FONT_FAMILY_BOLDITALIC%\033[0m\n\n" \
                    "\033[90m\033[3m%FORTUNE%\033[0m"
#define WELCOME_MSG_DEF_FG RGBColor{0.8f, 0.8f, 0.8f}
#define FORTUNE_CMD "fortune -s computers linuxcookie linux"

#define IMG_BUF_ZOOM_STEP 0.05f

//#define DICTIONARY_FILE_PATH "/usr/share/dict/words"
#define DICTIONARY_FILE_PATH "../dictionary.txt"

#define MAX_FILE_SIZE 1024*1024*1024

#define DATE_TIME_FORMAT "%F %T"
#define DATE_TIME_STR_LEN 19

//#define THEME_PATH "../external/onedark_theme.epf"
//#define THEME_PATH "../external/vibrant_ink_theme.epf"
#define THEME_PATH "../external/solarized_light_theme.epf"
