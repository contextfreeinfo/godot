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

using Function = GDScriptWasmCompiler::Function;
using LocalGroup = GDScriptWasmCompiler::LocalGroup;
using Self = GDScriptWasmCompiler::Self;
using WasmType = GDScriptWasmCompiler::Type;

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

WasmType convert_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::FLOAT:
			return GDScriptWasmCompiler::F64;
		case Variant::INT:
			return GDScriptWasmCompiler::I64;
		default:
			// Presumably a bool, char, or else handle of some sort.
			return GDScriptWasmCompiler::I32;
	}
}

// uint8_t convert_type(Self &self, WasmType p_type) {
// 	switch (p_type) {
// 		case GDScriptWasmCompiler::F64:
// 			return self.cg.f64;
// 		case GDScriptWasmCompiler::I64:
// 			return self.cg.i64;
// 		default:
// 			// Presumably a bool, char, or else handle of some sort.
// 			return self.cg.i32;
// 	}
// }

void ensure_local(Self &self, const StringName &name, WasmType type) {
	LocalGroup *locals = self.locals.getptr(name);
	DEV_ASSERT(type < WasmType::COUNT);
	// This can leave invalid active_type when exiting a local block, but that
	// should be fine if there are no static errors in the gdscript code.
	if (locals) {
		if (locals->ids[type] == UINT32_MAX) {
			locals->ids[type] = self.local_count;
			self.local_count += 1;
		}
		locals->active_type = type;
	} else {
		self.locals[name] = { type, self.local_count };
		self.local_count += 1;
	}
}

void ensure_local(Self &self, const GDScriptParser::AssignableNode *node) {
	ensure_local(self, node->identifier->name, convert_type(node->datatype.builtin_type));
}

uint32_t get_local(Self &self, const StringName &name) {
	LocalGroup *locals = self.locals.getptr(name);
	return locals ? locals->ids[locals->active_type] : UINT32_MAX;
}

// WasmType get_local_type(Self &self, const StringName &name) {
// 	LocalGroup *locals = self.locals.getptr(name);
// 	return locals ? locals->active_type : WasmType::COUNT;
// }

void compile_binary(Self &self, const GDScriptParser::BinaryOpNode *p_binary) {
	compile_expression(self, p_binary->left_operand);
	compile_expression(self, p_binary->right_operand);
	// if (self.dump_wasm) {
	// 	print_line("=== compile_binary:", p_binary->operation);
	// }
	WasmType type = convert_type(p_binary->left_operand->datatype.builtin_type);
	switch (p_binary->operation) {
		case GDScriptParser::BinaryOpNode::OP_ADDITION: {
			// if (self.dump_wasm) {
			// 	print_line("=== OP_ADDITION:", type);
			// }
			switch (type) {
				case WasmType::F64: {
					self.cg.f64.add();
				} break;
				case WasmType::I64: {
					self.cg.i64.add();
				} break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					// if (self.dump_wasm) {
					// 	print_line("=== OP_ADDITION:", p_binary->datatype.builtin_type);
					// }
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_COMP_EQUAL: {
			// if (self.dump_wasm) {
			// 	print_line("=== OP_COMP_EQUAL:", type);
			// }
			switch (type) {
				case WasmType::F64: {
					self.cg.f64.eq();
				} break;
				case WasmType::I64: {
					self.cg.i64.eq();
				} break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_COMP_GREATER: {
			// if (self.dump_wasm) {
			// 	print_line("=== OP_COMP_GREATER:", type);
			// }
			switch (type) {
				case WasmType::F64: {
					self.cg.f64.gt();
				} break;
				case WasmType::I64: {
					self.cg.i64.gt_s();
				} break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_COMP_LESS_EQUAL: {
			// if (self.dump_wasm) {
			// 	print_line("=== OP_COMP_LESS_EQUAL:", type);
			// }
			switch (type) {
				case WasmType::F64: {
					self.cg.f64.le();
				} break;
				case WasmType::I64: {
					self.cg.i64.le_s();
				} break;
				default: {
					if (self.dump_wasm) {
						print_line("!!! OP_COMP_LESS_EQUAL:", type);
					}
					// Presumably a bool, char, or else handle of some sort.
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_MODULO: {
			// if (self.dump_wasm) {
			// 	print_line("=== OP_MODULO:", type);
			// }
			switch (type) {
				case WasmType::F64: {
					// self.cg.f64.rem();
				} break;
				case WasmType::I64: {
					self.cg.i64.rem_s();
				} break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
				} break;
			}
		} break;
		case GDScriptParser::BinaryOpNode::OP_MULTIPLICATION: {
			// if (self.dump_wasm) {
			// 	print_line("=== OP_MULTIPLICATION:", type);
			// }
			switch (type) {
				case WasmType::F64: {
					self.cg.f64.mul();
				} break;
				case WasmType::I64: {
					self.cg.i64.mul();
				} break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
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
	Function *fun = nullptr;
	if (p_call->callee->type == GDScriptParser::Node::IDENTIFIER) {
		const GDScriptParser::IdentifierNode *identifier = static_cast<const GDScriptParser::IdentifierNode *>(p_call->callee);
		// print_line("=== compile_call:", identifier->name);
		if (identifier->source == GDScriptParser::IdentifierNode::UNDEFINED_SOURCE) {
			// See "Self function call" in gdscript_compiler.
			fun = self.functions.getptr(identifier->name);
			if (fun) {
				if (p_call->is_static) {
					// TODO Pass static instance?
				} else {
					// Pass self instance.
					self.cg.local.get(0);
				}
			}
		}
	}
	for (int i = 0; i < p_call->arguments.size(); i += 1) {
		compile_expression(self, p_call->arguments[i]);
	}
	if (fun) {
		uint32_t gap = 0;
		if (p_call->arguments.size() < fun->node->parameters.size()) {
			// TODO Verify that we don't skip past optionals gap.
			gap = static_cast<uint32_t>(fun->node->parameters.size() - p_call->arguments.size());
		}
		self.cg.call(fun->id + gap);
	} else {
		// TODO Descriptions for imported functions?
		// compile_expression(self, p_call->callee);
		for (int i = 0; i < p_call->arguments.size(); i += 1) {
			// Drop for now until we can print.
			self.cg.emit(0x1a);
		}
	}
}

void compile_identifier(Self &self, const GDScriptParser::IdentifierNode *p_identifier) {
	// if (self.dump_wasm) {
	// 	print_line("=== compile_identifier:", p_identifier->name, p_identifier->source);
	// }
	switch (p_identifier->source) {
		case GDScriptParser::IdentifierNode::FUNCTION_PARAMETER: {
			uint32_t id = get_local(self, p_identifier->name);
			if (id < UINT32_MAX) {
				self.cg.local.get(id);
				// if (self.dump_wasm) {
				// 	print_line("=== found local:", id, self.cg.input_types().size());
				// }
			}
		} break;
		case GDScriptParser::IdentifierNode::UNDEFINED_SOURCE: {
			// TODO Can this handle table entry ids?
			// Function *fun = self.functions.getptr(p_identifier->name);
			// if (fun) {
			// 	self.cg.call(fun->id);
			// 	// if (self.dump_wasm) {
			// 	// 	print_line("=== found fun:", fun->node, fun->id);
			// 	// }
			// } else {
			// 	//
			// }
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

void compile_literal(Self &self, const GDScriptParser::LiteralNode *literal) {
	switch (literal->value.get_type()) {
		case Variant::BOOL: {
			self.cg.i32.const_(literal->value);
		} break;
		case Variant::FLOAT: {
			self.cg.f64.const_(literal->value);
		} break;
		case Variant::INT: {
			self.cg.i64.const_(literal->value);
		} break;
		default: {
			if (self.dump_wasm) {
				print_line("=== compile_literal:", literal->value.get_type());
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
			compile_literal(self, static_cast<const GDScriptParser::LiteralNode *>(p_expression));
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
	// Process function.
	StringName name = p_func->identifier->name;
	CharString utf8 = String(name).utf8();
	// Make overloads for optional params.
	bool first = true;
	uint32_t prev_fun = 0;
	GDScriptParser::ExpressionNode *prev_initializer = nullptr;
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
		uint32_t fun = self.cg.function(wasm_params, wasm_return_type, [&self, p_func, first, i, prev_fun, prev_initializer]() {
			// if (self.dump_wasm) {
			// 	print_line("=== compile_function:", i, self.cg.cur_function_);
			// 	print_line("                     ", self.cg.locals().size());
			// 	print_line("                     ", self.cg.input_types().size());
			// }
			// Track scope.
			self.local_count = 0;
			self.locals.clear();
			if (!p_func->is_static) {
				// self.locals["self"] = { GDScriptWasmCompiler::I32, self.locals.size() };
				ensure_local(self, "self", WasmType::I32);
				// self.cg.declare_local(convert_type(self, GDScriptWasmCompiler::I32));
			}
			for (int j = 0; j < i; j += 1) {
				GDScriptParser::ParameterNode *parameter = p_func->parameters[j];
				ensure_local(self, parameter);
				// self.locals[parameter->identifier->name] = { convert_type(parameter->datatype.builtin_type), self.locals.size() };
				// self.cg.declare_local(convert_type(self, group.active_type));
			}
			// TODO Track info also for lambdas, which need separately generated.
			// Fill body.
			if (first) {
				// Full function.
				compile_block(self, p_func->body);
			} else {
				// Call previous with default value.
				if (!p_func->is_static) {
					self.cg.local.get(0);
				}
				for (int j = 0; j < i; j += 1) {
					// if (self.dump_wasm) {
					// 	print_line("=== get:", j + 1, self.cg.input_types().size());
					// 	// std::cerr << "=== get: " << self.cg.input_types().size() << " " << (j + 1) << std::endl;
					// 	// std::cerr << "=== get: " << (j + 1) << std::endl;
					// }
					self.cg.local.get(j + 1);
				}
				compile_expression(self, prev_initializer);
				self.cg.call(prev_fun);
			}
		});
		if (first) {
			// We also insert in order, so we can calculate fun value for overloads.
			// Each function that gets added to cg increments by 1.
			// Just need to check if the overload is valid before calculating the reference.
			self.functions.insert(name, { .id = fun, .node = p_func });
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
		prev_fun = fun;
		prev_initializer = initializer;
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
	if (!self.dump_wasm) {
		return OK;
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

void compile_imports(Self &self, GDScript *p_script, const GDScriptParser::ClassNode *p_class) {
	// TODO Either a prepass or make cg more flexible for defining imports as we go.
	self.imports["print_bool"] = self.cg.import_("godot", "print_bool", { self.cg.i32 }, {});
	self.imports["print_int"] = self.cg.import_("godot", "print_int", { self.cg.i64 }, {});
}

} //namespace

Error GDScriptWasmCompiler::compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state) {
	Error err = OK;
	const GDScriptParser *parser = p_parser;
	const GDScriptParser::ClassNode *root = parser->get_tree();
	compile_imports(self, p_script, root);
	compile_class(self, p_script, root, p_keep_state);
	if (!self.dump_wasm) {
		return OK;
	}
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
