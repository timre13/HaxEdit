#include "markdown.h"
#include "Logger.h"
#include "common/string.h"

namespace Markdown
{

static void debugLogCb(const char* msg, void*)
{
    Logger::err << "Markdown error: " << msg << Logger::End;
}

static std::string textTypeToStr(MD_TEXTTYPE type)
{
    switch (type)
    {
    case MD_TEXT_NORMAL:    return "normal";
    case MD_TEXT_NULLCHAR:  return "nullchar";
    case MD_TEXT_BR:        return "break";
    case MD_TEXT_SOFTBR:    return "softbreak";
    case MD_TEXT_ENTITY:    return "entity";
    case MD_TEXT_CODE:      return "code";
    case MD_TEXT_HTML:      return "html";
    case MD_TEXT_LATEXMATH: return "latexmath";
    }
}

static std::string spanTypeToStr(MD_SPANTYPE type)
{
    switch (type)
    {
    case MD_SPAN_EM:                return "em";
    case MD_SPAN_STRONG:            return "strong";
    case MD_SPAN_A:                 return "a";
    case MD_SPAN_IMG:               return "img";
    case MD_SPAN_CODE:              return "code";
    case MD_SPAN_DEL:               return "del";
    case MD_SPAN_LATEXMATH:         return "latexmath";
    case MD_SPAN_LATEXMATH_DISPLAY: return "latextmath_display";
    case MD_SPAN_WIKILINK:          return "wikilink";
    case MD_SPAN_U:                 return "u";
    }
}

static std::string blockTypeToStr(MD_BLOCKTYPE type)
{
    switch (type)
    {
    case MD_BLOCK_DOC:    return "doc";
    case MD_BLOCK_QUOTE:  return "quote";
    case MD_BLOCK_UL:     return "ul";
    case MD_BLOCK_OL:     return "ol";
    case MD_BLOCK_LI:     return "li";
    case MD_BLOCK_HR:     return "hr";
    case MD_BLOCK_H:      return "h";
    case MD_BLOCK_CODE:   return "code";
    case MD_BLOCK_HTML:   return "html";
    case MD_BLOCK_P:      return "p";
    case MD_BLOCK_TABLE:  return "table";
    case MD_BLOCK_THEAD:  return "thead";
    case MD_BLOCK_TBODY:  return "tbody";
    case MD_BLOCK_TR:     return "tr";
    case MD_BLOCK_TH:     return "th";
    case MD_BLOCK_TD:     return "td";
    }
}

static int enterBlockCb(MD_BLOCKTYPE block, void* detail, void* output_)
{
    String* output = (String*)output_;

    Logger::dbg << "Markdown: Entered block: " << blockTypeToStr(block) << Logger::End;

    switch (block)
    {
    case MD_BLOCK_DOC:
        break;

    case MD_BLOCK_QUOTE:
        break;

    case MD_BLOCK_UL:
        break;

    case MD_BLOCK_OL:
        break;

    case MD_BLOCK_LI:
        *output += U"\u2022 ";
        break;

    case MD_BLOCK_HR:
        break;

    case MD_BLOCK_H:
        break;

    case MD_BLOCK_CODE:
        break;

    case MD_BLOCK_HTML:
        break;

    case MD_BLOCK_P:
        *output += U"\n";
        break;

    case MD_BLOCK_TABLE:
        break;

    case MD_BLOCK_THEAD:
        break;

    case MD_BLOCK_TBODY:
        break;

    case MD_BLOCK_TR:
        break;

    case MD_BLOCK_TH:
        break;

    case MD_BLOCK_TD:
        break;
    }
    
    return 0; // OK
}

static int leaveBlockCb(MD_BLOCKTYPE block, void* detail, void* output_)
{
    String* output = (String*)output_;

    Logger::dbg << "Markdown: Left block: " << blockTypeToStr(block) << Logger::End;

    switch (block)
    {
    case MD_BLOCK_DOC:
        // Noop
        break;

    case MD_BLOCK_QUOTE:
        break;

    case MD_BLOCK_UL:
        break;

    case MD_BLOCK_OL:
        break;

    case MD_BLOCK_LI:
        break;

    case MD_BLOCK_HR:
        *output += U'\n'+String(40, U'\u2015')+U'\n';
        break;

    case MD_BLOCK_H:
        *output += U'\n';
        break;

    case MD_BLOCK_CODE:
        break;

    case MD_BLOCK_HTML:
        break;

    case MD_BLOCK_P:
        *output += U'\n';
        break;

    case MD_BLOCK_TABLE:
        break;

    case MD_BLOCK_THEAD:
        break;

    case MD_BLOCK_TBODY:
        break;

    case MD_BLOCK_TR:
        break;

    case MD_BLOCK_TH:
        break;

    case MD_BLOCK_TD:
        break;
    }

    return 0; // OK
}

static int enterSpanCb(MD_SPANTYPE span, void* detail, void* output_)
{
    String* output = (String*)output_;

    Logger::dbg << "Markdown: Entered span: " << spanTypeToStr(span) << Logger::End;

    switch (span)
    {
    case MD_SPAN_EM: // Italic
        *output += U"\033[3m";
        break;

    case MD_SPAN_STRONG: // Bold
        *output += U"\033[1m";
        break;

    case MD_SPAN_A:
        break;

    case MD_SPAN_IMG:
        break;

    case MD_SPAN_CODE:
        break;

    case MD_SPAN_DEL:
        break;

    case MD_SPAN_LATEXMATH:
        break;

    case MD_SPAN_LATEXMATH_DISPLAY:
        break;

    case MD_SPAN_WIKILINK:
        break;

    case MD_SPAN_U:
        break;
    }

    return 0; // OK
}

static int leaveSpanCb(MD_SPANTYPE span, void* detail, void* output_)
{
    String* output = (String*)output_;

    Logger::dbg << "Markdown: Left span: " << spanTypeToStr(span) << Logger::End;

    switch (span)
    {
    case MD_SPAN_EM: // Not italic
        *output += U"\033[23m";
        break;

    case MD_SPAN_STRONG: // Not bold
        *output += U"\033[21m";
        break;

    case MD_SPAN_A:
        break;

    case MD_SPAN_IMG:
        break;

    case MD_SPAN_CODE:
        break;

    case MD_SPAN_DEL:
        break;

    case MD_SPAN_LATEXMATH:
        break;

    case MD_SPAN_LATEXMATH_DISPLAY:
        break;

    case MD_SPAN_WIKILINK:
        break;

    case MD_SPAN_U:
        break;
    }

    return 0; // OK
}

static int textCb(MD_TEXTTYPE textType, const MD_CHAR* text, MD_SIZE len, void* output_)
{
    String* output = (String*)output_;
    String text_ = utf8To32(std::string(text, len));
    *output += text_;

    const std::string ttypeStr = textTypeToStr(textType);
    Logger::dbg << "Markdown: Text. Type: " << ttypeStr << ", content: \'" << text_ << '\'' << Logger::End;

    //switch (textType)
    //{
    //case MD_TEXT_NORMAL:
    //    output += text_;
    //    break;

    //case MD_TEXT_NULLCHAR:
    //    output += " ";
    //    break;

    //case MD_TEXT_BR:
    //    output += "\n";
    //    break;

    //case MD_TEXT_SOFTBR:
    //    output += "\n";
    //    break;

    //case MD_TEXT_ENTITY:
    //    // TODO: Support HTML Entities
    //    break;

    //case MD_TEXT_CODE:
    //    output += text_;
    //    break;

    //case MD_TEXT_HTML:
    //    output += text_;
    //    break;

    //case MD_TEXT_LATEXMATH:
    //    output += text_;
    //    break;
    //}

    return 0; // OK
}

String markdownToAnsiEscaped(const std::string& input)
{
    String output;

    MD_PARSER parser;
    parser.abi_version  = 0; // Reserved
    parser.flags        = 0 /*MD_FLAG_STRIKETHROUGH*/;
    parser.enter_block  = enterBlockCb;
    parser.leave_block  = leaveBlockCb;
    parser.enter_span   = enterSpanCb;
    parser.leave_span   = leaveSpanCb;
    parser.text         = textCb;
    parser.debug_log    = debugLogCb;
    parser.syntax       = nullptr; // Reserved

    md_parse(input.c_str(), input.length(), &parser, &output);
    return output;
}

} // namespace Markdown
