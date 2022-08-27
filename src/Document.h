#pragma once

#include "Logger.h"
#include "types.h"
#include "string.h"
#include "Buffer.h"
#include "LibLsp/lsp/lsRange.h"
#include <vector>
#include <stack>

class DocumentHistory final
{
public:
    struct Entry
    {
        struct LineEntry
        {
            String from  = U"\xffffffff";
            String to    = U"\xffffffff";
            size_t lineI = -1_st;
        };
        std::vector<LineEntry> lines;
        size_t oldCursPos  = -1_st;
        int    oldCursLine = -1;
        int    oldCursCol  = -1;
        size_t newCursPos  = -1_st;
        int    newCursLine = -1;
        int    newCursCol  = -1;
    };

private:
    std::stack<Entry> m_undoStack;
    std::stack<Entry> m_redoStack;

public:
    DocumentHistory() {}

    inline void clear()
    {
        while (!m_undoStack.empty()) m_undoStack.pop();
        while (!m_redoStack.empty()) m_redoStack.pop();
    }

    inline void add(const Entry& entry)
    {
        Logger::dbg << "New history entry added (" << entry.lines.size() << " lines)" << Logger::End;
        // Check if the entry has uninitialized values
        assert(!entry.lines.empty());
        assert(entry.oldCursPos  != -1_st);
        assert(entry.oldCursLine != -1);
        assert(entry.oldCursCol  != -1);
        assert(entry.newCursPos  != -1_st);
        assert(entry.newCursLine != -1);
        assert(entry.newCursCol  != -1);
        for (const auto& line : entry.lines)
        {
            assert(line.from != U"\xffffffff");
            assert(line.to != U"\xffffffff");
            assert(line.lineI != -1_st);
            assert(line.from != U"" || line.to != U"");
        }

        while (!m_redoStack.empty()) m_redoStack.pop(); // Clear the redo stack
        m_undoStack.push(entry);
    }

    inline bool canGoBack() const
    {
        return !m_undoStack.empty();
    }

    [[nodiscard]] inline Entry goBack()
    {
        assert(canGoBack());
        Entry entry = m_undoStack.top();
        m_undoStack.pop();
        m_redoStack.push(entry);
        return entry;
    }

    inline bool canGoForward() const
    {
        return !m_redoStack.empty();
    }

    [[nodiscard]] inline Entry goForward()
    {
        assert(canGoForward());
        Entry entry = m_redoStack.top();
        m_redoStack.pop();
        m_undoStack.push(entry);
        return entry;
    }
};

//class Buffer
//{
//    void 
//};
//
class Document final
{
private:
    std::vector<String> m_content;
    DocumentHistory m_history;

public:
    using range_t = lsRange;
    using pos_t = lsPosition;

    Document() {}

private:
    friend size_t Buffer::applyDeletion(const range_t& range);
    friend pos_t Buffer::applyInsertion(const pos_t& pos, const String& text);
    friend void Buffer::open(const std::string& filePath, bool isReload/*=false*/);

    void setContent(const String& content);
    size_t delete_(const range_t& range);
    lsPosition insert(const pos_t& pos, const String& text);
    inline void clearContent() { m_content.clear(); }

public:
    const std::vector<String>& getAll() const;
    Char getChar(const pos_t& pos);
    String getLine(size_t i) const;
    inline String getConcated() const { return lineVecConcat(m_content); }

    inline bool isEmpty() const { return m_content.empty(); }
    inline size_t getLineCount() const { return m_content.size(); }
    size_t getLineLen(size_t i) const;
    inline size_t calcCharCount() const { return countLineListLen(m_content); }

    void assertPos(int line, int col, size_t pos) const;

    // Note: We only allow reading
    inline decltype(m_content)::const_iterator begin() const { return m_content.cbegin(); }
    inline decltype(m_content)::const_iterator end() const { return m_content.cend(); }
};
