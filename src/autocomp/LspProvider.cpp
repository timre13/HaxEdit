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
#include "LibLsp/lsp/general/lsClientCapabilities.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "LibLsp/lsp/textDocument/did_close.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <filesystem>

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
    initreq.params.processId = boost::interprocess::ipcdetail::get_current_process_id();
    initreq.params.clientInfo.emplace();
    {
        initreq.params.clientInfo->name = "HaxEdit";
        initreq.params.clientInfo->version = "0.1";
    }

    Logger::dbg << "LSP: Sending init request: " << initreq.ToJson() << Logger::End;

    auto initRes = m_client->getEndpoint()->waitResponse(initreq);
    if (!initRes)
    {
        Logger::fatal << "LSP: Initialize timed out" << Logger::End;
    }

    Logger::dbg << "LSP server response: " << initRes->ToJson() << Logger::End;
    const auto& cap = initRes->response.result.capabilities;
    Logger::dbg << "\tCompletion supported?: " << (cap.completionProvider ? "YES" : "NO") << Logger::End;
    assert(cap.completionProvider);
    Logger::dbg << "\tHover supported?: " << (cap.hoverProvider ? "YES" : "NO") << Logger::End;
    assert(cap.hoverProvider);
    m_servCaps = std::move(cap);
}

void LspProvider::get(Popup* popupP)
{
    Logger::dbg << "LspProvider: " << "TODO" << Logger::End;
}

void LspProvider::onFileOpen(const std::string& path, const std::string& fileContent)
{
    // TODO: Make the path absolute
    //       or maybe not if it is surely handled outside
    assert(std::filesystem::path{path}.is_absolute());

    // TODO: This should be kept track in the buffer that invoked the method
    static int TODO_docVersion = 0;

    if (m_servCaps.textDocumentSync
     && m_servCaps.textDocumentSync->second
     && m_servCaps.textDocumentSync->second->openClose.get_value_or(false))
    {
        Notify_TextDocumentDidOpen::notify notif;
        notif.params.textDocument.uri.SetPath(path);
        notif.params.textDocument.languageId = "cpp"; // TODO: Detect this
        notif.params.textDocument.version = TODO_docVersion++;
        notif.params.textDocument.text = fileContent;
        Logger::dbg << "LSP: Sending TextDocumentDidOpen notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
}

void LspProvider::onFileChange(const std::string& path)
{
    using tDocSyncKind = lsTextDocumentSyncKind;
    if (m_servCaps.textDocumentSync
     && m_servCaps.textDocumentSync->first.get_value_or(tDocSyncKind::None) != tDocSyncKind::None)
    {
        const bool isIncSyncSupported = m_servCaps.textDocumentSync->first.get() == tDocSyncKind::Incremental;

        // TODO
    }
}

void LspProvider::onFileClose(const std::string& path)
{
    // TODO: Make the path absolute
    //       or maybe not if it is surely handled outside
    assert(std::filesystem::path{path}.is_absolute());

    if (m_servCaps.textDocumentSync
     && m_servCaps.textDocumentSync->second
     && m_servCaps.textDocumentSync->second->openClose.get_value_or(false))
    {
        Notify_TextDocumentDidClose::notify notif;
        notif.params.textDocument.uri.SetPath(path);
        Logger::dbg << "LSP: Sending TextDocumentDidClose notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
}

LspProvider::~LspProvider()
{
    Logger::log << "Shutting down LSP server" << Logger::End;

    Notify_Exit::notify notify;
    Logger::dbg << "LSP: Sending shutdown notification: " << notify.ToJson() << Logger::End;
    m_client->getEndpoint()->send(notify);
}

} // namespace Autocomp
