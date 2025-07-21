#include "parse.hpp"

int main(int argc, const char **argv)
{
	ASSERT(argc == 2);
	std::string source = argv[1];
	Lexer lexer { source };
	try {
		AstQuery query = parse_query(lexer);
		query.print();
	}
	catch(const ClientError &error) {
		error.report(source);
	}
}