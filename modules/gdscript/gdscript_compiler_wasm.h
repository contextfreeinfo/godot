/**************************************************************************/
/*  gdscript_compiler_wasm.h                                              */
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

#pragma once

#include "gdscript.h"
#include "gdscript_parser.h"

#include <wasmblr.h>

class GDScriptWasmCompiler {
public:
	enum Type {
		I32,
		I64,
		F64,
		COUNT,
		FUNCTION,
	};

	struct Decl {
		uint32_t id;
		Type type;
	};

	struct Function {
		uint32_t id;
		const GDScriptParser::FunctionNode *node;
	};

	struct LocalGroup {
		Type active_type = Type::COUNT;
		uint32_t ids[Type::COUNT] = { 0 };
	};

	// struct Scope {
	// 	List<HashMap<StringName, uint32_t>> decls;
	// 	uint32_t local_counts[Type::COUNT];
	// };

	struct Self {
		wasmblr::CodeGenerator cg;
		bool dump_wasm = false;
		HashMap<StringName, Function> functions;
		// Reuse decls for any with the same name and type.
		// It's illegal to have two locals in the same scope with the same name.
		// If multiple with same name, export with a `$type` suffix?
		HashMap<StringName, LocalGroup> locals;
		// TODO For debug info?: Vector<StringName> local_names;
		Vector<const GDScriptParser::Node *> scopes;
		// List<Scope> scops;
	};

private:
	Self self;

public:
	Error compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state = false);

	GDScriptWasmCompiler();
};
