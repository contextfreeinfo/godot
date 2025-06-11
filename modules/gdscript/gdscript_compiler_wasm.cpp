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

GDScriptWasmFunction *GDScriptWasmCompiler::_compile_function(Error &r_error, GDScript *p_script, const GDScriptParser::ClassNode *p_class, const GDScriptParser::FunctionNode *p_func, bool p_for_ready, bool p_for_lambda) {
	String name = p_func->identifier->name;
	CharString utf8 = name.utf8();
	if (dump_wasm) {
		print_line("compiling function");
		print_line(name);
		for (int i = 0; i < p_func->parameters.size(); i++) {
			print_line("parameter");
			const GDScriptParser::ParameterNode *parameter = p_func->parameters[i];
			print_line(parameter->identifier->name);
			print_line(parameter->datatype.to_string());
			if (parameter->initializer) {
				print_line(parameter->initializer->reduced_value);
			}
			print_line("/parameter");
		}
		print_line("/compiling function");
	}
	// p_func->parameters
	uint32_t fun = cg.function({ cg.i32 }, {}, [&]() {
		//
	});
	// -1 to exclude null char.
	std::string utf8_string{ utf8.ptr(), static_cast<size_t>(utf8.size()) - 1 };
	cg.export_(fun, utf8_string);
	return nullptr;
}

Error GDScriptWasmCompiler::_compile_class(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state) {
	for (int i = 0; i < p_class->members.size(); i++) {
		const GDScriptParser::ClassNode::Member &member = p_class->members[i];
		if (member.type == member.CONSTANT) {
			const GDScriptParser::ConstantNode *constant = member.constant;
			if (constant->identifier->name == "DUMP_WASM") {
				// print_line("DUMP_WASM");
				// print_line(constant->datatype.to_string());
				if (constant->initializer) {
					// print_line(constant->initializer->reduced_value);
					dump_wasm = constant->initializer->reduced_value && bool(constant->initializer->reduced_value);
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
			_compile_function(err, p_script, p_class, function);
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

Error GDScriptWasmCompiler::compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state) {
	Error err = OK;
	const GDScriptParser *parser = p_parser;
	const GDScriptParser::ClassNode *root = parser->get_tree();
	_compile_class(p_script, root, p_keep_state);
	// GDScriptParser::TreePrinter printer;
	// printer.print_tree(*p_parser);
	String tmp_dir = "/tmp/tom-godot";
	String tmp_file = tmp_dir.path_join(p_script->get_path().get_file());
	tmp_file.append_ascii(".wasm");
	DirAccess::make_dir_recursive_absolute(tmp_dir);
	Ref<FileAccess> file = FileAccess::open(tmp_file, FileAccess::WRITE);
	std::vector<uint8_t> wasm = cg.emit();
	file->store_buffer(wasm.data(), wasm.size());
	return err;
}

GDScriptWasmCompiler::GDScriptWasmCompiler() {
}
