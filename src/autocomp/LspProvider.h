#pragma once

#include "IProvider.h"
#include "../Logger.h"
#include "../Image.h"
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
#include "LibLsp/lsp/lsp_diagnostic.h"
#ifdef __clang__

#pragma clang diagnostic pop
#endif // __clang__

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
                    }
                })
        );

        if (errCode)
        {
            Logger::fatal << "Failed to start LSP server: ["
                << errCode.category().name() << "]: " << errCode.message() << Logger::End;
        }
        else
        {
            Logger::log << "Started LSP server" << Logger::End;
            m_remoteEndpoint.startProcessingMessages(m_readProxy, m_writeProxy);
        }
    }

    inline RemoteEndPoint* getEndpoint() { return &m_remoteEndpoint; }
    inline GenericEndpoint* getClientEndpoint() { return m_endpoint.get(); }

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

private:
    std::unique_ptr<LspClient> m_client;
    lsServerCapabilities m_servCaps;

    LspServerStatus m_status = LspServerStatus::Waiting;
    std::array<std::shared_ptr<Image>, (int)LspServerStatus::_Count> m_statusIcons;

    void busyBegin();
    void busyEnd();

public:
    LspProvider();

    using diagList_t = std::vector<lsDiagnostic>;
    using diagListMap_t = std::unordered_map<std::string, diagList_t>;
    static diagListMap_t s_diags;

    virtual void get(Popup* popupP) override;

    void onFileOpen(const std::string& path, const std::string& fileContent);
    void onFileChange(const std::string& path, int version, const std::string& newContent);
    void onFileClose(const std::string& path);

    struct HoverInfo
    {
        std::string text;
        bool gotRange = false;
        int startLine = -1;
        int startCol  = -1;
        int endLine   = -1;
        int endCol    = -1;
    };

    HoverInfo getHover(const std::string& path, uint line, uint col);

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

    std::shared_ptr<Image> getStatusIcon() const;

    ~LspProvider();
};

} // namespace Autocomp