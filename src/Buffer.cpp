#include "Buffer.h"
#include "config.h"
#include "Timer.h"
#include "types.h"
#include "syntax.h"
#include <fstream>
#include <sstream>

int Buffer::open(const std::string& filePath)
{
    TIMER_BEGIN_FUNC();

    glfwSetCursor(g_window, Cursors::busy);

    m_filePath = filePath;
    m_isReadOnly = false;
    Logger::dbg << "Opening file: " << filePath << Logger::End;

    try
    {
        std::fstream file;
        file.open(filePath, std::ios::in);
        if (file.fail())
        {
            throw std::runtime_error{"Open failed"};
        }
        std::stringstream ss;
        ss << file.rdbuf();

        m_content = ss.str();
        updateHighlighting();
        m_numOfLines = strCountLines(m_content);

        Logger::dbg << "Read "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) from file" << Logger::End;
        file.close();
    }
    catch (std::exception& e)
    {
        m_content.clear();
        updateHighlighting();
        m_numOfLines = 0;

        Logger::err << "Failed to open file: \"" << filePath << "\": " << e.what() << Logger::End;
        glfwSetCursor(g_window, nullptr);
        TIMER_END_FUNC();
        return 1;
    }

    // Try to open the file as writable. If fails, read-only file.
    {
        std::fstream file;
        file.open(filePath, std::ios_base::out | std::ios_base::app);
        if (!file.is_open())
        {
            Logger::warn << "File is read only" << Logger::End;
            m_isReadOnly = true;
        }
        file.close();
    }

    glfwSetCursor(g_window, nullptr);
    TIMER_END_FUNC();
    return 0;
}

int Buffer::saveToFile()
{
    TIMER_BEGIN_FUNC();

    Logger::dbg << "Writing buffer to file: " << m_filePath << Logger::End;

    try
    {
        std::fstream file;
        file.open(m_filePath, std::ios::out);
        if (file.fail())
        {
            throw std::runtime_error{"Open failed"};
        }
        file.write(m_content.c_str(), m_content.length());
        if (file.fail())
        {
            throw std::runtime_error{"Call to write() failed"};
        }
        file.close();
        Logger::dbg << "Wrote "
            << m_content.length() << " characters ("
            << m_numOfLines << " lines) to file" << Logger::End;
    }
    catch (std::exception& e)
    {
        Logger::err << "Failed to write to file: \"" << m_filePath << "\": " << e.what() << Logger::End;
        TIMER_END_FUNC();
        return 1;
    }

    m_isModified = false;
    m_isReadOnly = false;

    TIMER_END_FUNC();
    return 0;
}

int Buffer::saveAsToFile(const std::string& filePath)
{
    const std::string originalPath = filePath;
    m_filePath = filePath;
    Logger::dbg << "Saving as: " << filePath << Logger::End;

    if (saveToFile())
    {
        Logger::err << "Save as failed" << Logger::End;
        m_filePath = originalPath;
        return 1;
    }
    return 0;
}

static int getLineLenAt(const std::string& str, int lineI)
{
    int _lineI{};
    std::string line;
    std::stringstream ss; ss << str;
    while (std::getline(ss, line) && _lineI != lineI) { ++_lineI; }
    return line.length();
}

void Buffer::scrollViewportToCursor()
{
    // Scroll up when the cursor goes out of the viewport
    if (m_cursorLine-5 < -m_scrollY/g_fontSizePx)
    {
        scrollBy(-(m_scrollY+(m_cursorLine-5)*g_fontSizePx));
    }
    // Scroll down when the cursor goes out of the viewport
    // FIXME: Line wrapping breaks this, too
    else if (m_cursorLine+m_scrollY/g_fontSizePx+5 > g_textRenderer->getWindowHeight()/g_fontSizePx)
    {
        scrollBy(-(m_cursorLine*g_fontSizePx+m_scrollY-g_textRenderer->getWindowHeight()+g_fontSizePx*5));
    }
}

void Buffer::updateHighlighting()
{
    if (m_content.empty())
        return;

    Logger::dbg("Highighter");

    //Logger::log << "Updating syntax highlighting" << Logger::End;

    m_highlightBuffer = std::string(m_content.length(), SYNTAX_MARK_NONE);

#if ENABLE_SYNTAX_HIGHLIGHTING
    auto highlightWord{[&](const std::string& word, char colorMark, bool shouldBeWholeWord){
        assert(!word.empty());
        size_t start{};
        while (true)
        {
            size_t found = m_content.find(word, start);
            if (found == std::string::npos)
                break;
            if (!shouldBeWholeWord |
                ((found == 0 || !isalnum(m_content[found-1]))
                 && (found+word.size()-1 == m_content.size()-1 || !isalnum(m_content[found+word.size()]))))
                m_highlightBuffer.replace(found, word.length(), word.length(), colorMark);
            start = found+word.length();
        }
    }};

    auto highlightStrings{[&](){
        bool isInsideString = false;
        for (size_t i{}; i < m_content.size(); ++i)
        {
            if (m_highlightBuffer[i] == SYNTAX_MARK_NONE && m_content[i] == '"'
                    && (i == 0 || (m_content[i-1] != '\\' || (i >= 2 && m_content[i-2] == '\\'))))
                isInsideString = !isInsideString;
            if (isInsideString || (m_highlightBuffer[i] == SYNTAX_MARK_NONE && m_content[i] == '"'))
                m_highlightBuffer[i] = SYNTAX_MARK_STRING;
        }
    }};

    auto highlightNumbers{[&](){
        size_t i{};
        while (i < m_content.size())
        {
            // Go till a number
            while (i < m_content.size() && !isdigit(m_content[i]) && m_content[i] != '.')
                ++i;
            // Color the number
            while (i < m_content.size() && (isxdigit(m_content[i]) || m_content[i] == '.' || m_content[i] == 'x'))
                m_highlightBuffer[i++] = SYNTAX_MARK_NUMBER;
        }
    }};

    auto highlightPrefixed{[&](const std::string& prefix, char colorMark){
        size_t charI{};
        std::stringstream ss;
        ss << m_content;
        std::string line;
        while (std::getline(ss, line))
        {
            const size_t found = line.find(prefix);
            if (found != std::string::npos)
            {
                const size_t beginning = charI+found;
                const size_t size = line.size()-found;
                m_highlightBuffer.replace(beginning, size, size, colorMark);
            }
            charI += line.size()+1;
        }
    }};

    auto highlightPreprocessors{[&](){
        size_t charI{};
        std::stringstream ss;
        ss << m_content;
        std::string line;
        while (std::getline(ss, line))
        {
            const size_t prefixPos = line.find(Syntax::preprocessorPrefix);
            if (prefixPos != std::string::npos)
            {
                const size_t beginning = charI+prefixPos;
                if (m_highlightBuffer[beginning] == SYNTAX_MARK_NONE)
                {
                    const size_t preprocessorEnd = line.find_first_of(' ', prefixPos);
                    const size_t size = (preprocessorEnd != std::string::npos ? preprocessorEnd : line.size())-prefixPos;
                    m_highlightBuffer.replace(beginning, size, size, SYNTAX_MARK_PREPRO);
                }
            }
            charI += line.size()+1;
        }
    }};

    auto highlightBlockComments{[&](){
        bool isInsideComment = false;
        for (size_t i{}; i < m_content.size(); ++i)
        {
            if (m_highlightBuffer[i] != SYNTAX_MARK_STRING
                    && m_content.substr(i, Syntax::blockCommentBegin.size()) == Syntax::blockCommentBegin)
                isInsideComment = true;
            else if (m_highlightBuffer[i] != SYNTAX_MARK_STRING
                    && m_content.substr(i, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd)
                isInsideComment = false;
            if (isInsideComment
                    || (m_highlightBuffer[i] != SYNTAX_MARK_STRING
                    && (m_content.substr(i, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd
                    || (i > 0 && m_content.substr(i-1, Syntax::blockCommentEnd.size()) == Syntax::blockCommentEnd))))
                m_highlightBuffer[i] = SYNTAX_MARK_COMMENT;
        }
    }};

    auto highlightCharLiterals{[&](){
        bool isInsideCharLit = false;
        for (size_t i{}; i < m_content.size(); ++i)
        {
            if (m_highlightBuffer[i] == SYNTAX_MARK_NONE && m_content[i] == '\''
                    && (i == 0 || (m_content[i-1] != '\\')))
                isInsideCharLit = !isInsideCharLit;
            if (isInsideCharLit || (m_highlightBuffer[i] == SYNTAX_MARK_NONE && m_content[i] == '\''))
                m_highlightBuffer[i] = SYNTAX_MARK_CHARLIT;
        }
    }};

    auto highlightChar{[&](char c, char colorMark){
        for (size_t i{}; i < m_content.size(); ++i)
        {
            if (m_highlightBuffer[i] != SYNTAX_MARK_STRING && m_highlightBuffer[i] != SYNTAX_MARK_COMMENT
                    && m_content[i] == c)
                m_highlightBuffer[i] = colorMark;
        }
    }};

    auto highlightFilePaths{[&](){
        auto _isValidFilePath{[&](const std::string& path){ // -> bool
            if (path[0] == '/')
                // Absolute path
                return isValidFilePath(path);
            else
                // Relative path
                return isValidFilePath(getParentPath(m_filePath)/std::filesystem::path{path});
        }};

        for (size_t i{}; i < m_content.size();)
        {
            while (i < m_content.size() && (isspace(m_content[i]) || m_content[i] == '"'))
                ++i;

            std::string word;
            while (i < m_content.size() && !isspace(m_content[i]) && m_content[i] != '"')
                word += m_content[i++];

            if (_isValidFilePath(word))
            {
                m_highlightBuffer.replace(i-word.size(), word.size(), word.size(), SYNTAX_MARK_FILEPATH);
            }

            ++i;
        }
    }};


    timer.reset();
    for (const auto& word : Syntax::operatorList) highlightWord(word, SYNTAX_MARK_OPERATOR, false);
    Logger::dbg << "Highlighting of operators took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    for (const auto& word : Syntax::keywordList) highlightWord(word, SYNTAX_MARK_KEYWORD, true);
    Logger::dbg << "Highlighting of keywords took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    for (const auto& word : Syntax::typeList) highlightWord(word, SYNTAX_MARK_TYPE, true);
    Logger::dbg << "Highlighting of types took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightNumbers();
    Logger::dbg << "Highlighting of numbers took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightPrefixed(Syntax::lineCommentPrefix, SYNTAX_MARK_COMMENT);
    Logger::dbg << "Highlighting of line comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightBlockComments();
    Logger::dbg << "Highlighting of block comments took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightCharLiterals();
    Logger::dbg << "Highlighting of character literals took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightStrings();
    Logger::dbg << "Highlighting of strings took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightPreprocessors();
    Logger::dbg << "Highlighting preprocessor directives took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightChar(';', SYNTAX_MARK_SPECCHAR);
    highlightChar('{', SYNTAX_MARK_SPECCHAR);
    highlightChar('}', SYNTAX_MARK_SPECCHAR);
    highlightChar('(', SYNTAX_MARK_SPECCHAR);
    highlightChar(')', SYNTAX_MARK_SPECCHAR);
    Logger::dbg << "Highlighting special characters took " << timer.getElapsedTimeMs() << "ms" << Logger::End;

    timer.reset();
    highlightFilePaths();
    Logger::dbg << "Highlighting of paths took " << timer.getElapsedTimeMs() << "ms" << Logger::End;


    Logger::log << "Syntax highlighting updated" << Logger::End;

    Logger::log(Logger::End);
#endif
}

void Buffer::updateCursor()
{
    TIMER_BEGIN_FUNC();

    assert(m_cursorLine >= 0);
    assert(m_numOfLines == 0 || m_cursorLine < m_numOfLines);
    assert(m_cursorCol >= 0);

    auto getCursorLineLen{ // -> int
        [&]()
        {
            return getLineLenAt(m_content, m_cursorLine);
        }
    };

    switch (m_cursorMovCmd)
    {
    case CursorMovCmd::Left:
        if (m_cursorCol > 0)
        {
            --m_cursorCol;
            --m_cursorCharPos;
        }
        break;

    case CursorMovCmd::Right:
    {
        auto cursorLineLen = getCursorLineLen();
        if (cursorLineLen > 0 && m_cursorCol < cursorLineLen)
        {
            ++m_cursorCol;
            ++m_cursorCharPos;
        }
        break;
    }

    case CursorMovCmd::Up:
        if (m_cursorLine > 0)
        {
            const int prevCursorCol = m_cursorCol;

            --m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen(), m_cursorCol);

            m_cursorCharPos -= prevCursorCol + ((int)getCursorLineLen()+1-m_cursorCol);
        }
        break;

    case CursorMovCmd::Down:
        if (m_cursorLine < m_numOfLines-1)
        {
            const int prevCursorCol = m_cursorCol;
            const int prevCursorLineLen = getCursorLineLen();

            ++m_cursorLine;

            if (m_cursorCol != 0) // Column 0 always exists
                // If the new line is smaller than the current cursor column, step back to the end of the line
                m_cursorCol = std::min((int)getCursorLineLen(), m_cursorCol);

            m_cursorCharPos += (prevCursorLineLen+1-prevCursorCol) + m_cursorCol;
        }
        break;

    case CursorMovCmd::LineBeginning:
        m_cursorCharPos -= m_cursorCol;
        m_cursorCol = 0;
        break;

    case CursorMovCmd::LineEnd:
    {
        const int prevCursorCol = m_cursorCol;
        const int cursorLineLen = getCursorLineLen();
        m_cursorCharPos += cursorLineLen-prevCursorCol;
        m_cursorCol = cursorLineLen;
        break;
    }

    case CursorMovCmd::FirstChar:
        m_cursorLine = 0;
        m_cursorCol = 0;
        m_cursorCharPos = 0;
        break;

    case CursorMovCmd::LastChar:
        m_cursorLine = m_numOfLines == 0 ? 0 : m_numOfLines-1;
        m_cursorCol = getCursorLineLen();
        m_cursorCharPos = m_content.size()-1;
        break;

    case CursorMovCmd::None:
        break;
    }

    assert(m_cursorLine >= 0);
    assert(m_numOfLines == 0 || m_cursorLine < m_numOfLines);
    assert(m_cursorCol >= 0);

    // Scroll up when the cursor goes out of the viewport
    if (m_cursorMovCmd != CursorMovCmd::None)
    {
        scrollViewportToCursor();
        m_isCursorShown = true;
        m_cursorMovCmd = CursorMovCmd::None;
    }

    TIMER_END_FUNC();
}

void Buffer::updateRStatusLineStr()
{
    m_statusLineStr.str
        = std::to_string(m_cursorLine+1)
        +':'+std::to_string(m_cursorCol+1)
        +" | "+std::to_string(m_cursorCharPos);
    m_statusLineStr.maxLen = std::max(size_t(4+1+3+3+7), m_statusLineStr.str.size());
}

void Buffer::render()
{
    TIMER_BEGIN_FUNC();

    assert(m_size.x > 0);
    assert(m_size.y > 0);

    // Fill background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+m_size.x, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(BG_COLOR)
    );

    // Draw line number background
    g_uiRenderer->renderFilledRectangle(
            m_position,
            {m_position.x+g_fontSizePx*LINEN_BAR_WIDTH, m_position.y+m_size.y},
            RGB_COLOR_TO_RGBA(LINEN_BG_COLOR));

    const float initTextX = m_position.x+g_fontSizePx*LINEN_BAR_WIDTH;
    const float initTextY = m_position.y+m_scrollY;
    float textX = initTextX;
    float textY = initTextY;

    g_textRenderer->prepareForDrawing();
    g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});

    bool isLineBeginning = true;
    bool isLeadingSpace = true;
    int lineI{};
    int colI{};
    size_t charI{(size_t)-1};
    for (char c : m_content)
    {
        // Don't draw the part of the buffer that is below the viewport
        if (textY > m_position.y+m_size.y)
        {
            break;
        }
        const bool isCharInsideViewport = textY > -g_fontSizePx;

        ++charI;
        if (!isspace(c))
            isLeadingSpace = false;

        if (isCharInsideViewport && BUFFER_DRAW_LINE_NUMS && isLineBeginning)
        {
            g_textRenderer->setDrawingColor(
                    lineI == m_cursorLine ? LINEN_ACTIVE_FONT_COLOR : LINEN_FONT_COLOR);

            const std::string lineNumStr =
#if LINEN_DRAW_RELATIVE
                std::to_string(lineI == m_cursorLine ?
                        m_cursorLine+1 :
                        std::abs(lineI-m_cursorLine));
#else
                std::to_string(lineI+1);
#endif

            float digitX = m_position.x
#if LINEN_ALIGN_NONCURR_RIGHT
                + (lineI == m_cursorLine ? 0 : (LINEN_BAR_WIDTH-lineNumStr.length()*0.75f)*g_fontSizePx);
#else
            ;
#endif
            for (char digit : lineNumStr)
            {
                auto dimensions = g_textRenderer->renderChar(digit,
                        {digitX, textY},
                        lineI == m_cursorLine ? FontStyle::Bold : FontStyle::Italic);
                digitX += dimensions.advance/64.0f;
            }

            // Reset color
            g_textRenderer->setDrawingColor({1.0f, 1.0f, 1.0f});
        }

        // Render the cursor line highlight
        if (isCharInsideViewport && isLineBeginning && m_cursorLine == lineI)
        {
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2},
                    {textX+m_size.x,
                        initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx},
                    CURSOR_LINE_COLOR
            );
            // Bind the text renderer shader again
            g_textRenderer->prepareForDrawing();
        }

        // Render indent guides
        if (TAB_SPACE_COUNT > 0 && isCharInsideViewport
         && isLeadingSpace && colI%TAB_SPACE_COUNT==0 && colI != 0)
        {
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2},
                    {textX+g_fontSizePx/10, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5},
                    {0.7f, 0.7f, 0.7f, 0.7f}
            );
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*2},
                    {textX+g_fontSizePx/10, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*3},
                    {0.7f, 0.7f, 0.7f, 0.7f}
            );
            g_uiRenderer->renderFilledRectangle(
                    {textX, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*4},
                    {textX+g_fontSizePx/10, initTextY+textY-m_scrollY-m_position.y+2+g_fontSizePx/5*5},
                    {0.7f, 0.7f, 0.7f, 0.7f}
            );
            // Bind the text renderer shader again
            g_textRenderer->prepareForDrawing();
        }

        /*
         * Render a cursor at the current character position if it is the cursor position.
         */
        auto drawCursorIfNeeded{
            [&](int width)
            {
                if ((m_cursorMovCmd != CursorMovCmd::None || m_isCursorShown) && charI == m_cursorCharPos)
                {
#if CURSOR_DRAW_BLOCK
                    g_uiRenderer->renderRectangleOutline(
                            {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                            {1.0f, 0.0f, 0.0f},
                            2
                    );
                    g_uiRenderer->renderFilledRectangle(
                            {textX-2, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+width+2, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                            {1.0f, 0.0f, 0.0f, 0.4f}
                    );
#else
                    (void)width;
                    g_uiRenderer->renderFilledRectangle(
                            {textX-1, initTextY+textY-m_scrollY-m_position.y-2},
                            {textX+1, initTextY+textY-m_scrollY-m_position.y+g_fontSizePx+2},
                            {1.0f, 0.0f, 0.0f}
                    );
#endif

                    // Bind the text renderer shader again
                    g_textRenderer->prepareForDrawing();
                }
            }
        };


        switch (c)
        {
        case '\n': // New line
        case '\r': // Carriage return
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            textX = initTextX;
            textY += g_fontSizePx;
            isLineBeginning = true;
            isLeadingSpace = true;
            ++lineI;
            colI = 0;
            continue;

        case '\t': // Tab
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            textX += g_fontSizePx*4;
            ++colI;
            continue;

        case '\v': // Vertical tab
            drawCursorIfNeeded(g_fontSizePx*0.7f);
            textX = initTextX;
            textY += g_fontSizePx * 4;
            colI = 0;
            ++lineI;
            continue;
        }

        if (BUFFER_WRAP_LINES && textX+g_fontSizePx > m_position.x+m_size.x)
        {
            textX = initTextX;
            textY += g_fontSizePx;
        }

        uint advance{};
        if (isCharInsideViewport)
        {
            FontStyle charStyle = FontStyle::Regular;
            RGBColor charColor;
            switch (m_highlightBuffer[charI])
            {
            case SYNTAX_MARK_NONE:     charColor = SYNTAX_COLOR_NONE;     charStyle = SYNTAX_STYLE_NONE; break;
            case SYNTAX_MARK_KEYWORD:  charColor = SYNTAX_COLOR_KEYWORD;  charStyle = SYNTAX_STYLE_KEYWORD; break;
            case SYNTAX_MARK_TYPE:     charColor = SYNTAX_COLOR_TYPE;     charStyle = SYNTAX_STYLE_TYPE; break;
            case SYNTAX_MARK_OPERATOR: charColor = SYNTAX_COLOR_OPERATOR; charStyle = SYNTAX_STYLE_OPERATOR; break;
            case SYNTAX_MARK_NUMBER:   charColor = SYNTAX_COLOR_NUMBER;   charStyle = SYNTAX_STYLE_NUMBER; break;
            case SYNTAX_MARK_STRING:   charColor = SYNTAX_COLOR_STRING;   charStyle = SYNTAX_STYLE_STRING; break;
            case SYNTAX_MARK_COMMENT:  charColor = SYNTAX_COLOR_COMMENT;  charStyle = SYNTAX_STYLE_COMMENT; break;
            case SYNTAX_MARK_CHARLIT:  charColor = SYNTAX_COLOR_CHARLIT;  charStyle = SYNTAX_STYLE_CHARLIT; break;
            case SYNTAX_MARK_PREPRO:   charColor = SYNTAX_COLOR_PREPRO;   charStyle = SYNTAX_STYLE_PREPRO; break;
            case SYNTAX_MARK_SPECCHAR: charColor = SYNTAX_COLOR_SPECCHAR; charStyle = SYNTAX_STYLE_SPECCHAR; break;
            case SYNTAX_MARK_FILEPATH: charColor = SYNTAX_COLOR_FILEPATH; charStyle = SYNTAX_STYLE_FILEPATH; break;
            default:
                Logger::err << "BUG: Invalid highlighting buffer value: " << m_highlightBuffer[charI] << Logger::End;
                abort();
            }
            g_textRenderer->setDrawingColor(charColor);
            advance = g_textRenderer->renderChar(c, {textX, textY}, charStyle).advance;

            drawCursorIfNeeded(advance/64.f);
        }
        else
        {
            advance = g_textRenderer->getCharGlyphAdvance(c, FontStyle::Regular);
        }

        textX += (advance/64.0f);
        ++colI;
        isLineBeginning = false;
    }

    if (m_isDimmed)
    {
        g_uiRenderer->renderFilledRectangle(
                m_position,
                {m_position.x+m_size.x, m_position.y+m_size.y},
                {UNPACK_RGB_COLOR(BG_COLOR), 0.5f}
        );
    }

    TIMER_END_FUNC();
}

void Buffer::insert(char character)
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_content = m_content.insert(m_cursorCharPos, 1, character);

    if (character == '\n')
    {
        ++m_cursorLine;
        ++m_cursorCharPos;
        m_cursorCol = 0;
        ++m_numOfLines;
    }
    else
    {
        ++m_cursorCol;
        ++m_cursorCharPos;
    }
    m_isModified = true;

    m_isCursorShown = true;
    updateHighlighting();
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::deleteCharBackwards()
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    // If deleting at the beginning of the line and we have stuff to delete
    if (m_cursorCol == 0 && m_cursorLine != 0)
    {
        --m_cursorLine;
        m_cursorCol = getLineLenAt(m_content, m_cursorLine);
        m_content.erase(m_cursorCharPos-1, 1);
        --m_cursorCharPos;
        m_isModified = true;
    }
    // If deleting in the middle/end of the line and we have stuff to delete
    else if (m_cursorCharPos > 0)
    {
        m_content.erase(m_cursorCharPos-1, 1);
        --m_cursorCol;
        --m_cursorCharPos;
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_isCursorShown = true;
    updateHighlighting();
    scrollViewportToCursor();

    TIMER_END_FUNC();
}

void Buffer::deleteCharForward()
{
    if (m_isReadOnly)
        return;

    TIMER_BEGIN_FUNC();

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    int lineLen = getLineLenAt(m_content, m_cursorLine);

    // If deleting at the end of the line and we have stuff to delete
    if (m_cursorCol == lineLen && m_cursorLine < m_numOfLines)
    {
        m_content.erase(m_cursorCharPos, 1);
        --m_numOfLines;
        m_isModified = true;
    }
    // If deleting in the middle/beginning of the line and we have stuff to delete
    else if (m_cursorCharPos != (size_t)lineLen && m_cursorCharPos < m_content.size())
    {
        m_content.erase(m_cursorCharPos, 1);
        m_isModified = true;
    }

    assert(m_cursorCol >= 0);
    assert(m_cursorLine >= 0);

    m_isCursorShown = true;
    scrollViewportToCursor();
    updateHighlighting();

    TIMER_END_FUNC();
}
