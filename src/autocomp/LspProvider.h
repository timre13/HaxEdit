#pragma once

#include "IProvider.h"
#include "../Logger.h"
#include "../Image.h"
#include "../common/string.h"
#include "../languages.h"
#include <string>
#include <memory>
#include <boost/process.hpp>
#include <boost/process/pipe.hpp>
#include <boost/asio.hpp>
#include <system_error>
#include <chrono>
#include <unordered_map>
#include <vector>
using namespace std::chrono_literals;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif // __clang__
#include "LibLsp/lsp/ProcessIoService.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/lsp/general/lsServerCapabilities.h"
#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/JsonRpc/stream.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/lsp_diagnostic.h"
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/textDocument/code_action.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/textDocument/willSave.h"
#include "LibLsp/lsp/workspace/symbol.h"
#include "LibLsp/lsp/workspace/configuration.h"
#ifdef __clang__

#pragma clang diagnostic pop
#endif // __clang__

extern bool g_isRedrawNeeded;

namespace Autocomp
{

namespace bproc = boost::process;

class LspLog final : public lsp::Log
{
public:
    virtual void log(Level level, std::string&& msg) override
    {
        log(level, msg);
    }

    virtual void log(Level level, std::wstring&& msg) override
    {
        assert(false);
        (void)level;
        (void)msg;
    }

    virtual void log(Level level, const std::string& msg) override
    {
        switch (level)
        {
        case lsp::Log::Level::INFO:
        default:
            Logger::dbg << msg << Logger::End;
            break;

        case lsp::Log::Level::SEVERE:
            Logger::err << msg << Logger::End;
            break;
        }
    }

    virtual void log(Level level, const std::wstring& msg) override
    {
        assert(false);
        (void)level;
        (void)msg;
    }
};

struct BoostProcIpstream : lsp::base_istream<bproc::ipstream>
{
public:
    explicit BoostProcIpstream(bproc::ipstream& is)
        : base_istream<bproc::ipstream>(is)
    {
    }

    virtual std::string what() override
    {
        return "";
    }

    virtual void clear() override
    {
    }
};

struct BoostProcOpstream : lsp::base_ostream<bproc::opstream>
{
public:
    explicit BoostProcOpstream(bproc::opstream& _t)
        : lsp::base_ostream<bproc::opstream>(_t)
    {
    }

    virtual std::string what() override
    {
        return "";
    }

    virtual void clear() override
    {
    }
};

extern bool didServerCrash;

class LspClient final
{
public:
    std::shared_ptr<bproc::child> m_child;
    lsp::ProcessIoService m_asioIo;
    bproc::opstream m_writeStream;
    bproc::ipstream m_readStream;
    std::shared_ptr<lsp::ProtocolJsonHandler> m_protJsonHandler
        = std::make_shared<lsp::ProtocolJsonHandler>();
    LspLog m_logger;
    std::shared_ptr<GenericEndpoint> m_endpoint = std::make_shared<GenericEndpoint>(m_logger);
    RemoteEndPoint m_remoteEndpoint{m_protJsonHandler, m_endpoint, m_logger};
    std::shared_ptr<lsp::ostream> m_writeProxy = std::make_shared<BoostProcOpstream>(m_writeStream);
    std::shared_ptr<lsp::istream>  m_readProxy = std::make_shared<BoostProcIpstream>(m_readStream);

    LspClient(const std::string& exePath, const std::vector<std::string>& args)
    {
        Logger::dbg << "Starting LSP server (path=" << exePath << ", args=";
        for (const auto& arg : args)
            Logger::dbg << arg << " ";
        Logger::dbg << ")" << Logger::End;

        std::error_code errCode;
        m_child = std::make_shared<bproc::child>(
                m_asioIo.getIOService(),
                exePath,
                bproc::args = args,
                errCode,
                bproc::std_out > m_readStream,
                bproc::std_in < m_writeStream,
                bproc::on_exit([](int exitCode, const std::error_code& err){
                    if (exitCode == 0)
                    {
                        Logger::dbg << "LSP server exited successfully (code 0)" << Logger::End;
                    }
                    else
                    {
                        Logger::err << "LSP server exited with code "
                            << exitCode << ", error: " << err.message() << Logger::End;
                        didServerCrash = true; // Crashed
                        g_isRedrawNeeded = true; // Update status icon
                        assert(false);
                    }
                })
        );

        if (errCode)
        {
            Logger::err << "Failed to start LSP server: ["
                << errCode.category().name() << "]: " << errCode.message() << Logger::End;

            // Exit thread so we won't get terminated
            {
                Logger::dbg << "Closing LSP endpoint" << Logger::End;
                m_remoteEndpoint.stop();
                Logger::dbg << "Stopping process I/O" << Logger::End;
                m_asioIo.stop();
            }

            didServerCrash = true; // Failed to start
            g_isRedrawNeeded = true; // Update status icon
            assert(false);
            throw std::runtime_error{"Failed to start LSP server: ["s
                + errCode.category().name() + "]: " + errCode.message()};
        }
        else
        {
            Logger::log << "Started LSP server" << Logger::End;
            m_remoteEndpoint.startProcessingMessages(m_readProxy, m_writeProxy);
        }
    }

    inline RemoteEndPoint* getEndpoint() { assert(!didServerCrash); return &m_remoteEndpoint; }
    inline GenericEndpoint* getClientEndpoint() { assert(!didServerCrash); return m_endpoint.get(); }

    ~LspClient()
    {
        Logger::dbg << "Closing LSP endpoint" << Logger::End;
        m_remoteEndpoint.stop();
        Logger::dbg << "Stopping process I/O" << Logger::End;
        m_asioIo.stop();
        Logger::log << "LSP server shutdown routine finished" << Logger::End;
    }
};

class LspProvider final : public IProvider
{
public:
    enum class LspServerStatus
    {
        Ok,
        Waiting,
        Crashed,
        _Count,
    };

    struct WorkProgress
    {
        std::string title;
        std::string message;
        uint percentage{};
    };

private:
    std::unique_ptr<LspClient> m_client;
    std::string m_serverPath;
    boost::optional<ServerInfo> m_servInfo;
    lsServerCapabilities m_servCaps;

    // Don't rely on this. This is only used to display the status icon and its
    // update is delayed. Use `didCrash` to check if the server crashed.
    LspServerStatus m_status = LspServerStatus::Waiting;
    std::array<std::shared_ptr<Image>, (int)LspServerStatus::_Count> m_statusIcons;

    std::map<std::string, WorkProgress> m_progresses;

    void busyBegin();
    void busyEnd();

public:
    LspProvider();

    using diagList_t = std::vector<lsDiagnostic>;
    using diagListMap_t = std::unordered_map<std::string, diagList_t>;
    static diagListMap_t s_diags;

    virtual void get(bufid_t bufid, Popup* popupP) override;

    void onFileOpen(const std::string& path, Langs::LangId language, const std::string& fileContent);
    void onFileChange(const std::string& path, int version, const std::string& newContent);
    void onFileClose(const std::string& path);
    using saveReason_t = WillSaveTextDocumentParams::TextDocumentSaveReason;
    void beforeFileSave(const std::string& path, saveReason_t reason);
    bool onFileSaveNeedsContent() const; // Returns true if `onFileSave()` needs the file contents
    void onFileSave(const std::string& path, const std::string& contentIfNeeded);

    struct HoverInfo
    {
        String text;
        bool gotRange = false;
        int startLine = -1;
        int startCol  = -1;
        int endLine   = -1;
        int endCol    = -1;
    };

    HoverInfo getHover(const std::string& path, uint line, uint col);

    lsSignatureHelp getSignatureHelp(const std::string& path, uint line, uint col);

    struct Location
    {
        std::string path;
        int line = -1;
        int col  = -1;
    };

private:
    template <typename ReqType>
    Location _getDefOrDeclOrImp(const std::string& path, uint line, uint col);

public:
    Location getDefinition(const std::string& path, uint line, uint col);
    Location getDeclaration(const std::string& path, uint line, uint col);
    Location getImplementation(const std::string& path, uint line, uint col);

    using codeActionResult_t = decltype(td_codeAction::response::result);
    codeActionResult_t getCodeActionForLine(const std::string& path, uint line);
    void executeCommand(const std::string& cmd, const boost::optional<std::vector<lsp::Any>>& args);
    // Used by the workspace/applyEdit callback
    void _replyToWsApplyEdit(const std::string& msgIfErr);

    // Used by the workspace/configuration callback
    void _replyToWsConfig(const WorkspaceConfiguration::request* msg);

    struct CanRenameSymbolResult
    {
        bool isError{};
        // If isError is true, this contains an eror message.
        // Otherwise this is the name of the symbol to rename.
        std::string errorOrSymName;
        // `isError` is false and `errorOrSymName` is empty,
        // this contains the range of the name.
        lsRange rangeIfNoName;
    };
    CanRenameSymbolResult canRenameSymbolAt(const std::string& filePath, const lsPosition& pos);
    void renameSymbol(const std::string& filePath, const lsPosition& pos, const std::string& newName);

    using docSymbolResult_t = std::vector<lsDocumentSymbol>;
    docSymbolResult_t getDocSymbols(const std::string& filePath);

    using wpSymbolResult_t = std::vector<lsSymbolInformation>;
    wpSymbolResult_t getWpSymbols(const std::string& query="");

    std::vector<lsTextEdit> getFormattingEdits(const std::string& filePath);
    std::vector<lsTextEdit> getOnTypeFormattingEdits(
            const std::string& filePath, const lsPosition& pos, Char typedChar);

    std::shared_ptr<Image> getStatusIcon();

    // $/progress notification callback
    static bool progressCallback(std::unique_ptr<LspMessage> msg);

    inline const lsServerCapabilities& getCaps() const
    {
        return m_servCaps;
    }


    ~LspProvider();
};

} // namespace Autocomp
