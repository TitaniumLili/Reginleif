/**************************************************************************/
/*  gdscript_optimiser.cpp                                                */
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

#include "gdscript_optimiser.h"
#include "core/config/project_settings.h"

uint64_t GDScriptOptimiser::_generation_counter = 0;

void GDScriptOptimiser::record_use(HashMap<const GDScriptParser::Node*, VarLifetime>& r_lifetimes, const GDScriptParser::Node* p_key, int p_ip, bool p_is_write) {
	if (p_key == nullptr) {
		return;
	}

	VarLifetime& lt = r_lifetimes[p_key];
	if (p_is_write) {
		if (p_ip > lt.last_write_ip) {
			lt.last_write_ip = p_ip;
		}
	} else {
		if (p_ip > lt.last_read_ip) {
			lt.last_read_ip = p_ip;
		}
	}
}

///chokepoint for the entire SSR (stack slot reuse) feature!
GDScriptOptimiser::SlotDecision GDScriptOptimiser::try_reuse_slot(SiblingSlotPool& r_pool, int p_current_ip, bool p_eligible_for_reuse, uint32_t p_stack_floor, uint32_t p_locals_ceiling) {
	SlotDecision decision;

	static bool ssr_enabled_cached = false;
	static bool ssr_enabled_resolved = false;
	if (!ssr_enabled_resolved) {
		ssr_enabled_cached = GLOBAL_DEF("reginleif/optimisations/enable_stack_slot_reuse", true);
		ssr_enabled_resolved = true;
	}
	if (!ssr_enabled_cached) {
		return decision;
	}

	if (!p_eligible_for_reuse) {
		return decision;
	}

	List<FreedSlot>::Element* E = r_pool.free_slots.front();
	while (E != nullptr) {
		List<FreedSlot>::Element* next = E->next();
		bool below_floor = (p_stack_floor != UINT32_MAX) && (E->get().address < p_stack_floor);
		bool above_ceiling = (p_locals_ceiling != UINT32_MAX) && (E->get().address >= p_locals_ceiling);
		bool wrong_generation = (E->get().inline_generation != r_pool.inline_generation);
		if (below_floor || above_ceiling || wrong_generation) {
			r_pool.free_slots.erase(E);
		}
		E = next;
	}

	List<FreedSlot>::Element* best = nullptr;
	for (List<FreedSlot>::Element* F = r_pool.free_slots.front(); F != nullptr; F = F->next()) {
		if (F->get().freed_at_ip >= p_current_ip) {
			continue; ///still live
		}
		if (best == nullptr || F->get().address < best->get().address) {
			best = F;
		}
	}

	if (best == nullptr) {
		return decision;
	}

	decision.reused = true;
	decision.existing_stack_pos = best->get().address;
	r_pool.free_slots.erase(best);
	return decision;
}

void GDScriptOptimiser::register_freed_slot(SiblingSlotPool& r_pool, const StringName& p_name, uint32_t p_address, int p_freed_at_ip) {
	FreedSlot slot;
	slot.owner_name = p_name;
	slot.address = p_address;
	slot.freed_at_ip = p_freed_at_ip;
	slot.inline_generation = r_pool.inline_generation;
	r_pool.free_slots.push_back(slot);
}

void GDScriptOptimiser::register_freed_slot_range(SiblingSlotPool& r_pool, uint32_t p_base, uint32_t p_count, int p_freed_at_ip) {
	for (uint32_t i = 0; i < p_count; i++) {
		register_freed_slot(r_pool, StringName(), p_base + i, p_freed_at_ip);
	}
}

uint64_t GDScriptOptimiser::new_inline_generation() {
	return ++_generation_counter;
}
