#include "Document.h"

void Document::setContent(const String& content)
{
    m_content = splitStrToLines(content, true);
}

size_t Document::delete_(const range_t& range)
{
    Logger::dbg << "Deleting range: " << range.ToString() << Logger::End;

    size_t deletedCount{};

    int lineI = range.end.line;
    int colI = range.end.character;
    // The range is exclusive
    if (colI == 0)
    {
        --lineI;
        assert(lineI >= 0);
        assert(lineI < (int)m_content.size());
        colI = m_content[lineI].size()-1;
    }
    else
    {
        --colI;
    }

    while (true)
    {
        Logger::dbg << lineI << ':' << colI << Logger::End;

        assert(lineI >= 0);
        assert(lineI < (int)m_content.size());
        assert(colI >= 0);
        assert(colI < (int)m_content[lineI].size());

        // Do the deletion
        m_content[lineI].erase(colI, 1);
        ++deletedCount;
        if (m_content[lineI].empty())
        {
            m_content.erase(m_content.begin()+lineI);
            assert(colI == 0);
        }

        // Exit if this was the last char
        if (lineI == (int)range.start.line && colI == (int)range.start.character)
        {
            if (colI != 0)
            {
                // If we deleted the line break,
                if (!m_content[lineI].ends_with('\n'))
                {
                    // Append the next line to the end of the current one
                    assert(lineI+1 < (int)m_content.size()); // TODO: Support deleting from end of document
                    const String nextLine = m_content[lineI+1];
                    m_content.erase(m_content.begin()+lineI+1);
                    m_content[lineI].append(nextLine);
                }
            }
            break;
        }

        // Go to next char
        --colI;
        if (colI == -1)
        {
            --lineI;
            assert(lineI >= 0);
            assert(lineI < (int)m_content.size());
            colI = m_content[lineI].size()-1;
            //Logger::dbg << "Line " << lineI << " is " << m_content[lineI].size() << " chars long" << Logger::End;
            //Logger::dbg << "Line: " << strToAscii(m_content[lineI]) << Logger::End;
            assert(colI >= 0);
        }
    }

    // TODO: Add to history

    return deletedCount;
}

lsPosition Document::insert(const pos_t& pos, const String& text)
{
    // TODO: Check position

    int lineI = pos.line;
    int colI = pos.character;
    for (auto c : text)
    {
        m_content[lineI].insert(colI, 1, c);

        ++colI;
        // End
        if (c == '\n')
        {
            const String lineEnd = m_content[lineI].substr(colI);
            m_content[lineI].erase(colI);
            m_content.insert(m_content.begin()+lineI+1, lineEnd);
            colI = 0;

            ++lineI;
        }
    }

    // TODO: Add to history

    // Return the end
    return {lineI, colI};
}

const std::vector<String>& Document::getAll() const
{
    return m_content;
}

Char Document::getChar(const pos_t& pos)
{
    assert(pos.line < m_content.size());
    const String& line = m_content[pos.line];
    assert(pos.character < line.length());
    return line[pos.character];
}

String Document::getLine(size_t i) const
{
    assert(i < m_content.size());
    return m_content[i];
}

size_t Document::getLineLen(size_t i) const
{
    assert(i < m_content.size());
    return m_content[i].size();
}

void Document::assertPos(int line, int col, size_t pos) const
{
    assert(line >= 0);
    assert(m_content.empty() || (size_t)line < m_content.size());
    assert(col >= 0);
    assert(m_content.empty() || pos < countLineListLen(m_content));
}