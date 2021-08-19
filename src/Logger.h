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

        // If this is the beginning of the line, print the "initial"
        // depending on the logger type
        switch (m_type)
        {
        case Type::Debug:   std::cout << (m_isBeginning ? LOGGER_COLOR_DBG "[DBG]" LOGGER_COLOR_DEF ": " : "") << value; break;
        case Type::Log:     std::cout << (m_isBeginning ? LOGGER_COLOR_LOG "[INFO]" LOGGER_COLOR_DEF ": " : "") << value; break;
        case Type::Warning: std::cerr << (m_isBeginning ? LOGGER_COLOR_WARN "[WARN]" LOGGER_COLOR_DEF ": " : "")  << value; break;
        case Type::Error:   std::cerr << (m_isBeginning ? LOGGER_COLOR_ERR "[ERR]" LOGGER_COLOR_DEF ": " : "")   << value; break;
        case Type::Fatal:   std::cerr << (m_isBeginning ? LOGGER_COLOR_FATAL "[FATAL]" LOGGER_COLOR_DEF ": " : "") << value; break;
        default: abort();
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
            exit(1);
            break;
        }

        // We printed the \n, to this is the beginning of the new line
        m_isBeginning = true;

        // Make the operator chainable
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

