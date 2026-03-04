#include "ast.hpp"
#include "buffer.hpp"
#include "catalog.hpp"
#include "compile.hpp"
#include "error.hpp"
#include "execute.hpp"
#include "lexer.hpp"
#include "parse.hpp"
#include "token.hpp"

#include <cctype>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static std::string Trim(const std::string& text);
static void        ParseAndExecuteStatement(const std::string& source);
static void        ParseAndExecuteFile(const std::string& file_name);

int main(int argc, const char** argv)
{
    buffer::Init();
    catalog::Init();

    if (argc > 1)
    {
        ParseAndExecuteFile(argv[1]);
    }
    else
    {
        std::string source;
        std::printf("> ");
        while (std::getline(std::cin, source))
        {
            source = Trim(source);
            if (source.empty())
            {
                // ignore
            }
            if (source == "q" || source == "quit")
            {
                break;
            }
            ParseAndExecuteStatement(source);
            std::printf("> ");
        }
    }

    buffer::Destroy();
}

static std::string Trim(const std::string& text)
{
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(text[begin]) != 0)
    {
        begin++;
    }
    if (begin == text.size())
    {
        return "";
    }
    std::size_t end = text.size() - 1;
    while (end > begin && std::isspace(text[end]) != 0)
    {
        end--;
    }
    return text.substr(begin, end - begin + 1);
}

static void ParseAndExecuteStatement(const std::string& source)
{
    try
    {
        Lexer        lexer{source};
        AstStatement ast = ParseStatement(lexer);
        lexer.AcceptStep(Token::Semicolon);
        if (!lexer.Accept(Token::End))
        {
            lexer.Unexpected();
        }
        const Statement statement = CompileStatement(ast);
        ExecuteStatement(statement);
    }
    catch (const ClientError& error)
    {
        error.PrintError(source);
    }
    catch (const ServerError& error)
    {
        error.PrintError();
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "server error: %s\n", error.what());
    }
}

static void ParseAndExecuteFile(const std::string& file_name)
{
    std::string source;
    try
    {
        std::ifstream stream{file_name};
        if (!stream)
        {
            throw ClientError{"failed to open file: " + file_name};
        }
        source = {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        Lexer lexer{source};
        while (!lexer.Accept(Token::End))
        {
            const SourceText text_begin = lexer.GetToken().GetText();
            AstStatement     ast        = ParseStatement(lexer);
            const SourceText text_end   = lexer.GetToken().GetText();
            const SourceText text{text_begin, text_end};
            const Statement  statement = CompileStatement(ast);
            std::printf("> ");
            text.PrintEscaped();
            ExecuteStatement(statement);
            lexer.AcceptStep(Token::Semicolon);
        }
    }
    catch (const ClientError& error)
    {
        error.PrintError(source);
    }
    catch (const ServerError& error)
    {
        error.PrintError();
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "server error: %s\n", error.what());
    }
}
