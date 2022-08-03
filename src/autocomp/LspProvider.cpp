#include "LspProvider.h"
#include "Popup.h"
#include "../Logger.h"
#include "../App.h"
#include "../glstuff.h"
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif // __clang__
#include "LibLsp/lsp/ProcessIoService.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/general/initialized.h"
#include "LibLsp/lsp/general/shutdown.h"
#include "LibLsp/lsp/general/exit.h"
#include "LibLsp/lsp/general/lsClientCapabilities.h"
#include "LibLsp/lsp/general/lsServerCapabilities.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "LibLsp/lsp/textDocument/did_close.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#include "LibLsp/lsp/textDocument/hover.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include "LibLsp/lsp/textDocument/implementation.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <filesystem>
#include <type_traits>

#define LSP_TIMEOUT_MILLI 10000

namespace Autocomp
{

bool didServerCrash = false;

// static
LspProvider::diagListMap_t LspProvider::s_diags{};

static bool publishDiagnosticsCallback(std::unique_ptr<LspMessage> msg)
{
    Logger::dbg << "LSP: Received a textDocument/publishDiagnostics notification: "
        << msg->ToJson() << Logger::End;

    auto notif = dynamic_cast<Notify_TextDocumentPublishDiagnostics::notify*>(msg.get());
    assert(notif);

    Logger::dbg << "Path: " << notif->params.uri.GetAbsolutePath().path << Logger::End;
    Logger::dbg << "Diagn. count: " << notif->params.diagnostics.size() << Logger::End;

    auto insertedIt = LspProvider::s_diags.insert_or_assign(
            notif->params.uri.GetAbsolutePath().path, LspProvider::diagList_t{}).first;
    insertedIt->second = std::move(notif->params.diagnostics);

    g_isRedrawNeeded = true;

    return true; // TODO: What's this?
}

LspProvider::LspProvider()
{
    {
        Logger::log << "Loading LSP status icons" << Logger::End;
        static constexpr const char* statusIconPaths[] = {
            "../img/lsp_status/lsp_ok.png",           // Ok
            "../img/lsp_status/lsp_waiting.png",      // Waiting
            "../img/lsp_status/lsp_crashed.png",      // Crashed
        };
        static_assert(sizeof(statusIconPaths)/sizeof(statusIconPaths[0]) == (size_t)LspServerStatus::_Count);
        for (size_t i{}; i < (size_t)LspServerStatus::_Count; ++i)
        {
            m_statusIcons[i] = std::make_unique<Image>(App::getResPath(statusIconPaths[i]));
        }
    }

    Logger::log << "Starting LSP server" << Logger::End;

    busyBegin();

    static const std::string exe = "/home/mike/.local/share/nvim/plugged/lua-language-server/bin/lua-language-server";
    //static const std::string exe = "ccls";
    //static const std::string exe = "clangd";
    static const std::vector<std::string> args = {
    };

    try
    {
        m_client = std::make_unique<LspClient>(exe, args);
    }
    catch (const std::exception& e)
    {
        didServerCrash = true;
        g_isRedrawNeeded = true;
        return;
    }

    td_initialize::request initreq;
    initreq.params.processId = boost::interprocess::ipcdetail::get_current_process_id();
    initreq.params.clientInfo.emplace();
    {
        initreq.params.clientInfo->name = "HaxEdit";
        initreq.params.clientInfo->version = "0.1";
    }

    Logger::dbg << "LSP: Sending initialize request: " << initreq.ToJson() << Logger::End;

    if (didServerCrash) // Maybe it crashed in the meantime
        return;
    auto initRes = m_client->getEndpoint()->waitResponse(initreq, LSP_TIMEOUT_MILLI);
    if (!initRes)
    {
        Logger::err << "LSP: Initialize timed out" << Logger::End;
        didServerCrash = true;
        g_isRedrawNeeded = true;
        return;
    }

    Logger::dbg << "LSP server response: " << initRes->ToJson() << Logger::End;
    if (initRes->IsError())
    {
        Logger::err << "LSP server responded with error: " << initRes->error.error.ToString() << Logger::End;
        didServerCrash = true;
        g_isRedrawNeeded = true;
        return;
    }
    const auto& cap = initRes->response.result.capabilities;
    Logger::dbg << "\tCompletion supported?: " << (cap.completionProvider ? "YES" : "NO") << Logger::End;
    assert(cap.completionProvider);
    Logger::dbg << "\tHover supported?: " << (cap.hoverProvider ? "YES" : "NO") << Logger::End;
    assert(cap.hoverProvider);


    m_client->getClientEndpoint()->registerNotifyHandler(
            "textDocument/publishDiagnostics",
            publishDiagnosticsCallback
    );

    Notify_InitializedNotification::notify initednotif;
    if (didServerCrash) // Maybe it crashed in the meantime
        return;
    Logger::dbg << "LSP: Sending initialized notification: " << initednotif.ToJson() << Logger::End;

    m_servCaps = std::move(cap);

    busyEnd();
}

void LspProvider::get(bufid_t bufid, Popup* popupP)
{
    Logger::dbg << "LspProvider: " << "TODO" << Logger::End;
}

void LspProvider::busyBegin()
{
    assert(!didServerCrash);
    glfwSetCursor(g_window, Cursors::busy);
    m_status = LspServerStatus::Waiting;
    App::renderStatusLine();
    glfwSwapBuffers(g_window);
}

void LspProvider::busyEnd()
{
    glfwSetCursor(g_window, nullptr);
    m_status = LspServerStatus::Ok;
    App::renderStatusLine();
    glfwSwapBuffers(g_window);
}

void LspProvider::onFileOpen(const std::string& path, Langs::LangId language, const std::string& fileContent)
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
        notif.params.textDocument.languageId = Langs::langIdToLspId(language);
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
    if (didServerCrash) return;

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
    if (didServerCrash) return;

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

LspProvider::HoverInfo LspProvider::getHover(const std::string& path, uint line, uint col)
{
    if (didServerCrash) return {};

    busyBegin();

    if (m_servCaps.hoverProvider.get_value_or(false))
    {
        td_hover::request req;
        req.params.position.line = line;
        req.params.position.character = col;
        req.params.textDocument.uri.SetPath(path);
        Logger::dbg << "LSP: Sending textDocument/hover request: " << req.ToJson() << Logger::End;

        auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
        Logger::dbg << "LSP: Response: " << resp->ToJson() << Logger::End;
        if (resp->IsError())
        {
            busyEnd();
            return {};
        }

        HoverInfo info;
        if (resp->response.result.contents.second)
        {
            info.text = resp->response.result.contents.second->value;
        }
        if (resp->response.result.range)
        {
            info.gotRange  = true;
            info.startLine = resp->response.result.range->start.line;
            info.startCol  = resp->response.result.range->start.character;
            info.endLine   = resp->response.result.range->end.line;
            info.endCol    = resp->response.result.range->end.character;
        }

        busyEnd();
        return info;
    }
    else
    {
        Logger::dbg << "LSP: textDocument/hover is not supported" << Logger::End;
        busyEnd();
        return {};
    }
}

template <typename ReqType>
LspProvider::Location LspProvider::_getDefOrDeclOrImp(const std::string& path, uint line, uint col)
{
    if (didServerCrash) return {};

    busyBegin();

    static_assert(
            std::is_same<ReqType, td_definition::request>()
         || std::is_same<ReqType, td_declaration::request>()
         || std::is_same<ReqType, td_implementation::request>());

    constexpr bool needsDef  = std::is_same<ReqType, td_definition::request>();
    constexpr bool needsDecl = std::is_same<ReqType, td_declaration::request>();
    constexpr bool needsImp  = std::is_same<ReqType, td_implementation::request>();

    bool funcSupported;
    if constexpr (needsDef)
    {
        funcSupported = (m_servCaps.definitionProvider
                && (m_servCaps.definitionProvider->first.get_value_or(false)
                    || m_servCaps.definitionProvider->second));
    }
    else if constexpr (needsDecl)
    {
        funcSupported = true;
    }
    else if constexpr (needsImp)
    {
        funcSupported = true;
    }

    if (funcSupported)
    {
        ReqType req;
        req.params.position.line = line;
        req.params.position.character = col;
        req.params.textDocument.uri.SetPath(path);
        req.params.uri.emplace();
        req.params.uri->SetPath(path);
        if constexpr (needsDef)
            Logger::dbg << "LSP: Sending textDocument/definition request: " << req.ToJson() << Logger::End;
        else if constexpr (needsDecl)
            Logger::dbg << "LSP: Sending textDocument/declaration request: " << req.ToJson() << Logger::End;
        else if constexpr (needsImp)
            Logger::dbg << "LSP: Sending textDocument/implementation request: " << req.ToJson() << Logger::End;

        auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
        Logger::dbg << "LSP: Response: " << resp->ToJson() << Logger::End;
        if (resp->IsError())
        {
            busyEnd();
            return {};
        }

        Location loc;
        if (resp->response.result.first && !resp->response.result.first->empty())
        {
            // TODO: Don't use only the first one
            loc.path = resp->response.result.first.get()[0].uri.GetAbsolutePath().path;
            loc.line = resp->response.result.first.get()[0].range.start.line;
            loc.col = resp->response.result.first.get()[0].range.start.character;
        }
        else if (resp->response.result.second && !resp->response.result.second->empty())
        {
            // TODO: Don't use only the first one
            loc.path = resp->response.result.second.get()[0].targetUri.GetAbsolutePath().path;
            loc.line = resp->response.result.second.get()[0].targetSelectionRange.start.line;
            loc.col = resp->response.result.second.get()[0].targetSelectionRange.start.character;
        }

        busyEnd();
        return loc;
    }
    else
    {
        if constexpr (needsDef)
            Logger::dbg << "LSP: textDocument/definition is not supported" << Logger::End;
        else if constexpr (needsDecl)
            Logger::dbg << "LSP: textDocument/declaration is not supported" << Logger::End;
        else if constexpr (needsImp)
            Logger::dbg << "LSP: textDocument/implementation is not supported" << Logger::End;

        busyEnd();
        return {};
    }
}

LspProvider::Location LspProvider::getDefinition(const std::string& path, uint line, uint col)
{
    return _getDefOrDeclOrImp<td_definition::request>(path, line, col);
}

LspProvider::Location LspProvider::getDeclaration(const std::string& path, uint line, uint col)
{
    return _getDefOrDeclOrImp<td_declaration::request>(path, line, col);
}

LspProvider::Location LspProvider::getImplementation(const std::string& path, uint line, uint col)
{
    return _getDefOrDeclOrImp<td_implementation::request>(path, line, col);
}

LspProvider::codeActionResult_t LspProvider::getCodeActionForLine(const std::string& path, uint line)
{
    if (didServerCrash) return {};
    busyBegin();

    td_codeAction::request req;
    req.params.textDocument.uri.SetPath(path);
    req.params.range.start.line = line;
    req.params.range.start.character = 0;
    req.params.range.end.line = line+1;
    req.params.range.end.character = 0;
    Logger::dbg << "LSP: Sending textDocument/codeAction request: " << req.ToJson() << Logger::End;

    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);

    if (resp->IsError())
    {
        Logger::err << "LSP server responded with error: " << resp->error.error.ToString() << Logger::End;
        busyEnd();
        return {};
    }

    Logger::dbg << "LSP server response: " << resp->ToJson() << Logger::End;

    busyEnd();
    return resp->response.result;
}

std::shared_ptr<Image> LspProvider::getStatusIcon()
{
    if (didServerCrash)
        m_status = LspServerStatus::Crashed;

    assert(m_status < LspServerStatus::_Count);
    return m_statusIcons[(int)m_status];
}

LspProvider::~LspProvider()
{
    if (didServerCrash) return;

    Logger::log << "Shutting down LSP server" << Logger::End;

    // Send shutdown request
    {
        td_shutdown::request shutdownReq;
        Logger::dbg << "LSP: Sending shutdown request: " << shutdownReq.ToJson() << Logger::End;
        auto response = m_client->getEndpoint()->waitResponse(shutdownReq, LSP_TIMEOUT_MILLI);
        if (response->IsError())
        {
            Logger::fatal << "LSP: Server responded with error: " << response->error.error.ToString() << Logger::End;
        }
        Logger::dbg << "LSP: Server response: " << response->ToJson() << Logger::End;
    }

    // Send exit notification
    {
        Notify_Exit::notify exitNotif;
        Logger::dbg << "LSP: Sending exit notification: " << exitNotif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(exitNotif);
    }
}

} // namespace Autocomp
