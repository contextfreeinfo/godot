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

GDScriptWasmFunction *compile_function(GDScriptWasmCompilerSelf &self, Error &r_error, GDScript *p_script, const GDScriptParser::ClassNode *p_class, const GDScriptParser::FunctionNode *p_func, bool p_for_ready = false, bool p_for_lambda = false) {
	String name = p_func->identifier->name;
	CharString utf8 = name.utf8();
	// if (dump_wasm) {
	// 	print_line("compiling function");
	// 	print_line(name);
	// 	for (int i = 0; i < p_func->parameters.size(); i++) {
	// 		print_line("parameter");
	// 		const GDScriptParser::ParameterNode *parameter = p_func->parameters[i];
	// 		print_line(parameter->identifier->name);
	// 		print_line(parameter->datatype.to_string());
	// 		if (parameter->initializer) {
	// 			print_line(parameter->initializer->reduced_value);
	// 		}
	// 		print_line("/parameter");
	// 	}
	// 	print_line("/compiling function");
	// }
	// Make overloads for optional params.
	bool first = true;
	for (int i = p_func->parameters.size(); i >= 0; i--) {
		// TODO Also need a Variant-friendly wrapper for each function?
		// TODO Or can we store metadata to know how to use each function without that?
		GDScriptParser::ExpressionNode *initializer = i ? p_func->parameters[i - 1]->initializer : nullptr;
		std::vector<uint8_t> wasm_params;
		for (int j = 0; j < i; j += 1) {
			GDScriptParser::ParameterNode *parameter = p_func->parameters[j];
			switch (parameter->datatype.builtin_type) {
				case Variant::FLOAT: {
					wasm_params.push_back(self.cg.f64);
				} break;
				case Variant::INT: {
					wasm_params.push_back(self.cg.i64);
				} break;
				default: {
					// Presumably a bool, char, or else handle of some sort.
					wasm_params.push_back(self.cg.i32);
				} break;
			}
		}
		uint32_t fun = self.cg.function(wasm_params, {}, [&]() {
			if (first) {
				// TODO Full function.
				// p_func->body
			} else {
				// TODO Call previous with default value.
			}
			// TODO Fill function content.
		});
		first = false;
		// -1 to exclude null char.
		std::string utf8_string{ utf8.ptr(), static_cast<size_t>(utf8.size()) - 1 };
		// TODO Repeat for optional params. Name /0, /1, ...
		utf8_string += '/';
		utf8_string.append(std::to_string(i));
		self.cg.export_(fun, utf8_string);
		if (!initializer) {
			// That was the last.
			break;
		}
	}
	return nullptr;
}

Error compile_class(GDScriptWasmCompilerSelf &self, GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state) {
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
			compile_function(self, err, p_script, p_class, function);
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
