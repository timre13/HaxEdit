#include "LspProvider.h"
#include "Popup.h"
#include "../Logger.h"
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif // __clang__
#include "LibLsp/lsp/ProcessIoService.h"
#include "LibLsp/lsp/general/initialize.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__

namespace Autocomp
{

LspProvider::LspProvider()
{
    Logger::log << "Starting LSP server" << Logger::End;

    static const std::string exe = "clangd";
    static const std::vector<std::string> args = {
    };

    m_client = std::make_unique<LspClient>(exe, args);
    // TODO: Properly initialize
    //td_initialize::request initreq;
    //auto rsp = m_client->getEndpoint()->waitResponse(initreq);
    //if (rsp)
    //{
    //    Logger::log << "LSP server response: " << rsp->ToJson() << Logger::End;
    //}
    //else
    //{
    //    Logger::err << "LSP: Initialize timed out" << Logger::End;
    //}
}

LspProvider::~LspProvider()
{
    // TODO: Properly shut down
}

void LspProvider::get(Popup* popupP)
{
    Logger::dbg << "LspProvider: " << "TODO" << Logger::End;
}

} // namespace Autocomp
