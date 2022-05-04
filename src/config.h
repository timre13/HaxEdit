#pragma once

#define DEBUG_ENABLE_FUNC_TIMER         0

//-------------------- Window size --------------------

#define DEF_WIN_W                       1500
#define DEF_WIN_H                       1000

//-------------------- Fonts --------------------

#define FONT_FAMILY_REGULAR             "DejaVu Sans Mono"
#define FONT_FAMILY_BOLD                "DejaVu Sans Mono"
#define FONT_FAMILY_ITALIC              "DejaVu Sans Mono"
#define FONT_FAMILY_BOLDITALIC          "DejaVu Sans Mono"
#define DEF_FONT_SIZE_PX                18

//-------------------- Default theme colors --------------------

#define DEF_BG                          RGBColor{0.150f, 0.150f, 0.150f}
#define DEF_LINEN_FG                    RGBColor{0.560f, 0.710f, 0.070f}
#define DEF_CURSOR_LINE_COLOR           RGBAColor{0.300f, 0.300f, 0.300f, 0.5f}
#define DEF_SEL_BG                      RGBColor{0.156f, 0.541f, 0.862f}
#define DEF_SEL_FG                      RGBColor{1.000f, 1.000f, 1.000f}
#define DEF_FIND_MARK_COLOR             RGBColor{0.650f, 0.650f, 0.180f}

//-------------------- Line numbers --------------------

#define BUFFER_DRAW_LINE_NUMS           true
#define LINEN_BAR_WIDTH_CHAR            4
#define LINEN_CURR_LINE_FG              RGBColor{0.392f, 0.824f, 0.784f}
#define LINEN_DRAW_RELATIVE             true
#define LINEN_ALIGN_NONCURR_RIGHT       false

//-------------------- Tabs --------------------

#define TABLINE_HEIGHT_PX               20
#define TABLINE_BG_COLOR                RGBColor{0.098f, 0.118f, 0.196f}
#define TABLINE_TAB_WIDTH_PX            200
#define TABLINE_TAB_COLOR               RGBColor{0.157f, 0.157f, 0.353f}
#define TABLINE_ACTIVE_TAB_COLOR        RGBColor{0.275f, 0.275f, 0.510f}
#define TABLINE_TAB_MAX_TEXT_LEN        18

//-------------------- Paths --------------------

#define ICON_FILE_PATH                  "../img/logo.png"
//#define DICTIONARY_FILE_PATH            "/usr/share/dict/words"
#define DICTIONARY_FILE_PATH            "../external/dictionary.txt"
#define THEME_PATH                      "../external/themes/nightlion_aptan.epf"

//-------------------- Welcome screen --------------------

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
#define WELCOME_MSG                     "\033[1mWelcome to \033[92mHaxorEdit\033[0m\n\n" \
                                        "Built on %BUILD_DATE% (Build t.: %BUILD_TYPE%, Optim.: %BUILD_IS_OPTIMIZED%, " \
                                        "ASAN: %BUILD_IS_ASAN_ON%)\nCompiler: \033[36m%COMPILER_NAME%\033[0m\n" \
                                        "OpenGL Vendor: \033[36m%GL_VENDOR%\033[0m | Renderer: \033[36m%GL_RENDERER%\033[0m\n" \
                                        "Font family: R: %FONT_FAMILY_REGULAR%, B: \033[1m%FONT_FAMILY_BOLD%\033[0m, " \
                                        "I: \033[3m%FONT_FAMILY_ITALIC%\033[0m, \033[1m\033[3mBI: %FONT_FAMILY_BOLDITALIC%\033[0m\n\n" \
                                        "\033[90m\033[3m%FORTUNE%\033[0m"
#define WELCOME_MSG_DEF_FG              RGBColor{0.800f, 0.800f, 0.800f}
#define FORTUNE_CMD                     "fortune -s computers linuxcookie linux"

//-------------------- Buffer settings --------------------

// Number of spaces to insert when pressing tab, set to 0 to insert the tab character
#define TAB_SPACE_COUNT                 4
#define ENABLE_SYNTAX_HIGHLIGHTING      true
#define DRAW_INDENT_RAINBOW             true
#define SCROLL_SPEED_MULTIPLIER         40
#define BUFFER_WRAP_LINES               true
// The line break character is displayed as this character. Set to 0 to disable rendering of line break.
#define BUF_NL_DISP_CHAR_CODE           0xb6
// Milliseconds to wait before toggling cursor visibility
// Set to -1 to disable blinking
#define CURSOR_BLINK_MS                 500
#define AUTO_RELOAD_CHECK_FREQ_MS       2000

#define AUTO_RELOAD_MODE_DONT 0
#define AUTO_RELOAD_MODE_AUTO 1
#define AUTO_RELOAD_MODE_ASK  2
#define AUTO_RELOAD_MODE                AUTO_RELOAD_MODE_ASK

#define IMG_BUF_ZOOM_STEP               0.05f
#define HIDE_MOUSE_WHILE_TYPING         true

//-------------------- Dialogs --------------------

#define FILE_DIALOG_ICON_SIZE_PX        32
#define DIALOG_FLASH_TIME_MS            1000
#define DIALOG_FLASH_FREQ_MS            200

//-------------------- Misc. --------------------

#define DATE_TIME_FORMAT                "%F %T"
#define DATE_TIME_STR_LEN               19

#define MAX_FILE_SIZE                   1024*1024*1024

// Max. number of paths to store in the last files list
#define RECENT_LIST_MAX_SIZE            20
