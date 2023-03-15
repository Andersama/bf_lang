#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <iostream>

/*
> : increment pointer
< : decrement pointer
+ : ++*ptr
- : --*ptr
[ : while (*ptr) { ... }
] : (jump to start of while)
, : console input  (1 character)
. : console output (1 character)
*/

enum class bf_instruction : uint32_t {
	inc_memptr,
	dec_memptr,
	inc_memptr_value,
	dec_memptr_value,
	for_start,
	for_end,
	console_input,
	console_output,
	comment
};

enum class bf_error : uint32_t {
	ok,
	mismatched_for_end,
	program_oversized,
	out_of_bounds_access
};

constexpr bf_instruction bf_counterpart(bf_instruction inst) {
	return (inst == bf_instruction::comment) ? inst :
		(bf_instruction)((uint32_t)inst ^ 1u);
}

constexpr std::array<bf_instruction, 256> bf_command_table = []() {
	std::array<bf_instruction, 256> table = {};

	for (size_t i = 0; i < table.size(); i++) {
		table[i] = bf_instruction::comment;
	}

	table['>'] = bf_instruction::inc_memptr;
	table['<'] = bf_instruction::dec_memptr;
	table['+'] = bf_instruction::inc_memptr_value;
	table['-'] = bf_instruction::dec_memptr_value;
	table['['] = bf_instruction::for_start;
	table[']'] = bf_instruction::for_end;
	table[','] = bf_instruction::console_input;
	table['.'] = bf_instruction::console_output;

	return table;
}();

// we're going to do some run-length encoding of the brain fuck language
struct bf_optimized_instruction {
	union {
		int32_t i32;
		int8_t  i8;
		uint32_t jmp = {};
	};

	bf_instruction inst;

	bf_optimized_instruction() = default;
	bf_optimized_instruction(int32_t v, bf_instruction i) {
		i32 = v;
		inst = i;
	}
};

// attaches paired []'s together by their offsets
bf_error bf_handle_jumps(std::vector<bf_optimized_instruction>& instructions, std::vector<uint32_t>& loop_stack) {
	loop_stack.reserve(128);
	for (size_t i = 0; i < instructions.size(); i++) {
		bf_optimized_instruction& current = instructions[i];
		if (current.inst == bf_instruction::for_start) {
			loop_stack.emplace_back(i);
			continue;
		}

		if (current.inst == bf_instruction::for_end) {
			if (loop_stack.size() == 0) {
				return bf_error::mismatched_for_end;
			}
			uint32_t& jump_idx = loop_stack.back();
			current.jmp = jump_idx;
			bf_optimized_instruction& previous = instructions[jump_idx];
			previous.jmp = i;
			loop_stack.pop_back();
		}
	}

	if (loop_stack.size() > 0) {
		return bf_error::mismatched_for_end;
	}

	return bf_error::ok;
}

bf_error bf_parse_append(std::vector<bf_optimized_instruction>& instructions, const char* chars, size_t len) {
	size_t i = 0;
	size_t c = 0;
	size_t o = instructions.size();

	if (i < len) {
		bf_instruction next = bf_command_table[chars[i]];
		instructions.emplace_back(1, next);
		i++;
		c += (next != bf_instruction::comment);
	}

	for (; i < len; i++) {
		bf_instruction next = bf_command_table[chars[i]];
		instructions.emplace_back(0, next);

		bf_optimized_instruction& previous = instructions[c-1];
		c += (next != previous.inst) || (next >= bf_instruction::for_start && next <= bf_instruction::console_output) ||
			(previous.i32 == std::numeric_limits<int32_t>::max());

		bf_optimized_instruction& current = instructions[c-1];
		current.inst = next;
		current.i32 += 1;
	}

	//remove old
	instructions.erase(instructions.begin() + (o + c), instructions.end());

	if (instructions.size() > std::numeric_limits<uint32_t>::max()) {
		return bf_error::program_oversized;
	}

	std::vector<uint32_t> loop_stack;
	return bf_handle_jumps(instructions, loop_stack);
}

bf_error bf_optimize(std::vector<bf_optimized_instruction>& instructions) {
	//optimize inc and decs
	size_t ri = 1;
	size_t li = 0;

	size_t c = 0;
	// loop through instructions, for each pair of operations, combine if allowed
	// discard rhs operation by transforming it into a comment
	// if we find a comment then an operation, swap
	for (;ri < instructions.size();) {
		bf_optimized_instruction& lhs = instructions[li];
		bf_optimized_instruction& rhs = instructions[ri];
		bf_instruction lhs_counterpart = bf_counterpart(lhs.inst);

		if (lhs.inst == rhs.inst) {
			if (lhs.inst == bf_instruction::inc_memptr ||
				lhs.inst == bf_instruction::inc_memptr_value ||
				lhs.inst == bf_instruction::dec_memptr ||
				lhs.inst == bf_instruction::dec_memptr_value) {

				lhs.i32 += rhs.i32;
				//instructions.erase(instructions.begin() + ri);
				rhs.inst = bf_instruction::comment;
				ri++; 				// skip the comment
				continue;
			}

			if (lhs.inst == bf_instruction::comment) {
				ri++; 				// skip the comment
				continue;
			}
		} else if (rhs.inst == lhs_counterpart) {
			if (lhs.inst == bf_instruction::inc_memptr ||
				lhs.inst == bf_instruction::inc_memptr_value ||
				lhs.inst == bf_instruction::dec_memptr ||
				lhs.inst == bf_instruction::dec_memptr_value) {
				lhs.i32 -= rhs.i32;

				rhs.inst = bf_instruction::comment;
				ri++; 				// skip the comment
				continue;
			}

			if (lhs.inst == bf_instruction::comment) {
				ri++; 				// skip the comment
				continue;
			}
		} else {
			if (rhs.inst == bf_instruction::comment) {
				ri++; 				// skip the comment
				continue;
			}

			if (lhs.inst == bf_instruction::comment) {
				// swap comment to the rhs*
				bf_optimized_instruction tmp = lhs;
				lhs = rhs;
				rhs = tmp;

				ri++; 				// skip the comment
				continue;
			}
		}

		li = ri;
		ri++;
	}
	instructions.erase(instructions.begin() + (li+1), instructions.end());

	// remove left over comments and no-ops
	instructions.erase(std::remove_if(instructions.begin(),
	instructions.end(),
	[](const bf_optimized_instruction &x) { 
			return x.inst == bf_instruction::comment || ((x.inst == bf_instruction::inc_memptr ||
				x.inst == bf_instruction::inc_memptr_value ||
				x.inst == bf_instruction::dec_memptr ||
				x.inst == bf_instruction::dec_memptr_value) && x.i32 == 0);
		}),
	instructions.end());
	

	std::vector<uint32_t> loop_stack;
	return bf_handle_jumps(instructions, loop_stack);
}

bf_error bf_run(std::vector<bf_optimized_instruction>& instructions, char* mem, size_t mem_len) {
	size_t m = 0;
	size_t c = 0;
	for (; c < instructions.size();) {
		bf_optimized_instruction& current = instructions[c];
		switch (current.inst) {
		case bf_instruction::inc_memptr:
			c++;
			m += current.i32;
			break;
		case bf_instruction::dec_memptr:
			c++;
			m -= current.i32;
			break;
		case bf_instruction::inc_memptr_value:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			mem[m] += current.i32;
			break;
		case bf_instruction::dec_memptr_value:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			mem[m] -= current.i32;
			break;
		case bf_instruction::for_start: //jump if false
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c = (mem[m] != 0) ? c + 1 : current.jmp + 1;
			break;
		case bf_instruction::for_end: //jump if true
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c = (mem[m] != 0) ? current.jmp + 1 : c + 1;
			break;
		case bf_instruction::console_input:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			std::cin >> mem[m];
			break;
		case bf_instruction::console_output:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			std::cout << mem[m];
			break;
		case bf_instruction::comment:
		default:
			c++;
			break;
		}
	}
	return bf_error::ok;
}

bf_error bf_interpret(const char* chars, size_t len, char* mem, size_t mem_len) {
	size_t m = 0;
	size_t c = 0;
	for (; c < len;) {
		bf_instruction next = bf_command_table[chars[c]];
		switch (next) {
		case bf_instruction::inc_memptr:
			c++;
			m += 1;
			break;
		case bf_instruction::dec_memptr:
			c++;
			m -= 1;
			break;
		case bf_instruction::inc_memptr_value:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			mem[m] += 1;
			break;
		case bf_instruction::dec_memptr_value:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			mem[m] -= 1;
			break;
		case bf_instruction::for_start: //jump if false
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			//c = (mem[m] != 0) ? c + 1 : current.jmp + 1;
			c = c + 1; // default
			if (!mem[m]) {
				size_t count = 1;
				for (; c < len; c++) {
					bf_instruction next_for_end = bf_command_table[chars[c]];
					count += (next_for_end == bf_instruction::for_start) - (next_for_end == bf_instruction::for_end);
					if (!count) {
						c++;
						break;
					}
				}
			}
			break;
		case bf_instruction::for_end: //jump if true
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			//c = (mem[m] != 0) ? current.jmp + 1 : c + 1;
			c = c + 1; // default
			if (mem[m]) {
				size_t count = 1;
				for (c = c-2; c < len; c--) {
					bf_instruction next_for_end = bf_command_table[chars[c]];
					count += (next_for_end == bf_instruction::for_end) - (next_for_end == bf_instruction::for_start);
					if (!count) {
						c++;
						break;
					}
				}
			}
			break;
		case bf_instruction::console_input:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			std::cin >> mem[m];
			break;
		case bf_instruction::console_output:
			if (m >= mem_len) {
				return bf_error::out_of_bounds_access;
			}
			c++;
			std::cout << mem[m];
			break;
		case bf_instruction::comment:
		default:
			c++;
			break;
		}
	}
	return bf_error::ok;
}

void bf_print(std::vector<bf_optimized_instruction>& instructions, std::string &buf) {
	for (size_t i = 0; i < instructions.size(); i++) {
		bf_optimized_instruction& current = instructions[i];
		switch (current.inst) {
		case bf_instruction::inc_memptr:
			if (current.i32 > 0)
				buf.append(current.i32, '>');
			else if (current.i32 < 0)
				buf.append(-current.i32, '<');

			break;
		case bf_instruction::dec_memptr:
			if (current.i32 > 0)
				buf.append(current.i32, '<');
			else if (current.i32 < 0)
				buf.append(-current.i32, '>');

			break;
		case bf_instruction::inc_memptr_value:
			if (current.i32 > 0)
				buf.append(current.i32, '+');
			else if (current.i32 < 0)
				buf.append(-current.i32, '-');
			break;
		case bf_instruction::dec_memptr_value:
			if (current.i32 > 0)
				buf.append(current.i32, '-');
			else if (current.i32 < 0)
				buf.append(-current.i32, '+');

			break;
		case bf_instruction::for_start:
			buf.append(1, '[');
			break;
		case bf_instruction::for_end:
			buf.append(1, ']');
			break;
		case bf_instruction::console_input:
			buf.append(1, ',');
			break;
		case bf_instruction::console_output:
			buf.append(1, '.');
			break;
		default:
			break;
		}
	}
}

bf_error bf_run_debug(std::vector<bf_optimized_instruction>& instructions, const char* chars, size_t len, char* mem, size_t mem_len) {
	instructions.clear();
	bf_error result = bf_parse_append(instructions, chars, len);
	if (result != bf_error::ok)
		return result;
	return result = bf_run(instructions, mem, mem_len);
}

bf_error bf_run_release(std::vector<bf_optimized_instruction>& instructions, const char* chars, size_t len, char* mem, size_t mem_len) {
	instructions.clear();
	bf_error result = bf_parse_append(instructions, chars, len);
	if (result != bf_error::ok)
		return result;
	result = bf_optimize(instructions);
	if (result != bf_error::ok)
		return result;
	return result = bf_run(instructions, mem, mem_len);
}

