#include "Logger.h"

namespace Logger
{

std::string _loggerPrefix;

Logger dbg{Logger::Type::Debug};
Logger log{Logger::Type::Log};
Logger warn{Logger::Type::Warning};
Logger err{Logger::Type::Error};
Logger fatal{Logger::Type::Fatal};

void setLoggerVerbosity(LoggerVerbosity verbosity)
{
    switch (verbosity)
    {
    case LoggerVerbosity::Quiet:
        dbg.m_isEnabled   = false;
        log.m_isEnabled   = false;
        warn.m_isEnabled  = true;
        err.m_isEnabled   = true;
        fatal.m_isEnabled = true;
        break;

    case LoggerVerbosity::Verbose:
        dbg.m_isEnabled   = false;
        log.m_isEnabled   = true;
        warn.m_isEnabled  = true;
        err.m_isEnabled   = true;
        fatal.m_isEnabled = true;
        break;

    case LoggerVerbosity::Debug:
        dbg.m_isEnabled   = true;
        log.m_isEnabled   = true;
        warn.m_isEnabled  = true;
        err.m_isEnabled   = true;
        fatal.m_isEnabled = true;
        break;
    }
}

} // End of namespace Logger

