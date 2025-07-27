#include <iostream>
#include <fstream>
#include <streambuf>

#include "buffer.hpp"
#include "catalog.hpp"
#include "parse.hpp"
#include "execute.hpp"

static std::string trim(const std::string &text);
static void parse_and_execute_statement(const std::string &source);
static void parse_and_execute_file(const std::string &file);

int main(int argc, const char **argv)
{
	buffer::init();
	catalog::init();

	if (argc > 1) {
		parse_and_execute_file(argv[1]);
	}
	else {
		std::string source;
		std::printf("> ");
		while (std::getline(std::cin, source)) {
			source = trim(source);
			if (source == "") {
				// ignore
			}
			if (source == "q" || source == "quit") {
				break;
			}
			else {
				parse_and_execute_statement(source);
			}
			std::printf("> ");
		}
	}

	buffer::destroy();
}

static std::string trim(const std::string &text)
{
	size_t begin = 0;
    while (begin < text.size() && std::isspace(text[begin])) {
		begin++;
    }
    if (begin == text.size()) {
		return "";
	}
    size_t end = text.size() - 1;
    while (end > begin && std::isspace(text[end])) {
		end--;
    }
    return text.substr(begin, end - begin + 1);
}

static void parse_and_execute_statement(const std::string &source)
{
	try {
		Lexer lexer { source };
		AstStatement ast = parse_statement(lexer);
		lexer.accept_step(Token::Semicolon);
		if (!lexer.accept(Token::End)) {
			lexer.unexpected();
		}
		const Statement statement = compile_statement(ast);
		execute_statement(statement);
	}
	catch (const ClientError &error) {
		error.print_error(source);
	}
	catch (const ServerError &error) {
		error.print_error();
	}
	catch (const std::exception &error) {
		std::fprintf(stderr, "server error: %s\n", error.what());
	}
}

static void parse_and_execute_file(const std::string &file)
{
	std::string source;
	try {
		std::ifstream stream { file };
		if (!stream) {
			throw ClientError { "failed to open file: " + file };
		}
		source = { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		Lexer lexer { source };
		while (!lexer.accept(Token::End)) {
			const SourceText text_begin = lexer.get_token().get_text();
			AstStatement ast = parse_statement(lexer);
			const SourceText text = text_begin + lexer.get_token().get_text();
			const Statement statement = compile_statement(ast);
			std::printf("> ");
			text.print_escaped();
			execute_statement(statement);
			lexer.accept_step(Token::Semicolon);
		}
	}
	catch (const ClientError &error) {
		error.print_error(source);
	}
	catch (const ServerError &error) {
		error.print_error();
	}
	catch (const std::exception &error) {
		std::fprintf(stderr, "server error: %s\n", error.what());
	}
}
