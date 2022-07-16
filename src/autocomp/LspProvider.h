#pragma once

#include "IProvider.h"
#include "../Logger.h"
#include <string>
#include <memory>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <system_error>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif // __clang__
#include "LibLsp/lsp/ProcessIoService.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
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
                    Logger::log << "LSP server exited with code "
                        << exitCode << ", error: " << err << Logger::End;
                })
        );

        if (errCode)
        {
            Logger::fatal << "Failed to start LSP server" << Logger::End;
        }
        else
        {
            Logger::log << "Started LSP server" << Logger::End;
        }
    }

    inline RemoteEndPoint* getEndpoint() { return &m_remoteEndpoint; }

    ~LspClient()
    {
        m_remoteEndpoint.stop();
    }
};

class LspProvider final : public IProvider
{
private:
    std::unique_ptr<LspClient> m_client;

public:
    LspProvider();
    ~LspProvider();

    virtual void get(Popup* popupP) override;
};

} // namespace Autocomp
