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

	// This is wasteful, but it avoids multiple HashMap lookups, and the number
	// of local vars is likely to be small enough that memory usage matters
	// less.
	struct LocalGroup {
		Type active_type;
		uint32_t ids[Type::COUNT];
		LocalGroup() :
				active_type(Type::COUNT) {
			std::fill(std::begin(ids), std::end(ids), UINT32_MAX);
		}
		LocalGroup(Type p_active_type, uint32_t p_id) :
				LocalGroup() {
			active_type = p_active_type;
			ids[p_active_type] = p_id;
		}
	};

	struct Self {
		wasmblr::CodeGenerator cg;
		bool dump_wasm = false;
		HashMap<StringName, Function> functions;
		HashMap<StringName, uint32_t> imports;
		uint32_t local_count;
		// Reuse decls for any with the same name and type.
		// It's illegal to have two locals in the same scope with the same name.
		HashMap<StringName, LocalGroup> locals;
		// If multiple with same name, export with a `$type` suffix?
		// TODO For debug info?: Vector<StringName> local_names;
	};

private:
	Self self;

public:
	Error compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state = false);

	GDScriptWasmCompiler();
};
