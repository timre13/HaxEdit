#include "DictionaryProvider.h"
#include "Popup.h"
#include "../App.h"
#include "../common/file.h"

namespace Autocomp
{

DictionaryProvider::DictionaryProvider()
{
    Logger::log << "Loading dictionary: " << quoteStr(DICTIONARY_FILE_PATH) << Logger::End;

    try
    {
        const String content = loadUnicodeFile(App::getResPath(DICTIONARY_FILE_PATH));
        LineIterator it = content;
        String line;
        while (it.next(line))
        {
            if (line.length() > 2)
            {
                m_words.push_back(line);
            }
        }
    }
    catch (std::exception& e)
    {
        Logger::err << "Failed to load dictionary: " << quoteStr(DICTIONARY_FILE_PATH)
            << ": " << e.what() << Logger::End;
        throw std::runtime_error{"Failed to load dictionary: "+quoteStr(DICTIONARY_FILE_PATH)+": "+e.what()};
    }

    Logger::log << "Loaded dictionary " << quoteStr(DICTIONARY_FILE_PATH) << " with "
        << m_words.size() << " words (filtered)" << Logger::End;
    if (m_words.size() > 50'000)
    {
        Logger::warn << "Dictionary has a lot of words, autocompletion may be slow" << Logger::End;
    }
}

void DictionaryProvider::get(bufid_t, Popup* popupP)
{
    Logger::dbg << "DictionaryProvider: feeding words into Popup (count: " << m_words.size() << ")" << Logger::End;
    for (const auto& word : m_words)
    {
        Popup::Item item;
        item.label = utf32To8(word);
        item.kind.emplace(lsCompletionItemKind::Text);
        popupP->addItem(std::move(item));
    }
}

}
