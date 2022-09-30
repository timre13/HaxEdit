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
    /*
     * An entry is a group of changes.
     *
     * The changes are collected in `m_currEntry` and are pushed to the undo stack
     * by calling `pushEntry()`. Grouping can be useful when one operation is made up
     * of multiple changes. (Example: the block deletion has a change for each affected line.)
     */
    struct Entry
    {
        struct Change
        {
            enum class Type : bool
            {
                Insertion,
                Deletion,
            } type{};
            lsRange range;
            String text;
        };
        std::vector<Change> changes;

        /*
         * Info that is not used by `Document` when undoing/redoing,
         * but is returned from `Document::undo()` and `Document::redo()`, so `Buffer`
         * can use it.
         */
        struct ExtraInfo
        {
            time_t  timestamp;
            struct CursPos
            {
                int     line  = -1;
                int     col   = -1;
                size_t  index = -1;
            };
            CursPos oldCursPos;
            CursPos newCursPos;
        } extraInfo;
    };

private:
    std::stack<Entry> m_undoStack;
    std::stack<Entry> m_redoStack;
    DocumentHistory::Entry m_currEntry;

public:
    DocumentHistory() {}

    inline void clear()
    {
        while (!m_undoStack.empty()) m_undoStack.pop();
        while (!m_redoStack.empty()) m_redoStack.pop();
    }

    inline void add(const DocumentHistory::Entry::Change& change)
    {
        Logger::dbg << "New change added ("
            << "type=" << (change.type == Entry::Change::Type::Insertion ? "INS": "DEL")
            << ", range=" << change.range.ToString()
            << ", lineCount=" << strCountLines(change.text) << ')' << Logger::End;
        m_currEntry.changes.push_back(change);
    }

    inline void beginEntry(int cursLine, int cursCol, size_t cursI)
    {
        m_currEntry.extraInfo.oldCursPos.line = cursLine;
        m_currEntry.extraInfo.oldCursPos.col = cursCol;
        m_currEntry.extraInfo.oldCursPos.index = cursI;
    }

    inline void endEntry(int cursLine, int cursCol, size_t cursI)
    {
        if (!m_currEntry.changes.empty())
        {
            // Fail if `beginEntry()` wasn't called
            assert(m_currEntry.extraInfo.oldCursPos.col != -1
                && m_currEntry.extraInfo.oldCursPos.line != -1
                && m_currEntry.extraInfo.oldCursPos.index != -1_st);

            Logger::dbg << "Pushed a new history entry (" << m_currEntry.changes.size()
                << " changes)" << Logger::End;
            while (!m_redoStack.empty()) m_redoStack.pop(); // Clear the redo stack
            m_currEntry.extraInfo.timestamp = std::time(nullptr);
            m_currEntry.extraInfo.newCursPos.line = cursLine;
            m_currEntry.extraInfo.newCursPos.col = cursCol;
            m_currEntry.extraInfo.newCursPos.index = cursI;

            m_undoStack.push(std::move(m_currEntry));
            assert(m_currEntry.changes.empty());
        }
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
    // To allow calling `delete()`
    friend size_t Buffer::applyDeletion(const range_t&);
    // To allow calling `insert()`
    friend pos_t Buffer::applyInsertion(const pos_t&, const String&);
    // To allow calling `clearContent()`, `setContent()` and `clearHistory()`
    friend void Buffer::open(const std::string& filePath, bool isReload/*=false*/);
     // To allow calling `undo()`
    friend void Buffer::undo();
    // To allow calling `redo()`
    friend void Buffer::redo();
    // To allow calling `m_history->beginEntry()`
    friend void Buffer::beginHistoryEntry();
    // To allow calling `m_history->endEntry()`
    friend void Buffer::endHistoryEntry();

    lsRange _makeRangeInclusive(lsRange range) const;

    /*
     * The actual delete implementation.
     * This does NOT create a history entry.
     *
     * @returns The deleted string.
     * @warning Do not call this (only from `delete_()`, `undo()` and `redo()`.
     * @see `delete_()` for more info.
     */
    String _delete_impl(range_t range);

    /*
     * Delete text from the specified range and create a history entry for the edit.
     *
     * @returns The number of characters deleted.
     */
    size_t delete_(const range_t& range);

    /*
     * The actual insert implementation.
     * This does NOT create a history entry.
     *
     * @returns The last insert position.
     * @warning Do not call this (only from `insert()`, `undo()` and `redo()`.
     * @see `insert()` for more info.
     */
    lsPosition _insert_impl(const pos_t& pos, const String& text);

    /*
     * Insert text to the specified position and create a history entry for the edit.
     *
     * @returns The position AFTER the last inserted character.
     */
    lsPosition insert(const pos_t& pos, const String& text);

    void setContent(const String& content);
    inline void clearContent() { m_content.clear(); }

    DocumentHistory::Entry::ExtraInfo undo();
    DocumentHistory::Entry::ExtraInfo redo();
    void clearHistory();

public:
    String get(lsRange range) const;
    const std::vector<String>& getAll() const;
    Char getChar(const pos_t& pos);
    String getLine(size_t i) const;
    inline String getConcated() const { return lineVecConcat(m_content); }

    inline bool isEmpty() const { return m_content.empty(); }
    inline size_t getLineCount() const { return m_content.size(); }
    size_t getLineLen(size_t i) const;
    inline size_t calcCharCount() const { return countLineListLen(m_content); }

    void assertPos(int line, int col, size_t pos) const;

    inline const DocumentHistory& getHistory() const { return m_history; }

    // Note: We only allow reading
    inline decltype(m_content)::const_iterator begin() const { return m_content.cbegin(); }
    inline decltype(m_content)::const_iterator end() const { return m_content.cend(); }
};
