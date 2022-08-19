#define _DEF_GLOBALS_
#include "globals.h"
#undef _DEF_GLOBALS_
#include "App.h"
#include "common/file.h"

std::string testName = "applyEdit";

static void checkTest(const std::string& result)
{
    static int testId = 0;

    const std::string& expected = loadAsciiFile("../samples/"+testName+"_output"+std::to_string(testId)+".txt");
    if (expected == result)
    {
        Logger::log << "Test '" << testName << " #" << testId << "' passed" << Logger::End;
    }
    else
    {
        Logger::err << "Test '" << testName << " #" << testId << "' FAILED" << Logger::End;
        Logger::err << "Expected: \"" << expected << "\", got: \"" << result << '"' << Logger::End;
        std::exit(1);
    }

    ++testId;
}

class TestRunner
{
public:
    void runTests(Buffer* buffer)
    {
        Logger::log << "---------- Running tests ----------" << Logger::End;

        auto getApplyEditResult = [&](const lsPosition& start, const lsPosition& end, const std::string newText){
            static auto input = splitStrToLines(loadUnicodeFile("../samples/"+testName+"_input.txt"), true);
            buffer->m_content = input;
            buffer->applyEdit(lsTextEdit{{start, end}, newText, {}});
            return strToAscii(lineVecConcat(buffer->m_content));
        };

        // No insertion
        checkTest(getApplyEditResult({ 0,  0}, { 0,  0}, ""));

        // Insertion at line beginning
        checkTest(getApplyEditResult({ 0,  0}, { 0,  0}, "werwqerwer"));
        // Insertion inside line
        checkTest(getApplyEditResult({ 0, 10}, { 0, 10}, "12341234"));
        // Insertion at the end of a line
        checkTest(getApplyEditResult({ 3,  8}, { 3,  8}, "owerwerqwer"));

        // Multi-line insertion at the beginning of a line
        checkTest(getApplyEditResult({ 3,  0}, { 3,  0}, "asdfoiwejrweorijwef\nwef\n\n\nweroj\nwer"));
        // Multi-line insertion at the beginning of a line (starting with a line break
        checkTest(getApplyEditResult({ 3,  0}, { 3,  0}, "\nasdfoiwejrweorijwef\nwef\n\n\nweroj\nwer"));
        // Multi-line insertion at the beginning of a line (ending with a line break)
        checkTest(getApplyEditResult({ 3,  0}, { 3,  0}, "asdfoiwejrweorijwef\nwef\n\n\nweroj\nwer\n"));

        // Multi-line insertion inside a line
        checkTest(getApplyEditResult({ 3,  3}, { 3,  3}, "123941234\n1234\n\n32452345\n\n\n2345\n2345\n6354"));
        // Multi-line insertion inside a line (starting with a line break
        checkTest(getApplyEditResult({ 3,  3}, { 3,  3}, "\n123941234\n1234\n\n32452345\n\n\n2345\n2345\n6354"));
        // Multi-line insertion inside a line (ending with a line break)
        checkTest(getApplyEditResult({ 3,  3}, { 3,  3}, "123941234\n1234\n\n32452345\n\n\n2345\n2345\n6354\n"));

        // Multi-line insertion at the end of a line
        checkTest(getApplyEditResult({ 4,  4}, { 4,  4}, "3428752348975\n1234\n\n123134234\n1234"));
        // Multi-line insertion at the end of a line (starting with a line break
        checkTest(getApplyEditResult({ 4,  4}, { 4,  4}, "\n3428752348975\n1234\n\n123134234\n1234"));
        // Multi-line insertion at the end of a line (ending with a line break)
        checkTest(getApplyEditResult({ 4,  4}, { 4,  4}, "3428752348975\n1234\n\n123134234\n1234\n"));

        // Single-line insertion inside a line (starting with a line break)
        checkTest(getApplyEditResult({ 3,  3}, { 3,  3}, "\n234234"));
        // Single-line insertion inside a line (ending with a line break)
        checkTest(getApplyEditResult({ 3,  3}, { 3,  3}, "234234\n"));

        Logger::log << "---------- Finished running tests ----------" << Logger::End;
    }
};

int main()
{
    Logger::setLoggerVerbosity(Logger::LoggerVerbosity::Verbose);

    TestRunner runner;
    Buffer buffer;
    runner.runTests(&buffer);

    return 0;
}
