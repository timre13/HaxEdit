#pragma once

#include <iostream>

#define LOGGER_COLOR_DEF   "\033[0m"
#define LOGGER_COLOR_DBG   "\033[96m"
#define LOGGER_COLOR_LOG   "\033[32m"
#define LOGGER_COLOR_WARN  "\033[33m"
#define LOGGER_COLOR_ERR   "\033[91m"
#define LOGGER_COLOR_FATAL "\033[101;97m"

namespace Logger
{

enum class LoggerVerbosity
{
    Quiet, // Only print warnings, errors and fatal messages
    Verbose, // Print more stuff
    Debug, // Print a lot of stuff
};

/*
 * Controls the logger
 */
enum Control
{
    End, // Can be used to mark the end of the line
};

extern std::string _loggerPrefix;

class Logger final
{
public:
    /*
     * The type of the logger
     */
    enum class Type
    {
        Debug,
        Log,
        Warning,
        Error,
        Fatal,
    };

private:
    // Whether this is the beginning of the line
    bool m_isBeginning{true};
    // The logger type: info, error, etc.
    Type m_type{};
    // If this logger object is enabled
    bool m_isEnabled{true};

    friend void setLoggerVerbosity(LoggerVerbosity verbosity);

public:
    Logger(Type type)
        : m_type{type}
    {
    }

    template <typename T>
    inline Logger& operator<<(const T &value)
    {
        if (!m_isEnabled)
            return *this;

        if (m_isBeginning)
        {
            switch (m_type)
            {
            case Type::Debug:   std::cout << LOGGER_COLOR_DBG   "[DBG]"   << (_loggerPrefix.empty() ? "" : '<'+_loggerPrefix+'>') << LOGGER_COLOR_DEF ": "; break;
            case Type::Log:     std::cout << LOGGER_COLOR_LOG   "[INFO]"  << (_loggerPrefix.empty() ? "" : '<'+_loggerPrefix+'>') << LOGGER_COLOR_DEF ": "; break;
            case Type::Warning: std::cerr << LOGGER_COLOR_WARN  "[WARN]"  << (_loggerPrefix.empty() ? "" : '<'+_loggerPrefix+'>') << LOGGER_COLOR_DEF ": "; break;
            case Type::Error:   std::cerr << LOGGER_COLOR_ERR   "[ERR]"   << (_loggerPrefix.empty() ? "" : '<'+_loggerPrefix+'>') << LOGGER_COLOR_DEF ": "; break;
            case Type::Fatal:   std::cerr << LOGGER_COLOR_FATAL "[FATAL]" << (_loggerPrefix.empty() ? "" : '<'+_loggerPrefix+'>') << LOGGER_COLOR_DEF ": "; break;
            }
        }

        switch (m_type)
        {
        case Type::Debug:   std::cout << value; break;
        case Type::Log:     std::cout << value; break;
        case Type::Warning: std::cerr << value; break;
        case Type::Error:   std::cerr << value; break;
        case Type::Fatal:   std::cerr << value; break;
        }

        m_isBeginning = false;

        // Make the operator chainable
        return *this;
    }

    inline Logger& operator<<(Control ctrl)
    {
        if (!m_isEnabled)
            return *this;
        if (ctrl != End)
            return *this;

        switch (m_type)
        {
        case Type::Debug:
        case Type::Log:
            std::cout << '\n';
            break;

        case Type::Warning:
        case Type::Error:
            std::cerr << '\n';
            break;

        case Type::Fatal:
            std::cerr << '\n';
            std::cerr << "\n==================== Fatal error. Exiting. ====================\n";
            abort();
            break;
        }

        // We printed the \n, to this is the beginning of the new line
        m_isBeginning = true;

        // Make the operator chainable
        return *this;
    }

    /*
     * Sets the prefix to `prefix`.
     */
    inline Logger& operator()(const std::string& prefix)
    {
        _loggerPrefix = prefix;
        return *this;
    }

    /*
     * If ctrl is End, clears the prefix.
     */
    inline Logger& operator()(Control ctrl)
    {
        if (ctrl == Control::End)
            _loggerPrefix.clear();
        return *this;
    }
};

/*
 * Logger object instances with different types
 */
extern Logger dbg;
extern Logger log;
extern Logger warn;
extern Logger err;
extern Logger fatal;

void setLoggerVerbosity(LoggerVerbosity verbosity);

} // End of namespace Logger

