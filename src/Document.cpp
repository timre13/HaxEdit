#include "Document.h"
#include <ctime>

void Document::setContent(const String& content)
{
    m_content = splitStrToLines(content, true);
}

lsRange Document::_makeRangeInclusive(lsRange range) const
{
    if (range.end.character == 0)
    {
        --range.end.line;
        assert(range.end.line < m_content.size());
        range.end.character = m_content[range.end.line].size()-1;
    }
    else
    {
        --range.end.character;
    }
    return range;
}

String Document::get(lsRange range) const
{
    Logger::dbg << "Getting from range: " << range.ToString() << Logger::End;

    String output;

    range = _makeRangeInclusive(range);
    int lineI = range.end.line;
    int colI = range.end.character;
    while (true)
    {
        //Logger::dbg << lineI << ':' << colI << Logger::End;

        assert(lineI >= 0);
        assert(lineI < (int)m_content.size());
        assert(colI >= 0);
        assert(colI < (int)m_content[lineI].size());

        output += m_content[lineI][colI];

        // Exit if this was the last char
        if (lineI == (int)range.start.line && colI == (int)range.start.character)
        {
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
            assert(colI >= 0);
        }
    }

    String out = output;
    // We delete backwards, so we must reverse the string
    std::reverse(out.begin(), out.end());
    return out;
}

String Document::_delete_impl(range_t range)
{
    Logger::dbg << "Deleting range: " << range.ToString() << Logger::End;

    String deletedStr;

    range = _makeRangeInclusive(range);
    int lineI = range.end.line;
    int colI = range.end.character;

    while (true)
    {
        //Logger::dbg << lineI << ':' << colI << Logger::End;

        assert(lineI >= 0);
        assert(lineI < (int)m_content.size());
        assert(colI >= 0);
        assert(colI < (int)m_content[lineI].size());

        // If this is the last character of the document
        if (lineI == int(m_content.size())-1 && colI == int(m_content[lineI].size())-1)
        {
            // If this was the only character to be deleted, exit
            if (lineI == (int)range.start.line && colI == (int)range.start.character)
                break;

            // Skip the last character
            goto skip_char;
        }

        deletedStr += m_content[lineI][colI];
        // Do the deletion
        m_content[lineI].erase(colI, 1);
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
                    assert(lineI+1 < (int)m_content.size());
                    const String nextLine = m_content[lineI+1];
                    m_content.erase(m_content.begin()+lineI+1);
                    m_content[lineI].append(nextLine);
                }
            }
            break;
        }

skip_char:

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

    String out = deletedStr;
    // We delete backwards, so we must reverse the deleted string
    std::reverse(out.begin(), out.end());
    return out;
}

size_t Document::delete_(const range_t& range)
{
    const String deletedStr = _delete_impl(range);
    if (deletedStr.empty()) // If there was nothing to delete
        return 0;

    DocumentHistory::Entry::Change change;
    change.type = DocumentHistory::Entry::Change::Type::Deletion;
    change.range = range;
    change.text = deletedStr;
    m_history.add(std::move(change));

    return deletedStr.length();
}

lsPosition Document::_insert_impl(const pos_t& pos, const String& text)
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

    // Return the end
    return {lineI, colI};
}

lsPosition Document::insert(const pos_t& pos, const String& text)
{
    if (text.empty()) // If there is nothing to insert
        return pos;

    const lsPosition endPos = _insert_impl(pos, text);

    DocumentHistory::Entry::Change change;
    change.type = DocumentHistory::Entry::Change::Type::Insertion;
    change.range = {pos, endPos};
    change.text = text;
    m_history.add(std::move(change));

    return endPos;
}

const std::vector<String>& Document::getAll() const
{
    return m_content;
}

Char Document::getChar(const pos_t& pos) const
{
    assert(pos.line < m_content.size());
    const String& line = m_content[pos.line];
    assert(pos.character < line.length());
    return line[pos.character];
}

Char Document::getChar(size_t pos) const
{
    if (isEmpty())
        return '\0';

    size_t i{};
    for (const String& line : m_content)
    {
        if (i + line.size() - 1 < pos)
        {
            i += line.size();
        }
        else
        {
            return line[pos-i];
        }
    }
    Logger::fatal << "Tried to get character at invalid position: "
        << pos << " / " << std::to_string((int)pos) << Logger::End;
    return '\0';
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

DocumentHistory::Entry::ExtraInfo Document::undo()
{
    const DocumentHistory::Entry entry = m_history.goBack();
    for (auto it{entry.changes.rbegin()}; it != entry.changes.rend(); ++it)
    {
        const auto& change = *it;
        if (change.type == DocumentHistory::Entry::Change::Type::Insertion)
            _delete_impl(change.range);
        else
            _insert_impl(change.range.start, change.text);
    }
    return entry.extraInfo;
}

DocumentHistory::Entry::ExtraInfo Document::redo()
{
    const DocumentHistory::Entry entry = m_history.goForward();
    for (const auto& change : entry.changes)
    {
        if (change.type == DocumentHistory::Entry::Change::Type::Insertion)
            _insert_impl(change.range.start, change.text);
        else
            _delete_impl(change.range);
    }
    return entry.extraInfo;
}

void Document::clearHistory()
{
    m_history.clear();
}
