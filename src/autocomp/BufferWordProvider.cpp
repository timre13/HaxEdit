#include "BufferWordProvider.h"
#include "Popup.h"
#include "../Logger.h"

namespace Autocomp
{

void BufferWordProvider::get(bufid_t bufid, Popup* popupP)
{
    Logger::dbg << "BufferWordProvider: feeding words into Popup (count: " << m_words.size() << ")" << Logger::End;

    if (m_words.find(bufid) == m_words.end())
        return;

    auto words = m_words.find(bufid)->second;
    for (const auto& word : words)
    {
        popupP->addItem(Popup::Item{Popup::Item::Type::BufferWord, word});
    }
}

void BufferWordProvider::add(bufid_t bufid, const String& word)
{
    if (m_words.find(bufid) == m_words.end())
        m_words.insert({bufid, {word}});
    else
        m_words.at(bufid).push_back(word);
}

void BufferWordProvider::clear()
{
    m_words.clear();
}

} // namespace Autocomp

