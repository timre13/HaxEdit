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

    if (m_servCaps.textDocumentSync
     && m_servCaps.textDocumentSync->second
     && m_servCaps.textDocumentSync->second->openClose.get_value_or(false))
    {
        Notify_TextDocumentDidOpen::notify notif;
        notif.params.textDocument.uri.SetPath(path);
        notif.params.textDocument.languageId = "lua"; // TODO: Detect this
        notif.params.textDocument.text = fileContent;
        Logger::dbg << "LSP: Sending textDocument/didOpen notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
    else
    {
        Logger::dbg << "LSP: textDocument/didOpen is not supported" << Logger::End;
    }
}

void LspProvider::onFileChange(const std::string& path, int version, const std::string& newContent)
{
    using tDocSyncKind = lsTextDocumentSyncKind;
    //Logger::log << m_servCaps.textDocumentSync->first.get_value_or(tDocSyncKind::None) << Logger::End;

    // Get the values if available, otherwise default to None
    const tDocSyncKind kindVal1
        = m_servCaps.textDocumentSync
        ? m_servCaps.textDocumentSync->first.get_value_or(tDocSyncKind::None)
        : tDocSyncKind::None;
    const tDocSyncKind kindVal2
        = m_servCaps.textDocumentSync && m_servCaps.textDocumentSync->second
        ? m_servCaps.textDocumentSync->second->change.get_value_or(tDocSyncKind::None)
        : tDocSyncKind::None;
    const tDocSyncKind kindVal = (kindVal1 != tDocSyncKind::None ? kindVal1 : kindVal2);

    if (kindVal != tDocSyncKind::Full)
    {
        const bool isIncSyncSupported = (kindVal == tDocSyncKind::Incremental);
        Logger::dbg << "LSP: Incremental textDocument/didChange supported? " << (isIncSyncSupported ? "YES" : "NO") << Logger::End;

        Notify_TextDocumentDidChange::notify notif;
        notif.params.uri.emplace();
        notif.params.uri->SetPath(path); // Only for old protocol support
        notif.params.textDocument.uri.SetPath(path);
        notif.params.textDocument.version = version;
        notif.params.contentChanges.emplace_back();
        notif.params.contentChanges[0].text = newContent;
        // TODO: Send only difference if supported (isIncSyncSupported)
        //notif.params.contentChanges[0].range; // If this is given, .text is the diff
        Logger::dbg << "LSP: Sending textDocument/didChange notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
    else
    {
        Logger::dbg << "LSP: textDocument/didChange is not supported" << Logger::End;
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
        Logger::dbg << "LSP: Sending textDocument/didClose notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
    else
    {
        Logger::dbg << "LSP: textDocument/didClose is not supported" << Logger::End;
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
