// bf_lang.cpp : Defines the entry point for the application.
//

#include "bf_lang.h"

using namespace std;

int main()
{
	std::string_view bf_program = "++++++++ ++++++++ ++++++++ ++ > ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++++++++ ++ <[->.-<]";
	char a = 'z';
	std::vector<bf_optimized_instruction> instructions;
	std::vector<char> bf_mem;
	std::string bf_optimized_program;

	bf_optimized_program.reserve(bf_program.size());
	bf_mem.reserve(30000);
	bf_mem.resize(bf_mem.capacity());

	instructions.clear();
	bf_error result = bf_parse_append(instructions, bf_program.data(), bf_program.size());
	if (result != bf_error::ok)
		return (int)result;

	bf_optimized_program.clear();
	bf_print(instructions, bf_optimized_program);
	std::cout << bf_optimized_program << '\n';

	bf_optimize(instructions);
	if (result != bf_error::ok)
		return (int)result;

	bf_optimized_program.clear();
	bf_print(instructions, bf_optimized_program);
	std::cout << bf_optimized_program << '\n';

	result = bf_run(instructions, bf_mem.data(), bf_mem.size());
	if (result != bf_error::ok)
		return (int)result;

	std::cout << '\n';

	std::memset(bf_mem.data(), 0, bf_mem.size());
	bf_interpret(bf_program.data(), bf_program.size(), bf_mem.data(), bf_mem.size());

	return 0;
}
