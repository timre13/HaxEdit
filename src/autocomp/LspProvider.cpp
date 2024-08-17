#include "LspProvider.h"
#include "Popup.h"
#include "../Logger.h"
#include "../App.h"
#include "../glstuff.h"
#include "../globals.h"
#include "../doxygen.h"
#include "../markdown.h"
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
#include "LibLsp/lsp/textDocument/did_save.h"
#include "LibLsp/lsp/textDocument/hover.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include "LibLsp/lsp/textDocument/implementation.h"
#include "LibLsp/lsp/textDocument/prepareRename.h"
#include "LibLsp/lsp/textDocument/rename.h"
#include "LibLsp/lsp/textDocument/completion.h"
#include "LibLsp/lsp/textDocument/formatting.h"
#include "LibLsp/lsp/textDocument/onTypeFormatting.h"
#include "LibLsp/lsp/workspace/execute_command.h"
#include "LibLsp/lsp/workspace/applyEdit.h"
#include "LibLsp/lsp/workspace/configuration.h"
#include "LibLsp/lsp/windows/MessageNotify.h"
#include "LibLsp/lsp/general/progress.h"
#include "LibLsp/lsp/lsAny.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <filesystem>
#include <type_traits>
#include <mutex>

#define LSP_TIMEOUT_MILLI 10000

// See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#workspace_configuration
// This stores server-specific configuration. A key should be the name of a server to be configured.
// Inside there should be a map with sections (see the spec.) as keys.
// Data inside sections should be JSON, which is sent as-is to the server.
// This is the GLOBAL server configuration. Scoped configuration is not yet implemented.
std::map<std::string, std::map<std::string, std::string>> serverConfig{
    {"server-name-one", {
        {"section1", "['my json data', 123]"},
        {"section2", "['my json data', 123]"},
    }},
    {"server-name-two", {
        {"section1", "['my json data', 123]"},
        {"section2", "['my json data', 123]"},
    }},
    {"vscode-html-language-server", {
        {"html", "['my json data', 1]"},
        {"css", "['my json data', 2]"},
        {"javascript", "['my json data', 3]"},
    }},
};

template <typename T>
static std::string respGetErrMsg(const std::unique_ptr<lsp::ResponseOrError<T>>& resp)
{
    return resp->error.error.ToString();
}

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

static void _applyEditsToTabRec(
        const std::vector<Split::child_t>& children,
        const std::string& filePath, const std::vector<lsTextEdit>& edits)
{
    for (auto& child : children)
    {
        if (child.index() == Split::CHILD_TYPE_SPLIT) // A split
        {
            _applyEditsToTabRec(std::get<std::unique_ptr<Split>>(child)->getChildren(), filePath, edits);
        }
        else // A buffer
        {
            auto& buff = std::get<std::unique_ptr<Buffer>>(child);
            if (std::filesystem::canonical(buff->getFilePath()) == filePath)
            {
                buff->applyEdits(edits);
            }
        }
    }
}

static void _applyEditsToFile(const std::string& filePath, const std::vector<lsTextEdit>& edits)
{
    for (const auto& split : g_tabs)
    {
        if (!split->hasChild())
            continue;
        _applyEditsToTabRec(split->getChildren(), filePath, edits);
    }
}

static void applyWorkspaceEdit(const lsWorkspaceEdit& edit)
{
    if (edit.documentChanges) // `documentChanges` is preferred over `changes`
    {
        Logger::dbg << "Will apply changes to " << edit.documentChanges->size() << " files" << Logger::End;
        for (const auto& fileChange : edit.documentChanges.get())
        {
            // TODO: `change` can also be of type TextDocumentEdit, CreateFile,
            // RenameFile or DeleteFile
            //       In that case it is stored in `change.second`
            assert(fileChange.first);

            const auto& filePath =
                fileChange.first.get().textDocument.uri.GetAbsolutePath().path;
            _applyEditsToFile(filePath, fileChange.first.get().edits);
        }
    }
    else if (edit.changes)
    {
        Logger::dbg << "Will apply changes to " << edit.changes->size() << " files" << Logger::End;
        for (const auto& fileChange : edit.changes.get())
        {
            const auto &filePath = std::filesystem::canonical(
                    fileChange.first.starts_with("file://") ? fileChange.first.substr(6) : fileChange.first);
            _applyEditsToFile(filePath, fileChange.second);
        }
    }
}

static bool workspaceApplyEditCallback(std::unique_ptr<LspMessage> msg)
{
    Logger::dbg << "LSP: Received a workspace/applyEdit request: "
        << msg->ToJson() << Logger::End;

    auto req = dynamic_cast<WorkspaceApply::request*>(msg.get());
    assert(req);

    const lsWorkspaceEdit& edit = req->params.edit;
    applyWorkspaceEdit(edit);

    // Tell the server that we applied the edits
    Autocomp::lspProvider->_replyToWsApplyEdit("");

    return true; // TODO: What's this?
}

static bool workspaceConfigurationCallback(std::unique_ptr<LspMessage> msg)
{
    Logger::dbg << "LSP: Received a workspace/configuration request: "
        << msg->ToJson() << Logger::End;

    const auto ptr = dynamic_cast<const WorkspaceConfiguration::request*>(msg.get());
    assert(ptr);
    Autocomp::lspProvider->_replyToWsConfig(ptr);

    return true; // TODO: What's this?
}

namespace
{

struct ProgressValue
{
    std::string kind;
    boost::optional<std::string> title;
    boost::optional<std::string> message;
    boost::optional<uint> percentage{};
};
REFLECT_MAP_TO_STRUCT(ProgressValue, kind, title, message, percentage);

}

static std::mutex progCallbackMutex;
bool LspProvider::progressCallback(std::unique_ptr<LspMessage> msg)
{
    std::lock_guard<std::mutex> lock{progCallbackMutex};

    Logger::dbg << "LSP: Received a $/progress notification: "
        << msg->ToJson() << Logger::End;

    auto notif = dynamic_cast<Notify_Progress::notify*>(msg.get());
    assert(notif);

    std::string token;
    if (notif->params.token.first.has_value())
        token = notif->params.token.first.get();
    else if (notif->params.token.second.has_value())
        token = "#"+std::to_string(notif->params.token.second.get());
    else
        assert(false);

    ProgressValue progress;
    // Parse `value`, an `Any` object to our struct
    notif->params.value.GetFromMap(progress);

    Logger::dbg << "Received a $/progress notification: "
        << "token: " << token
        << ", kind: " << progress.kind
        << ", title: \"" << (progress.title ? progress.title.value() : "<NULL>")  << '"'
        << ", message: \"" << progress.message.value_or("<NULL>") << '"'
        << ", percentage: " << (progress.percentage ? std::to_string(progress.percentage.value()) : "<NULL>")
        << Logger::End;
    /*
     * A progress notification of kind "begin" should begin an already existing progress
     * created by the "window/workDoneProgress/create" request.
     * We don't handle said requests (I find it overkill to have a separate request
     * for progress creation and we'd also need to patch LspCpp to support it), so we create the progress here.
     */
    if (progress.kind == "begin") // Set up a new token
    {
        // Fail if this token already exists
        assert(Autocomp::lspProvider->m_progresses.find(token) == Autocomp::lspProvider->m_progresses.end());
        assert(progress.title.has_value());
        // Create the new token
        auto& insertedRef = Autocomp::lspProvider->m_progresses.emplace(token, WorkProgress{}).first->second;

        insertedRef.title = progress.title.value();
        insertedRef.message = progress.message;
        insertedRef.percentage = progress.percentage;
        assert(!insertedRef.percentage.has_value()
                || (insertedRef.percentage.value() >= 0 && insertedRef.percentage.value() <= 100));
    }
    else if (progress.kind == "report") // Update progress
    {
        auto progIt = Autocomp::lspProvider->m_progresses.find(token);
        assert(progIt != Autocomp::lspProvider->m_progresses.end()); // Fail if this is an invalid token
        auto& progRef = progIt->second;

        assert(!progress.title.has_value()); // The title can't be changed in the report notification

        // Update values if given
        if (progress.message.has_value())
            progRef.message = progress.message;
        progRef.percentage = progress.percentage;
        assert(!progRef.percentage.has_value()
                || (progRef.percentage.value() >= 0 && progRef.percentage.value() <= 100));
    }
    else if (progress.kind == "end") // Delete token
    {
        auto tokenIt = Autocomp::lspProvider->m_progresses.find(token);
        // Fail if attempted to delete a nonexistent token
        assert(tokenIt != Autocomp::lspProvider->m_progresses.end());
        Autocomp::lspProvider->m_progresses.erase(tokenIt);

        assert(!progress.title.has_value()); // The title can't be changed in the end notification
        assert(!progress.percentage.has_value()); // The percentage can't be changed in the end notification

        // TODO: Show a notification or something with the final message?
    }
    else
    {
        Logger::err << "Invalid $/progress notification kind (ignoring): \"" << progress.kind << '"' << Logger::End;
        assert(false);
        return true;
    }

    if (!Autocomp::lspProvider->m_progresses.empty())
    {
        // TODO: Render all of the progresses (FloatingWindows in a vector?)
        const auto& prog = Autocomp::lspProvider->m_progresses.begin()->second;
        g_progressPopup->setTitle(utf8To32(strCapitalize(prog.title)));
        g_progressPopup->setContent(utf8To32(strCapitalize(prog.message.value_or(""))));
        g_progressPopup->setPercentage(prog.percentage);
        g_progressPopup->setPos({
                std::max(g_windowWidth-(int)g_progressPopup->calcWidth()-10, 0),
                std::max(g_windowHeight-(int)g_progressPopup->calcHeight()-g_fontSizePx*2-10, 0),
        });
        g_progressPopup->show();
    }
    else
    {
        g_progressPopup->hideAndClear();
    }
    g_isRedrawNeeded = true;

    return true; // TODO: What's this?
}

void LspProvider::_replyToWorkDoneProgressCreate(const window_workDoneProgressCreate::request* req)
{
    if (didServerCrash) return;

    window_workDoneProgressCreate::response resp;
    resp.id = req->id;

    Logger::dbg << "LSP: Sending reply to window/workDoneProgress/create: " << resp.ToJson() << Logger::End;
    m_client->getEndpoint()->sendResponse(resp);
}

static bool workDoneProgressCreateCallback(std::unique_ptr<LspMessage> msg)
{
    Logger::dbg << "LSP: Received a window/workDoneProgress/create request: "
        << msg->ToJson() << Logger::End;

    auto msg_ = dynamic_cast<const window_workDoneProgressCreate::request*>(msg.get());
    assert(msg_);
    Autocomp::lspProvider->_replyToWorkDoneProgressCreate(msg_);

    return true; // TODO: What's this?
}

static bool windowLogMessageCallback(std::unique_ptr<LspMessage> msg)
{
    const auto msg_ = dynamic_cast<const Notify_LogMessage::notify*>(msg.get());
    assert(msg_);

    switch (msg_->params.type)
    {
    default:
    case lsMessageType::Log:
        Logger::dbg << "[LSP LOG]: " << msg_->params.message << Logger::End;
        break;
    case lsMessageType::Info:
        Logger::log << "[LSP LOG]: " << msg_->params.message << Logger::End;
        break;
    case lsMessageType::Warning:
        Logger::warn << "[LSP LOG]: " << msg_->params.message << Logger::End;
        break;
    case lsMessageType::Error:
        Logger::err << "[LSP LOG]: " << msg_->params.message << Logger::End;
        break;
    }

    return true; // TODO: What's this?
}

static bool clangdFileStatusCallback(std::unique_ptr<LspMessage> msg)
{
    Logger::dbg << "LSP: Received a textDocument/clangd.fileStatus notification: " << msg->ToJson() << Logger::End;

    // See:
    //  - https://clangd.llvm.org/extensions#file-status
    //  - https://github.com/llvm/llvm-project/blob/f0df4fbd0c7b6bb369ceaa1fd6f9e0c88d781ae5/clang-tools-extra/clangd/TUScheduler.cpp#L1576
    //  - https://github.com/llvm/llvm-project/blob/1d0d1f20e7733e3a230f30282c7339f2d3be19c0/clang-tools-extra/clangd/Protocol.h#L1790

    auto msg_ = dynamic_cast<const Notify_ClangdFileStatus::notify*>(msg.get());
    assert(msg_);
    Autocomp::lspProvider->_processClangdFileStatus(msg_);

    return true; // TODO: What's this?
}

void LspProvider::_processClangdFileStatus(const Notify_ClangdFileStatus::notify* msg)
{
    m_fileStatuses.insert_or_assign(msg->params.uri.GetAbsolutePath(), msg->params.state);
    if (g_activeBuff && g_activeBuff->getFilePath() == msg->params.uri.GetAbsolutePath().path)
        g_isRedrawNeeded = true;
}

std::optional<std::string> LspProvider::getStatusForFile(const std::string& path)
{
    const auto& it = m_fileStatuses.find(path);
    if (it == m_fileStatuses.end())
        return {};
    return it->second;
}

LspProvider::LspProvider()
{
    BusynessHandler bh{this};

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

    //const std::string root = "/home/mike/programming/cpp/opengl_text_editor/build";
    const std::string root = g_exeDirPath;
    //static const std::string exe = "/home/mike/.local/share/nvim/plugged/lua-language-server/bin/lua-language-server";
    //static const std::string exe = "ccls";
    static const std::string exe = "/usr/bin/clangd";
    //static const std::string exe = "/home/mike/.local/bin/pyright-langserver";
    //static const std::string exe = "gopls";
    //static const std::string exe = "/usr/local/bin/vscode-html-language-server";
    //static const std::string exe = "invalid";
    static const std::vector<std::string> args = {
       //exe,
       // "--stdio"
    };

    try
    {
        m_client = std::make_unique<LspClient>(m_serverPath, args);
    }
    catch (const std::exception& e)
    {
        didServerCrash = true;
        g_isRedrawNeeded = true;
        return;
    }

    // Register handlers
    {
        m_client->getClientEndpoint()->registerNotifyHandler(
                "textDocument/publishDiagnostics",
                publishDiagnosticsCallback
        );

        m_client->getClientEndpoint()->registerRequestHandler(
                "workspace/applyEdit",
                workspaceApplyEditCallback
        );

        m_client->getClientEndpoint()->registerRequestHandler(
                "workspace/configuration",
                workspaceConfigurationCallback
        );

        m_client->getClientEndpoint()->registerNotifyHandler(
                "$/progress",
                progressCallback
        );

        m_client->getClientEndpoint()->registerRequestHandler(
                "window/workDoneProgress/create",
                workDoneProgressCreateCallback
        );

        m_client->getClientEndpoint()->registerNotifyHandler(
                "window/logMessage",
                windowLogMessageCallback
        );

        // Note: This is a clangd extension
        m_client->getClientEndpoint()->registerNotifyHandler(
                "textDocument/clangd.fileStatus",
                clangdFileStatusCallback
        );
    }

    td_initialize::request initreq;
    initreq.params.processId = boost::interprocess::ipcdetail::get_current_process_id();
    initreq.params.clientInfo.emplace();
    initreq.params.clientInfo->name = "HaxEdit";
    initreq.params.clientInfo->version = "0.1";
    initreq.params.rootPath.emplace();
    initreq.params.rootPath = root;
    initreq.params.rootUri.emplace();
    initreq.params.rootUri->SetPath({root});
    initreq.params.workspaceFolders.emplace();
    initreq.params.workspaceFolders->emplace_back();
    initreq.params.workspaceFolders->back().uri.SetPath({root});
    initreq.params.workspaceFolders->back().name = "root";
    initreq.params.initializationOptions.emplace();
    initreq.params.initializationOptions->SetJsonString("{\"clangdFileStatus\": true}", lsp::Any::kObjectType);
    auto& caps = initreq.params.capabilities;
    caps.workspace.emplace();
    caps.workspace->applyEdit.emplace(true);
    caps.workspace->workspaceEdit.emplace();
    caps.workspace->workspaceEdit->documentChanges.emplace(true);
    //caps.workspace->workspaceEdit->resourceOperations -- Not supported yet
    caps.workspace->workspaceEdit->failureHandling.emplace("abort"); // Probably
    caps.workspace->workspaceEdit->normalizesLineEndings.emplace(false); // TODO: We should support it
    caps.workspace->symbol.emplace();
    caps.workspace->symbol->symbolKind.emplace();
    caps.workspace->symbol->symbolKind->valueSet.emplace();
    for (int val{1}; val <= (int)lsSymbolKind::TypeParameter; ++val)
        caps.workspace->symbol->symbolKind->valueSet->emplace_back((lsSymbolKind)val);
    caps.workspace->executeCommand.emplace();
    caps.workspace->workspaceFolders.emplace(true); // TODO: Actually support workspace folders
    //caps.workspace->fileOperations -- TODO: Support file operations
    caps.workspace->configuration.emplace(true);
    caps.textDocument.emplace();
    caps.textDocument->synchronization.willSave.emplace(true);
    caps.textDocument->synchronization.didSave.emplace(true);
    caps.textDocument->completion.emplace();
    caps.textDocument->completion->completionItem.emplace();
    caps.textDocument->completion->completionItem->snippetSupport.emplace(true);
    caps.textDocument->completion->completionItem->commitCharactersSupport.emplace(false);
    caps.textDocument->completion->completionItem->documentationFormat.emplace();
    caps.textDocument->completion->completionItem->documentationFormat->push_back("markdown");
    caps.textDocument->completion->completionItem->documentationFormat->push_back("plaintext");
    //caps.textDocument->completion->completionItem->deprecatedSupport -- TODO
    //caps.textDocument->completion->completionItem->preselectSupport -- TODO
    //caps.textDocument->completion->completionItem->tagSupport -- TODO (also implement in LspCpp)
    //caps.textDocument->completion->completionItem->insertReplaceSupport -- TODO (also implement in LspCpp)
    caps.textDocument->completion->completionItemKind.emplace();
    caps.textDocument->completion->completionItemKind->valueSet.emplace();
    for (int kind{1}; kind <= (int)lsCompletionItemKind::TypeParameter; ++kind)
        caps.textDocument->completion->completionItemKind->valueSet->push_back((lsCompletionItemKind)kind);
    caps.textDocument->hover.emplace();
    caps.textDocument->hover->contentFormat.emplace();
    caps.textDocument->hover->contentFormat->emplace_back("markdown");
    caps.textDocument->hover->contentFormat->emplace_back("plaintext");
    caps.textDocument->signatureHelp.emplace();
    caps.textDocument->signatureHelp->signatureInformation.emplace();
    //caps.textDocument->signatureHelp->signatureInformation->documentationFormat.push_back("markdown"); -- TODO
    caps.textDocument->signatureHelp->signatureInformation->documentationFormat.push_back("plaintext");
    caps.textDocument->signatureHelp->signatureInformation->parameterInformation.labelOffsetSupport.emplace(false); // I don't know if LspCpp supports it
    caps.textDocument->declaration.emplace();
    caps.textDocument->declaration->linkSupport.emplace(true);
    caps.textDocument->definition.emplace();
    caps.textDocument->definition->linkSupport.emplace(true);
    caps.textDocument->implementation.emplace();
    caps.textDocument->implementation->linkSupport.emplace(true);
    //caps.textDocument->documentHighlight // TODO: Support documentHighlight
    caps.textDocument->documentSymbol.emplace(); // Note: Only used for the breadcrumb bar
    caps.textDocument->documentSymbol->hierarchicalDocumentSymbolSupport.emplace(true);
    caps.textDocument->documentSymbol->symbolKind.emplace();
    caps.textDocument->documentSymbol->symbolKind->valueSet.emplace();
    for (int skind{1}; skind <= (int)lsSymbolKind::TypeParameter; ++skind)
        initreq.params.capabilities.textDocument->documentSymbol->symbolKind->valueSet->push_back((lsSymbolKind)skind);
    caps.textDocument->codeAction.emplace();
    //caps.textDocument->codeLens -- TODO: Support code lens
    //caps.textDocument->documentLink -- TODO
    //caps.textDocument->colorProvider -- TODO
    caps.textDocument->formatting.emplace();
    //caps.textDocument->rangeFormatting -- TODO
    caps.textDocument->onTypeFormatting.emplace();
    caps.textDocument->rename.emplace();
    caps.textDocument->rename->prepareSupport.emplace(true);
    caps.textDocument->publishDiagnostics.emplace();
    //caps.textDocument->publishDiagnostics->relatedInformation -- TODO
    //caps.textDocument->publishDiagnostics->dataSupport -- TODO
    //caps.textDocument->foldingRange -- TODO
    //caps.textDocument->callHierarchy -- TODO: Incoming/outgoing calls
    //caps.textDocument->typeHierarchyCapabilities -- TODO
    //caps.textDocument->inlineValue -- TODO (also support in LspCpp)
    //caps.textDocument->inlayHint -- TODO: Maybe? (too much work) (also support in LspCpp)
    caps.window.emplace();
    caps.window->SetJsonString("{"
                "\"workDoneProgress\": true"
                // TODO: ShowMessage
                // TODO: ShowDocument
            "}",
            lsp::Any::Type::kObjectType);

    const auto initRes = sendRequest<LSP_TIMEOUT_MILLI>(initreq);
    if (!initRes)
    {
        Logger::err << "LSP: Initialize failed" << Logger::End;
        didServerCrash = true;
        g_isRedrawNeeded = true;
        return;
    }

    m_servInfo = initRes->response.result.serverInfo;
    if (m_servInfo)
    {
        Logger::dbg << "Server name: " << m_servInfo->name << Logger::End;
        Logger::dbg << "Server version: " << m_servInfo->version.get_value_or("???") << Logger::End;
        g_lspInfoPopup->setTitle(U"LSP Server");
        g_lspInfoPopup->setContent(
                U"Name: "+utf8To32(m_servInfo->name)+U"\n"
                U"Version: "+utf8To32(m_servInfo->version.get_value_or("???")));
    }
    const auto& cap = initRes->response.result.capabilities;
    Logger::dbg << "\tCompletion supported?: " << (cap.completionProvider ? "YES" : "NO") << Logger::End;
    assert(cap.completionProvider);
    Logger::dbg << "\tHover supported?: " << (cap.hoverProvider ? "YES" : "NO") << Logger::End;
    assert(cap.hoverProvider);

    Notify_InitializedNotification::notify initednotif;
    if (didServerCrash) // Maybe it crashed in the meantime
        return;
    Logger::dbg << "LSP: Sending initialized notification: " << initednotif.ToJson() << Logger::End;

    m_servCaps = std::move(cap);
}

void LspProvider::get(bufid_t bufid, Popup* popupP)
{
    //Logger::dbg << "LspProvider: " << "TODO" << Logger::End;
    if (!g_activeBuff) return;

    td_completion::request req;
    req.params.context.emplace();
    req.params.context->triggerKind = lsCompletionTriggerKind::Invoked;
    req.params.position.line = g_activeBuff->getCursorLine();
    req.params.position.character = g_activeBuff->getCursorCol();
    req.params.textDocument.uri.SetPath(g_activeBuff->getFilePath());
    req.params.uri.emplace();
    req.params.uri->SetPath(g_activeBuff->getFilePath());

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return;

    for (const auto& item : resp->response.result.items)
    {
        popupP->addItem({std::move(item)});
    }

    //g_activeBuff->getFilePath();
}

void LspProvider::_busyBegin()
{
    assert(!didServerCrash);
#ifndef TESTING
    glfwSetCursor(g_window, Cursors::busy);
#endif
    m_status = LspServerStatus::Waiting;
    App::renderStatusLine();
    glfwSwapBuffers(g_window);
}

void LspProvider::_busyEnd()
{
#ifndef TESTING
    glfwSetCursor(g_window, nullptr);
#endif
    m_status = LspServerStatus::Ok;
    App::renderStatusLine();
    glfwSwapBuffers(g_window);
}

void LspProvider::_replyToWsApplyEdit(const std::string& msgIfErr)
{
    if (didServerCrash) return;

    WorkspaceApply::response resp;
    if (msgIfErr.empty()) // No error
    {
        resp.result.applied = true;
    }
    else // Error
    {
        resp.result.applied = false;
        resp.result.failureReason.emplace(msgIfErr);
    }

    Logger::dbg << "LSP: Sending reply to workspace/applyEdit: " << resp.ToJson() << Logger::End;
    m_client->getEndpoint()->sendResponse(resp);
}

void LspProvider::_replyToWsConfig(const WorkspaceConfiguration::request* msg)
{
    if (didServerCrash) return;

    WorkspaceConfiguration::response resp;
    resp.id = msg->id;

    const auto servName = m_servInfo.has_value() ? m_servInfo->name : std::filesystem::path{m_serverPath}.filename().string();
    if (serverConfig.contains(servName))
    {
        Logger::dbg << "Found configuration for the server: " << servName << Logger::End;
        const auto& confForServer = serverConfig.at(servName);
        for (const auto& item : msg->params.items)
        {
            if (confForServer.contains(item.section))
            {
                Logger::dbg << "Found configuration section: " << item.section << Logger::End;
                // TODO: Implement scope (item.scopeUri)
                Logger::dbg << "Configuration scope: " << item.scopeUri.GetRawPath() << Logger::End;
                lsp::Any val;
                val.SetJsonString(confForServer.at(item.section), lsp::Any::kUnKnown);
                resp.result.push_back(val);
            }
            else
            {
                Logger::dbg << "No such configuration section: " << item.section << Logger::End;
            }
        }
    }
    else
    {
        Logger::dbg << "There is no configuration for the server: " << servName << Logger::End;
    }

    Logger::dbg << "LSP: Sending reply to workspace/configuration: " << resp.ToJson() << Logger::End;
    m_client->getEndpoint()->sendResponse(resp);
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
#ifndef TESTING
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
#endif
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

void LspProvider::beforeFileSave(const std::string& path, LspProvider::saveReason_t reason)
{
    if (didServerCrash) return;
    BusynessHandler bh{this};

    if (m_servCaps.textDocumentSync
     && m_servCaps.textDocumentSync->second
     && m_servCaps.textDocumentSync->second->willSave.get_value_or(false))
    {
        td_willSave::notify notif;
        notif.params.textDocument.uri.SetPath(path);
        notif.params.reason.emplace(reason);
        Logger::dbg << "LSP: Sending textDocument/willSave notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
    else
    {
        Logger::dbg << "LSP: textDocument/willSave is not supported" << Logger::End;
    }
}

bool LspProvider::onFileSaveNeedsContent() const
{
    return (
            m_servCaps.textDocumentSync
            && m_servCaps.textDocumentSync->second
            && m_servCaps.textDocumentSync->second->save
            && m_servCaps.textDocumentSync->second->save->includeText);
}

void LspProvider::onFileSave(const std::string& path, const std::string& contentIfNeeded)
{
    if (didServerCrash) return;
    BusynessHandler bh{this};

    if (m_servCaps.textDocumentSync
     && m_servCaps.textDocumentSync->second
     && m_servCaps.textDocumentSync->second->save)
    {
        Notify_TextDocumentDidSave::notify notif;
        notif.params.textDocument.uri.SetPath(path);
        if (!contentIfNeeded.empty())
            notif.params.text.emplace(contentIfNeeded);

        Logger::dbg << "LSP: Sending textDocument/didSave notification: " << notif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(notif);
    }
    else
    {
        Logger::dbg << "LSP: textDocument/didSave is not supported" << Logger::End;
    }
}

LspProvider::HoverInfo LspProvider::getHover(const std::string& path, uint line, uint col)
{
    BusynessHandler bh(this);

    if (m_servCaps.hoverProvider.get_value_or(false))
    {
        td_hover::request req;
        req.params.position.line = line;
        req.params.position.character = col;
        req.params.textDocument.uri.SetPath(path);

        auto resp = sendRequest<500>(req);
        if (!resp) return {};

        HoverInfo info;
        if (resp->response.result.contents.second)
        {
            const auto& markupContent = resp->response.result.contents.second.get();
            if (markupContent.kind == "markdown") // Kind can be plaintext or markdown
            {
                // Note: Can contain code blocks (MarkupContent is preferred over MarkedString
                //       because they can contain markdown with code inside, but MarkedString
                //       can only contain code without markdown)
                info.text = Markdown::markdownToAnsiEscaped(Doxygen::doxygenToAnsiEscaped(markupContent.value));
            }
            else
            {
                if (markupContent.kind != "plaintext")
                    Logger::warn << "Unknown MarkupContent kind: " << markupContent.kind << Logger::End;
                info.text = utf8To32(Doxygen::doxygenToAnsiEscaped(markupContent.value));
            }
        }
        else if (resp->response.result.contents.first)
        {
            const auto& markedStrings = resp->response.result.contents.first.get();
            info.text = U"";
            for (const auto& mstr : markedStrings)
            {
                // If the values are plain text objects
                if (mstr.first.has_value())
                {
                    info.text += utf8To32(mstr.first.get()) + U'\n';
                }
                // If the values are MarkedStrings
                else if (mstr.second.has_value())
                {
                    if (mstr.second->language.get_value_or("").empty())
                    {
                        info.text += utf8To32(mstr.second->value) + U'\n';
                    }
                    else
                    {
                        // TODO: Highlight based on the specified language
                        info.text += utf8To32(mstr.second->value) + U'\n';
                        Logger::warn << "Unimplemented syntax highligthing for MarkedString language: "
                            << mstr.second->language.get() << Logger::End;
                    }
                }
                else
                {
                    assert(false);
                }
            }
        }
        info.text = strTrimTrailingLineBreak(info.text);

        if (resp->response.result.range)
        {
            info.gotRange  = true;
            info.startLine = resp->response.result.range->start.line;
            info.startCol  = resp->response.result.range->start.character;
            info.endLine   = resp->response.result.range->end.line;
            info.endCol    = resp->response.result.range->end.character;
        }

        return info;
    }
    else
    {
        Logger::dbg << "LSP: textDocument/hover is not supported" << Logger::End;
        return {};
    }
}

lsSignatureHelp LspProvider::getSignatureHelp(const std::string& path, uint line, uint col)
{
    BusynessHandler bh{this};

    td_signatureHelp::request req;
    req.params.position.line = line;
    req.params.position.character = col;
    req.params.textDocument.uri.SetPath(path);

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return {};
    return std::move(resp->response.result);
}

template <typename ReqType>
LspProvider::Location LspProvider::_getDefOrDeclOrImp(const std::string& path, uint line, uint col)
{
    BusynessHandler bn{this};

    constexpr bool needsDef  = std::is_same<ReqType, td_definition::request>();
    constexpr bool needsDecl = std::is_same<ReqType, td_declaration::request>();
    constexpr bool needsImp  = std::is_same<ReqType, td_implementation::request>();
    static_assert(needsDef || needsDecl || needsImp);

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

        const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
        if (!resp) return {};

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
    BusynessHandler bh{this};

    td_codeAction::request req;
    req.params.textDocument.uri.SetPath(path);
    req.params.range.start.line = line;
    req.params.range.start.character = 0;
    req.params.range.end.line = line+1;
    req.params.range.end.character = 0;

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return {};

    return resp->response.result;
}

void LspProvider::executeCommand(const std::string& cmd, const boost::optional<std::vector<lsp::Any>>& args)
{
    BusynessHandler bh{this};

    wp_executeCommand::request req;
    req.params.command = cmd;
    req.params.arguments = args;

#if 0
    Logger::dbg << "LSP: Sending workspace/executeCommand request: " << req.ToJson() << Logger::End;
    m_client->getEndpoint()->send(req);
#else
    sendRequest<LSP_TIMEOUT_MILLI>(req);
#endif
}

LspProvider::CanRenameSymbolResult LspProvider::canRenameSymbolAt(const std::string& filePath, const lsPosition& pos)
{
    if (didServerCrash) return {.isError=true};
    BusynessHandler bh{this};

    td_prepareRename::request req;
    req.params.textDocument.uri.SetPath(filePath);
    req.params.uri.emplace();
    req.params.uri->SetPath(filePath);
    req.params.position = pos;

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp)
    {
        // FIXME: `resp` is NULL here
        //if (resp->is_error)
        //    return {.isError=true, .errorOrSymName=resp->error.error.message};
        //return {.isError=true, .errorOrSymName="Timed out"};
        return {.isError=true, .errorOrSymName="Error"};
    }

    std::string symName;
    lsRange range;
    if (resp->response.result.first)
    {
        range = resp->response.result.first.get();
    }
    else if (resp->response.result.second)
    {
        const auto result = resp->response.result.second.get();
        assert(!result.placeholder.empty());
        symName = result.placeholder;
    }
    else
    {
        assert(false);
    }

    return {.isError=false, .errorOrSymName=symName, .rangeIfNoName=range};
}

void LspProvider::renameSymbol(const std::string& filePath, const lsPosition& pos, const std::string& newName)
{
    BusynessHandler bh{this};

    td_rename::request req;
    req.params.textDocument.uri.SetPath(filePath);
    req.params.position = pos;
    req.params.newName = newName;

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp)
    {
        // FIXME: Resp is NULL here
        //g_statMsg.set(
        //        "Failed to rename symbol: "+(resp->is_error ? respGetErrMsg(resp) : "Request timed out"),
        //        StatusMsg::Type::Error);
        g_statMsg.set(
                "Failed to rename symbol",
                StatusMsg::Type::Error);
        return;
    }

    const auto& edit = resp->response.result;
    applyWorkspaceEdit(edit);
}

LspProvider::docSymbolResult_t LspProvider::getDocSymbols(const std::string& filePath)
{
    BusynessHandler bh{this};

    td_symbol::request req;
    req.params.textDocument.uri.SetPath(filePath);

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return {};

    return resp->response.result;
}

LspProvider::wpSymbolResult_t LspProvider::getWpSymbols(const std::string& query/*=""*/)
{
    BusynessHandler bh{this};

    wp_symbol::request req;
    req.params.query = query;

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return {};

    return resp->response.result;
}

std::vector<lsTextEdit> LspProvider::getFormattingEdits(const std::string& filePath)
{
    BusynessHandler bh{this};

    td_formatting::request req;
    req.params.textDocument.uri.SetPath(filePath);
    req.params.options.tabSize = 4;
    req.params.options.insertSpaces = true;
    req.params.options.trimTrailingWhitespace.emplace(true);
    req.params.options.insertFinalNewline.emplace(true);
    req.params.options.trimFinalNewlines.emplace(true);

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return {};

    return resp->response.result;
}

std::vector<lsTextEdit> LspProvider::getOnTypeFormattingEdits(
        const std::string& filePath, const lsPosition& pos, Char typedChar)
{
    BusynessHandler bh{this};

    td_onTypeFormatting::request req;
    req.params.textDocument.uri.SetPath(filePath);
    req.params.position = pos;
    req.params.ch = utf32To8(charToStr(typedChar));
    req.params.options.tabSize = 4;
    req.params.options.insertSpaces = true;
    req.params.options.trimTrailingWhitespace.emplace(true);
    req.params.options.insertFinalNewline.emplace(true);
    req.params.options.trimFinalNewlines.emplace(true);

    const auto resp = sendRequest<LSP_TIMEOUT_MILLI>(req);
    if (!resp) return {};

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
    Logger::log << "Shutting down LSP server" << Logger::End;

    // Send shutdown request
    {
        td_shutdown::request shutdownReq;
        sendRequest<LSP_TIMEOUT_MILLI>(shutdownReq);
    }

    // Send exit notification
    {
        Notify_Exit::notify exitNotif;
        Logger::dbg << "LSP: Sending exit notification: " << exitNotif.ToJson() << Logger::End;
        m_client->getEndpoint()->send(exitNotif);
    }
}

} // namespace Autocomp
