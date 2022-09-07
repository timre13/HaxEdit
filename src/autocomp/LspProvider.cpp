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
#include "LibLsp/lsp/workspace/execute_command.h"
#include "LibLsp/lsp/workspace/applyEdit.h"
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

namespace
{

struct ProgressValue
{
    std::string kind;
    std::string title;
    std::string message;
    uint percentage{};
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
        << ", title: \"" << progress.title << '"'
        << ", message: \"" << progress.message << '"'
        << ", percentage: " << progress.percentage
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
        // Create the new token
        auto& insertedRef = Autocomp::lspProvider->m_progresses.emplace(token, WorkProgress{}).first->second;

        insertedRef.title = progress.title;
        insertedRef.message = progress.message;
        insertedRef.percentage = progress.percentage;
    }
    else if (progress.kind == "report") // Update progress
    {
        auto progIt = Autocomp::lspProvider->m_progresses.find(token);
        assert(progIt != Autocomp::lspProvider->m_progresses.end()); // Fail if this is an invalid token
        auto& progRef = progIt->second;

        assert(progress.title.empty()); // The title can't be changed in the report notification

        // Update values if given
        if (!progress.message.empty())
            progRef.message = progress.message;
        if (progress.percentage)
            progRef.percentage = progress.percentage;
    }
    else if (progress.kind == "end") // Delete token
    {
        auto tokenIt = Autocomp::lspProvider->m_progresses.find(token);
        // Fail if attempted to delete a nonexistent token
        assert(tokenIt != Autocomp::lspProvider->m_progresses.end());
        Autocomp::lspProvider->m_progresses.erase(tokenIt);

        assert(progress.title.empty()); // The title can't be changed in the end notification
        assert(progress.percentage == 0); // The percentage can't be changed in the end notification

        // TODO: Show a notification or something with the final message?
    }
    else
    {
        Logger::err << "Invalid $/progress notification kind (ignoring): \"" << progress.kind << '"' << Logger::End;
        return true;
    }

    if (!Autocomp::lspProvider->m_progresses.empty())
    {
        // TODO: Render all of the progresses (FloatingWindows in a vector?)
        const auto& prog = Autocomp::lspProvider->m_progresses.begin()->second;
        g_progressPopup->setTitle(utf8To32(strCapitalize(prog.title)));
        g_progressPopup->setContent(utf8To32(strCapitalize(prog.message)));
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
        m_client = std::make_unique<LspClient>(exe, args);
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

        m_client->getClientEndpoint()->registerNotifyHandler(
                "$/progress",
                progressCallback
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
    auto& caps = initreq.params.capabilities;
    caps.workspace.emplace();
    caps.workspace->applyEdit.emplace(true);
    caps.workspace->workspaceEdit.emplace();
    caps.workspace->workspaceEdit->documentChanges.emplace(true);
    //caps.workspace->workspaceEdit->resourceOperations -- Not supported yet
    caps.workspace->workspaceEdit->failureHandling.emplace("abort"); // Probably
    caps.workspace->workspaceEdit->normalizesLineEndings.emplace(false); // TODO: We should support it
    //caps.workspace->symbol -- TODO: Support searching workspace symbols
    caps.workspace->executeCommand.emplace();
    caps.workspace->workspaceFolders.emplace(true); // TODO: Actually support workspace folders
    //caps.workspace->fileOperations -- TODO: Support file operations
    caps.textDocument.emplace();
    caps.textDocument->synchronization.willSave.emplace(true);
    caps.textDocument->synchronization.didSave.emplace(true);
    caps.textDocument->completion.emplace();
    caps.textDocument->completion->completionItem.emplace();
    //caps.textDocument->completion->completionItem->snippetSupport -- TODO
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
    caps.textDocument->declaration->linkSupport.emplace(false); // TODO: Support `LocationLink`s
    caps.textDocument->definition.emplace();
    caps.textDocument->definition->linkSupport.emplace(false); // TODO: Support `LocationLink`s
    caps.textDocument->implementation.emplace();
    caps.textDocument->implementation->linkSupport.emplace(false); // TODO: Support `LocationLink`s
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
    //caps.textDocument->formatting -- TODO
    //caps.textDocument->rangeFormatting -- TODO
    //caps.textDocument->onTypeFormatting -- TODO
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
        Logger::err << "LSP server responded with error: " << respGetErrMsg(initRes) << Logger::End;
        didServerCrash = true;
        g_isRedrawNeeded = true;
        return;
    }
    if (initRes->response.result.serverInfo)
    {
        Logger::dbg << "Server name: " << initRes->response.result.serverInfo->name << Logger::End;
        Logger::dbg << "Server version: " << initRes->response.result.serverInfo->version.get_value_or("???") << Logger::End;
        g_lspInfoPopup->setTitle(U"LSP Server");
        g_lspInfoPopup->setContent(
                U"Name: "+utf8To32(initRes->response.result.serverInfo->name)+U"\n"
                U"Version: "+utf8To32(initRes->response.result.serverInfo->version.get_value_or("???")));
        g_lspInfoPopup->show();
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

    busyEnd();
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

    Logger::dbg << "LSP: Sending textDocument/completion request: " << req.ToJson() << Logger::End;

    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);
    if (resp->IsError())
    {
        Logger::err << "LSP server responded with error: " << respGetErrMsg(resp) << Logger::End;
        busyEnd();
        return;
    }
    Logger::dbg << "LSP: Response: " << resp->ToJson() << Logger::End;

    for (const auto& item : resp->response.result.items)
    {
        popupP->addItem({std::move(item)});
    }

    //g_activeBuff->getFilePath();
}

void LspProvider::busyBegin()
{
    assert(!didServerCrash);
#ifndef TESTING
    glfwSetCursor(g_window, Cursors::busy);
#endif
    m_status = LspServerStatus::Waiting;
    App::renderStatusLine();
    glfwSwapBuffers(g_window);
}

void LspProvider::busyEnd()
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
    busyBegin();

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

    busyEnd();
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
    busyBegin();

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

    busyEnd();
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
        assert(resp);
        if (resp->IsError())
        {
            Logger::err << "LSP server responded with error: " << respGetErrMsg(resp) << Logger::End;
            busyEnd();
            return {};
        }
        Logger::dbg << "LSP: Response: " << resp->ToJson() << Logger::End;

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

lsSignatureHelp LspProvider::getSignatureHelp(const std::string& path, uint line, uint col)
{
    if (didServerCrash) return {};

    busyBegin();

    td_signatureHelp::request req;
    req.params.position.line = line;
    req.params.position.character = col;
    req.params.textDocument.uri.SetPath(path);
    Logger::dbg << "LSP: Sending textDocument/signatureHelp request: " << req.ToJson() << Logger::End;

    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);
    if (resp->IsError())
    {
        Logger::err << "LSP server responded with error: " << respGetErrMsg(resp) << Logger::End;
        busyEnd();
        return {};
    }
    Logger::dbg << "LSP: Response: " << resp->ToJson() << Logger::End;

    busyEnd();
    return std::move(resp->response.result);
}

template <typename ReqType>
LspProvider::Location LspProvider::_getDefOrDeclOrImp(const std::string& path, uint line, uint col)
{
    if (didServerCrash) return {};

    busyBegin();

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
        Logger::err << "LSP server responded with error: " << respGetErrMsg(resp) << Logger::End;
        busyEnd();
        return {};
    }

    Logger::dbg << "LSP server response: " << resp->ToJson() << Logger::End;

    busyEnd();
    return resp->response.result;
}

void LspProvider::executeCommand(const std::string& cmd, const boost::optional<std::vector<lsp::Any>>& args)
{
    if (didServerCrash) return;
    busyBegin();

    wp_executeCommand::request req;
    req.params.command = cmd;
    req.params.arguments = args;

    Logger::dbg << "LSP: Sending workspace/executeCommand request: " << req.ToJson() << Logger::End;

#if 1
    m_client->getEndpoint()->send(req);
#else
    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);

    if (resp->IsError())
    {
        Logger::err << "LSP server responded with error: " << respGetErrMsg(resp) << Logger::End;
        busyEnd();
        return;
    }

    Logger::dbg << "LSP server response: " << resp->ToJson() << Logger::End;
#endif

    busyEnd();
}

LspProvider::CanRenameSymbolResult LspProvider::canRenameSymbolAt(const std::string& filePath, const lsPosition& pos)
{
    if (didServerCrash) return {.isError=true};
    busyBegin();

    td_prepareRename::request req;
    req.params.textDocument.uri.SetPath(filePath);
    req.params.uri.emplace();
    req.params.uri->SetPath(filePath);
    req.params.position = pos;

    Logger::dbg << "LSP: Sending textDocument/prepareRename request: " << req.ToJson() << Logger::End;
    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);
    if (resp->IsError())
    {
        Logger::err << "LSP: Server responded with error: " << respGetErrMsg(resp) << Logger::End;
        busyEnd();
        return {.isError=true, .errorOrSymName=resp->error.error.message};
    }

    Logger::dbg << "LSP server response: " << resp->ToJson() << Logger::End;

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

    busyEnd();
    return {.isError=false, .errorOrSymName=symName, .rangeIfNoName=range};
}

void LspProvider::renameSymbol(const std::string& filePath, const lsPosition& pos, const std::string& newName)
{
    if (didServerCrash) return;
    busyBegin();

    td_rename::request req;
    req.params.textDocument.uri.SetPath(filePath);
    req.params.position = pos;
    req.params.newName = newName;

    Logger::dbg << "LSP: Sending textDocument/rename request: " << req.ToJson() << Logger::End;
    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);
    if (resp->IsError())
    {
        Logger::err << "LSP: Server responded with error: " << respGetErrMsg(resp) << Logger::End;
        g_statMsg.set("Failed to rename symbol: "+respGetErrMsg(resp), StatusMsg::Type::Error);
        busyEnd();
        return;
    }

    Logger::dbg << "LSP server response: " << resp->ToJson() << Logger::End;
    const auto& edit = resp->response.result;
    applyWorkspaceEdit(edit);

    busyEnd();
}

LspProvider::docSymbolResult_t LspProvider::getDocSymbols(const std::string& filePath)
{
    if (didServerCrash) return {};
    busyBegin();

    td_symbol::request req;
    req.params.textDocument.uri.SetPath(filePath);
    Logger::dbg << "LSP: Sending textDocument/documentSymbol request: " << req.ToJson() << Logger::End;

    auto resp = m_client->getEndpoint()->waitResponse(req, LSP_TIMEOUT_MILLI);
    assert(resp);
    if (resp->IsError())
    {
        Logger::err << "LSP: Server responded with error: " << respGetErrMsg(resp) << Logger::End;
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
            Logger::fatal << "LSP: Server responded with error: " << respGetErrMsg(response) << Logger::End;
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
