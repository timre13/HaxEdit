#include "DictionaryProvider.h"
#include "Popup.h"

namespace Autocomp
{

std::vector<String> DictionaryProvider::s_words;

void DictionaryProvider::get(Popup* popupP)
{
    Logger::dbg << "DictionaryProvider: feeding words into Popup (count: " << s_words.size() << ")" << Logger::End;
    for (const auto& word : s_words)
    {
        popupP->addItem(Popup::Item{Popup::Item::Type::DictionaryWord, word});
    }
}

}
