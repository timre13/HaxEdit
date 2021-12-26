#include "BufferWordProvider.h"
#include "Popup.h"
#include "../Logger.h"

namespace Autocomp
{

void BufferWordProvider::get(Popup* popupP)
{
    Logger::dbg << "BufferWordProvider: feeding words into Popup (count: " << m_words.size() << ")" << Logger::End;
    for (const auto& word : m_words)
    {
        popupP->addItem(Popup::Item{Popup::Item::Type::BufferWord, word});
    }
}

void BufferWordProvider::add(const String& word)
{
    m_words.push_back(word);
}

void BufferWordProvider::clear()
{
    m_words.clear();
}

} // namespace Autocomp

