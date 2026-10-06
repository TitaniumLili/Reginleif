/**************************************************************************/
/*  gdscript_optimiser.h                                                  */
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
/// Hey! This file was created for the Reginleif fork! It's not part of Godot upstream.
/// Copyright (c) 2026 chesedcore (Monarch).
/// Licensed under the MIT License, same terms as the Godot engine.

#pragma once

#include "gdscript_parser.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"

class GDScriptOptimiser {
public:
	struct VarLifetime {
		int last_read_ip = -1;
		int last_write_ip = -1;
	};

	struct FreedSlot {
		StringName owner_name; ///kept around for debugging
		uint32_t address = 0;
		int freed_at_ip = -1;
		uint64_t inline_generation = 0;
	};

	///made so that across an if/else/elif branch, you don't waste the max amount of stack slots possible
	///and instead use a pool that gives you the minimum stack slots needed for the op
	struct SiblingSlotPool {
		List<FreedSlot> free_slots;
		uint64_t inline_generation = 0;
	};

	struct SlotDecision {
		bool reused = false;
		uint32_t existing_stack_pos = 0;
	};

	static void record_use(HashMap<const GDScriptParser::Node*, VarLifetime>& r_lifetimes, const GDScriptParser::Node* p_key, int p_ip, bool p_is_write);

	/// "can a local thingy use a slot declared at, or freed before, p_current_ip?"
	static SlotDecision try_reuse_slot(SiblingSlotPool& r_pool, int p_current_ip, bool p_eligible_for_reuse, uint32_t p_stack_floor = UINT32_MAX, uint32_t p_locals_ceiling = UINT32_MAX);
	static void register_freed_slot(SiblingSlotPool& r_pool, const StringName& p_name, uint32_t p_address, int p_freed_at_ip);
	///the range registered is half open [)
	static void register_freed_slot_range(SiblingSlotPool& r_pool, uint32_t p_base, uint32_t p_count, int p_freed_at_ip);

	static uint64_t new_inline_generation();

private:
	static uint64_t _generation_counter;
};