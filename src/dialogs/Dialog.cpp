#include "Dialog.h"
#include "../Timer.h"
#include "../TextRenderer.h"
#include "../UiRenderer.h"

Dialog::callback_t Dialog::EMPTY_CB = [](int, Dialog*, void*){};

Dialog::Dialog(callback_t cb, void* cbUserData)
    : m_callback{cb}, m_cbUserData{cbUserData}
{
}
