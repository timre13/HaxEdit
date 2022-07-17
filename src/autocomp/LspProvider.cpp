#include "LspProvider.h"
#include "Popup.h"
#include "../Logger.h"
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif // __clang__
#include "LibLsp/lsp/ProcessIoService.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/general/exit.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__

namespace Autocomp
{

LspProvider::LspProvider()
{
    Logger::log << "Starting LSP server" << Logger::End;

    static const std::string exe = "/home/mike/.local/share/nvim/plugged/lua-language-server/bin/lua-language-server";
    //static const std::string exe = "ccls";
    //static const std::string exe = "clangd";
    static const std::vector<std::string> args = {
    };

    m_client = std::make_unique<LspClient>(exe, args);
    td_initialize::request initreq;

    Logger::dbg << "LSP: Sending init request: " << initreq.ToJson() << Logger::End;

    auto rsp = m_client->getEndpoint()->waitResponse(initreq);
    if (rsp)
    {
        Logger::dbg << "LSP server response: " << rsp->ToJson() << Logger::End;
    }
    else
    {
        Logger::err << "LSP: Initialize timed out" << Logger::End;
    }
}

void LspProvider::get(Popup* popupP)
{
    Logger::dbg << "LspProvider: " << "TODO" << Logger::End;
}

LspProvider::~LspProvider()
{
    Logger::log << "Shutting down LSP server" << Logger::End;

    Notify_Exit::notify notify;
    Logger::dbg << "LSP: Sending shutdown notification: " << notify.ToJson() << Logger::End;
    m_client->getEndpoint()->send(notify);
}

} // namespace Autocomp
