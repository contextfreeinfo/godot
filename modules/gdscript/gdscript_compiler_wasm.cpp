/**************************************************************************/
/*  gdscript_compiler_wasm.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gdscript_compiler_wasm.h"
#include "core/io/dir_access.h"

#include <string>

/*
2025-06-11T15:02:52Z tom@hierba:~/projects/godot
$ time scons platform=linuxbsd target=editor tests=yes -j8
scons: Reading SConscript files ...
Building for platform "linuxbsd", architecture "x86_64", target "editor".
scons: done reading SConscript files.
scons: Building targets ...
[ 23%] Compiling modules/gdscript/gdscript_compiler_wasm.cpp ...
[ 99%] Linking Static Library bin/obj/modules/libmodule_gdscript.linuxbsd.editor.x86_64.a ...
Ranlib Library bin/obj/modules/libmodule_gdscript.linuxbsd.editor.x86_64.a ...
[ 99%] Linking Program bin/godot.linuxbsd.editor.x86_64 ...
[100%] scons: done building targets.
INFO: Time elapsed: 00:00:19.72
real	0m21.933s
user	0m22.237s
sys	0m1.515s
2025-06-11T15:09:16Z tom@hierba:~/projects/godot
$ time ./bin/godot.linuxbsd.editor.x86_64 --test --test-suite="[Modules][GDScript]"
[doctest] doctest version is "2.4.11"
[doctest] run with "--help" for options
===============================================================================
[doctest] test cases:   1 |   1 passed | 0 failed | 1239 skipped
[doctest] assertions: 598 | 598 passed | 0 failed |
[doctest] Status: SUCCESS!
real	0m0.699s
user	0m0.613s
sys	0m0.086s
2025-06-11T15:09:18Z tom@hierba:~/projects/godot
2025-06-11T15:03:16Z tom@hierba:/tmp/tom-godot
$ wasm2wat --generate-names --fold-exprs recursion.gd.wasm
(module
  (type $t0 (func (param i32)))
  (type $t1 (func (param i32)))
  (type $t2 (func (param i32)))
  (func $is_prime/2 (type $t0) (param $p0 i32))
  (func $is_prime/1 (type $t1) (param $p0 i32))
  (func $test/0 (type $t2) (param $p0 i32))
  (export "test/0" (func $test/0))
  (export "is_prime/1" (func $is_prime/1))
  (export "is_prime/2" (func $is_prime/2)))
2025-06-11T15:09:19Z tom@hierba:/tmp/tom-godot
*/

namespace {

using FunctionInfo = GDScriptWasmCompiler::FunctionInfo;
using Self = GDScriptWasmCompiler::Self;

void compile_block(Self &self, GDScriptParser::SuiteNode *p_block);
void compile_expression(Self &self, const GDScriptParser::ExpressionNode *p_expression);

uint8_t convert_type(Self &self, Variant::Type p_type) {
	switch (p_type) {
		case Variant::FLOAT:
			return self.cg.f64;
		case Variant::INT:
			return self.cg.i64;
		default:
			// Presumably a bool, char, or else handle of some sort.
			return self.cg.i32;
	}
}

void compile_binary(Self &self, const GDScriptParser::BinaryOpNode *p_binary) {
	compile_expression(self, p_binary->left_operand);
	compile_expression(self, p_binary->right_operand);
	switch (p_binary->operation) {
		case GDScriptParser::BinaryOpNode::OP_ADDITION: {
			switch (p_binary->datatype.builtin_type) {
				// case Variant::FLOAT: {
				// 	self.cg.f64.add();
				// } break;
				// case Variant::INT: {
				// 	self.cg.i64.add();
				// } break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					if (self.dump_wasm) {
						print_line("=== OP_ADDITION:", p_binary->datatype.builtin_type);
					}
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_COMP_EQUAL: {
			switch (p_binary->datatype.builtin_type) {
				// case Variant::FLOAT: {
				// 	self.cg.f64.eq();
				// } break;
				// case Variant::INT: {
				// 	self.cg.i64.eq();
				// } break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					if (self.dump_wasm) {
						print_line("=== OP_COMP_EQUAL:", p_binary->datatype.builtin_type);
					}
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_COMP_GREATER: {
			switch (p_binary->datatype.builtin_type) {
				// case Variant::FLOAT: {
				// 	self.cg.f64.gt();
				// } break;
				// case Variant::INT: {
				// 	self.cg.i64.gt_s();
				// } break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					if (self.dump_wasm) {
						print_line("=== OP_COMP_GREATER:", p_binary->datatype.builtin_type);
					}
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_COMP_LESS_EQUAL: {
			switch (p_binary->datatype.builtin_type) {
				// case Variant::FLOAT: {
				// 	self.cg.f64.le();
				// } break;
				// case Variant::INT: {
				// 	self.cg.i64.le_s();
				// } break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					if (self.dump_wasm) {
						print_line("=== OP_COMP_LESS_EQUAL:", p_binary->datatype.builtin_type);
					}
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_MODULO: {
			switch (p_binary->datatype.builtin_type) {
				// case Variant::FLOAT: {
				// 	// self.cg.f64.rem();
				// } break;
				// case Variant::INT: {
				// 	self.cg.i64.rem_s();
				// } break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					if (self.dump_wasm) {
						print_line("=== OP_MODULO:", p_binary->datatype.builtin_type);
					}
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_MULTIPLICATION: {
			switch (p_binary->datatype.builtin_type) {
				// case Variant::FLOAT: {
				// 	self.cg.f64.mul();
				// } break;
				// case Variant::INT: {
				// 	self.cg.i64.mul();
				// } break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					if (self.dump_wasm) {
						print_line("=== OP_MULTIPLICATION:", p_binary->datatype.builtin_type);
					}
				} break;
			}
		} break;
		default: {
			if (self.dump_wasm) {
				print_line("=== p_binary->operation:", p_binary->operation);
			}
		} break;
	}
}

void compile_call(Self &self, const GDScriptParser::CallNode *p_call) {
	compile_expression(self, p_call->callee);
	for (int i = 0; i < p_call->arguments.size(); i += 1) {
		compile_expression(self, p_call->arguments[i]);
	}
}

void compile_identifier(Self &self, const GDScriptParser::IdentifierNode *p_identifier) {
	if (self.dump_wasm) {
		print_line("=== compile_identifier:", p_identifier->name, p_identifier->source);
	}
	switch (p_identifier->source) {
		case GDScriptParser::IdentifierNode::FUNCTION_PARAMETER: {
			for (int i = 0; i < self.scopes.size(); i += 1) {
				const GDScriptParser::Node *scope = self.scopes[i];
				if (scope->type == GDScriptParser::Node::FUNCTION) {
					const GDScriptParser::FunctionNode *fun = static_cast<const GDScriptParser::FunctionNode *>(scope);
					for (int j = 0; j < fun->parameters.size(); j += 1) {
						const GDScriptParser::ParameterNode *parameter = fun->parameters[j];
						if (parameter->identifier->name == p_identifier->name) {
							if (self.dump_wasm) {
								// TODO Need some way to correlate this back to wasm things.
								print_line("=== found parameter:", i, j);
								goto PARAMETER_FOUND;
							}
						}
					}
				}
			}
		PARAMETER_FOUND:;
		} break;
		case GDScriptParser::IdentifierNode::UNDEFINED_SOURCE: {
			FunctionInfo *fun = self.functions.getptr(p_identifier->name);
			if (fun) {
				if (self.dump_wasm) {
					print_line("=== found fun:", fun->node, fun->wasm_id);
				}
			} else {
				//
			}
		} break;
		default: {
			if (self.dump_wasm) {
				// print_line("=== compile_identifier");
				// print_line(p_identifier->name);
				// print_line(p_identifier->source);
				// print_line("=== /compile_identifier");
			}
		} break;
	}
}

void compile_expression(Self &self, const GDScriptParser::ExpressionNode *p_expression) {
	switch (p_expression->type) {
		case GDScriptParser::Node::BINARY_OPERATOR: {
			compile_binary(self, static_cast<const GDScriptParser::BinaryOpNode *>(p_expression));
			// op->left_operand
		} break;
		case GDScriptParser::Node::CALL: {
			compile_call(self, static_cast<const GDScriptParser::CallNode *>(p_expression));
		} break;
		case GDScriptParser::Node::IDENTIFIER: {
			compile_identifier(self, static_cast<const GDScriptParser::IdentifierNode *>(p_expression));
		} break;
		case GDScriptParser::Node::LITERAL: {
			const GDScriptParser::LiteralNode *literal = static_cast<const GDScriptParser::LiteralNode *>(p_expression);
			// TODO literal->value
		} break;
		default: {
			if (self.dump_wasm) {
				print_line("=== compile_expression:", p_expression->type);
			}
		} break;
	}
}

void compile_if(Self &self, const GDScriptParser::IfNode *p_if) {
	compile_expression(self, p_if->condition);
	self.cg.if_(self.cg.void_);
	compile_block(self, p_if->true_block);
	if (p_if->false_block) {
		self.cg.else_();
		compile_block(self, p_if->false_block);
	}
	self.cg.end();
}

void compile_block(Self &self, GDScriptParser::SuiteNode *p_block) {
	for (int i = 0; i < p_block->statements.size(); i++) {
		const GDScriptParser::Node *statement = p_block->statements[i];
		switch (statement->type) {
			case GDScriptParser::Node::IF: {
				compile_if(self, static_cast<const GDScriptParser::IfNode *>(statement));
			} break;
			case GDScriptParser::Node::RETURN: {
				const GDScriptParser::ReturnNode *return_node = static_cast<const GDScriptParser::ReturnNode *>(statement);
				compile_expression(self, return_node->return_value);
				self.cg.return_();
			} break;
			default: {
				if (statement->is_expression()) {
					compile_expression(self, static_cast<const GDScriptParser::ExpressionNode *>(statement));
				} else if (self.dump_wasm) {
					print_line("=== statement:", statement->type);
				}
			} break;
		}
	}
}

void compile_function(Self &self, const GDScriptParser::FunctionNode *p_func) {
	StringName name = p_func->identifier->name;
	CharString utf8 = String(name).utf8();
	// Make overloads for optional params.
	bool first = true;
	for (int i = p_func->parameters.size(); i >= 0; i--) {
		// TODO Also need a Variant-friendly wrapper for each function?
		// TODO Or can we store metadata to know how to use each function without that?
		GDScriptParser::ExpressionNode *initializer = i ? p_func->parameters[i - 1]->initializer : nullptr;
		std::vector<uint8_t> wasm_params;
		if (!p_func->is_static) {
			// Self parameter.
			wasm_params.push_back(self.cg.i32);
		}
		for (int j = 0; j < i; j += 1) {
			GDScriptParser::ParameterNode *parameter = p_func->parameters[j];
			wasm_params.push_back(convert_type(self, parameter->datatype.builtin_type));
		}
		std::vector<uint8_t> wasm_return_type;
		if (p_func->return_type) {
			wasm_return_type.push_back(convert_type(self, p_func->return_type->datatype.builtin_type));
		}
		// Emit happens late, so capture by value anything that won't live until emit time.
		uint32_t fun = self.cg.function(wasm_params, wasm_return_type, [&self, p_func, first]() {
			// Track scope.
			// TODO Helper to manage push and pop of scopes.
			// TODO Track info also for lambdas, which need separately generated.
			self.scopes.push_back(p_func);
			// Fill body.
			if (first) {
				// Full function.
				compile_block(self, p_func->body);
			} else {
				// TODO Call previous with default value.
			}
			// Pop context.
			self.scopes.remove_at(self.scopes.size() - 1);
		});
		if (first) {
			// We also insert in order, so we can calculate fun value for overloads.
			// Each function that gets added to cg increments by 1.
			// Just need to check if the overload is valid before calculating the reference.
			self.functions.insert(name, { .node = p_func, .wasm_id = fun });
		}
		// if (self.dump_wasm) {
		// 	print_line("=== funs has ===");
		// 	print_line(name);
		// 	print_line(fun);
		// 	print_line("=== /funs has ===");
		// }
		first = false;
		// -1 to exclude null char.
		std::string utf8_string{ utf8.ptr(), static_cast<size_t>(utf8.size()) - 1 };
		// TODO Also handle varargs cases.
		utf8_string += '/';
		utf8_string.append(std::to_string(i));
		self.cg.export_(fun, utf8_string);
		if (!initializer) {
			// That was the last.
			break;
		}
	}
}

Error compile_class(Self &self, GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state) {
	for (int i = 0; i < p_class->members.size(); i++) {
		const GDScriptParser::ClassNode::Member &member = p_class->members[i];
		if (member.type == member.CONSTANT) {
			const GDScriptParser::ConstantNode *constant = member.constant;
			if (constant->identifier->name == "DUMP_WASM") {
				// print_line("DUMP_WASM");
				// print_line(constant->datatype.to_string());
				if (constant->initializer) {
					// print_line(constant->initializer->reduced_value);
					self.dump_wasm = constant->initializer->reduced_value && bool(constant->initializer->reduced_value);
				}
				break;
			}
		}
	}
	for (int i = 0; i < p_class->members.size(); i++) {
		const GDScriptParser::ClassNode::Member &member = p_class->members[i];
		if (member.type == member.FUNCTION) {
			const GDScriptParser::FunctionNode *function = member.function;
			Error err = OK;
			compile_function(self, function);
			if (err) {
				return err;
			}
		} else if (member.type == member.VARIABLE) {
			const GDScriptParser::VariableNode *variable = member.variable;
			if (variable->property == GDScriptParser::VariableNode::PROP_INLINE) {
				if (variable->setter != nullptr) {
					// Error err = _parse_setter_getter(p_script, p_class, variable, true);
					// if (err) {
					// 	return err;
					// }
				}
				if (variable->getter != nullptr) {
					// Error err = _parse_setter_getter(p_script, p_class, variable, false);
					// if (err) {
					// 	return err;
					// }
				}
			}
		}
	}
	return OK;
}

} //namespace

Error GDScriptWasmCompiler::compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state) {
	Error err = OK;
	const GDScriptParser *parser = p_parser;
	const GDScriptParser::ClassNode *root = parser->get_tree();
	compile_class(self, p_script, root, p_keep_state);
	// GDScriptParser::TreePrinter printer;
	// printer.print_tree(*p_parser);
	String tmp_dir = "/tmp/tom-godot";
	String tmp_file = tmp_dir.path_join(p_script->get_path().get_file());
	tmp_file.append_ascii(".wasm");
	DirAccess::make_dir_recursive_absolute(tmp_dir);
	Ref<FileAccess> file = FileAccess::open(tmp_file, FileAccess::WRITE);
	std::vector<uint8_t> wasm = self.cg.emit();
	file->store_buffer(wasm.data(), wasm.size());
	return err;
}

GDScriptWasmCompiler::GDScriptWasmCompiler() {
}
