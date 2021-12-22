#include "DictionaryProvider.h"
#include "../Logger.h"
#include "../common.h"
#include "../types.h"
#include "../config.h"
#include "Popup.h"

namespace Autocomp
{

DictionaryProvider::DictionaryProvider()
{
    static const std::string dictFilePath = DICTIONARY_FILE_PATH;

    Logger::log << "Loading dictionary: " << quoteStr(dictFilePath) << Logger::End;

    try
    {
        const String content = loadUnicodeFile(dictFilePath);
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
        Logger::err << "Failed to load dictionary: " << quoteStr(dictFilePath)
            << ": " << e.what() << Logger::End;
        return;
    }

    Logger::log << "Loaded dictionary " << quoteStr(dictFilePath) << " with "
        << m_words.size() << " words (filtered)" << Logger::End;
    if (m_words.size() > 50'000)
    {
        Logger::warn << "Dictionary has a lot of words, autocompletion may be slow" << Logger::End;
    }
}

void DictionaryProvider::get(Popup* popupP)
{
    for (const auto& word : m_words)
    {
        popupP->addItem(new Popup::Item{
                Popup::Item::Type::Word,
                word});
    }
}

}
