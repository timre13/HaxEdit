#include "FindDialog.h"
#include "../UiRenderer.h"
#include "../TextRenderer.h"

FindDialog::FindDialog(callback_t cb, void* cbUserData)
    : AskerDialog{cb, cbUserData, ""}
{
    Logger::dbg << "Created a FindDialog" << Logger::End;
}
