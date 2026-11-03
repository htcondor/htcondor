/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

// TEST: DagArc/Edge/EdgeTable (condor_dagman/edge.h, edge.cpp) -- the
// non-template edge-table layer that dag.hpp's Dag<D, N>::Connect() is
// built on. Exercised standalone here, without going through Connect() at all.
//
// Not covered: the ASSERT()/EXCEPT() invariant checks (e.g. AddArc(NO_ID),
// GetArc() on a missing id, GetEdge(0)) -- unlike dag.hpp, edge.h/edge.cpp
// still depend on condor_common.h/condor_debug.h and abort the process on
// those, so there's nothing a unit test could catch.

#include "condor_common.h"
#include "condor_debug.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "edge.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

//------------------------------------------------------------------------------------
static bool test_dagarc_default_construction() {
	DagArc arc;

	emit_test("Test DagArc's default constructor sets id=NO_ID, metadata=0, IsWeak()=false");
	emit_input_header();
	emit_param("Construction", "DagArc()");

	emit_output_expected_header();
	emit_retval("id==NO_ID=TRUE metadata=0 IsWeak=FALSE");

	std::string actual = std::string("id==NO_ID=") + tfstr(arc.id == NO_ID) +
	                      " metadata=" + std::to_string(arc.metadata) +
	                      " IsWeak=" + tfstr(arc.IsWeak());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (arc.id != NO_ID || arc.metadata != 0 || arc.IsWeak()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_dagarc_parametrized_construction_and_is_weak() {
	DagArc weak(5, ARC_WEAK);
	DagArc strong(6, 0);
	DagArc done(7, ARC_DONE); // ARC_DONE alone must not read as weak

	emit_test("Test DagArc(id, meta) stores both fields, and IsWeak() only reflects ARC_WEAK");
	emit_input_header();
	emit_param("Construction", "DagArc(5, ARC_WEAK), DagArc(6, 0), DagArc(7, ARC_DONE)");

	emit_output_expected_header();
	emit_retval("weak.id=5 weak.IsWeak=TRUE strong.IsWeak=FALSE done.IsWeak=FALSE");

	std::string actual = "weak.id=" + std::to_string(weak.id) + " weak.IsWeak=" + tfstr(weak.IsWeak()) +
	                      " strong.IsWeak=" + tfstr(strong.IsWeak()) + " done.IsWeak=" + tfstr(done.IsWeak());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (weak.id != 5 || ! weak.IsWeak()) { FAIL; }
	if (strong.IsWeak()) { FAIL; }
	if (done.IsWeak()) { FAIL; } // ARC_DONE is a distinct bit from ARC_WEAK

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_add_arc_new() {
	Edge edge;
	size_t idx = edge.AddArc(1, 0);

	emit_test("Test Edge::AddArc() on an empty Edge appends and returns index 0");
	emit_input_header();
	emit_param("AddArc", "id=1");

	emit_output_expected_header();
	emit_retval("idx=0 size=1");

	std::string actual = "idx=" + std::to_string(idx) + " size=" + std::to_string(edge.size());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (idx != 0 || edge.size() != 1) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_add_arc_dedupes_existing_id() {
	Edge edge;
	size_t idx1 = edge.AddArc(1);
	size_t idx2 = edge.AddArc(1); // same id again -- must not append a second arc

	emit_test("Test Edge::AddArc() re-declaring an existing id returns the same index without growing the Edge");
	emit_input_header();
	emit_param("AddArc", "id=1, then id=1 again");

	emit_output_expected_header();
	emit_retval("idx1=0 idx2=0 size=1");

	std::string actual = "idx1=" + std::to_string(idx1) + " idx2=" + std::to_string(idx2) +
	                      " size=" + std::to_string(edge.size());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (idx1 != idx2 || edge.size() != 1) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_add_arc_strongest_wins_upgrade() {
	Edge edge;
	edge.AddArc(1, ARC_WEAK);
	bool weak_before = edge.GetArc(1).IsWeak();
	edge.AddArc(1, 0); // strong re-declaration
	bool weak_after = edge.GetArc(1).IsWeak();

	emit_test("Test Edge::AddArc() upgrades an existing weak arc to strong on a strong re-declaration");
	emit_input_header();
	emit_param("AddArc", "id=1 weak, then id=1 strong");

	emit_output_expected_header();
	emit_retval("weak_before=TRUE weak_after=FALSE");

	std::string actual = std::string("weak_before=") + tfstr(weak_before) + " weak_after=" + tfstr(weak_after);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! weak_before || weak_after) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_add_arc_weak_never_downgrades_strong() {
	Edge edge;
	edge.AddArc(1, 0); // strong
	edge.AddArc(1, ARC_WEAK); // weak re-declaration must not downgrade

	emit_test("Test Edge::AddArc() never downgrades an existing strong arc on a weak re-declaration");
	emit_input_header();
	emit_param("AddArc", "id=1 strong, then id=1 weak");

	emit_output_expected_header();
	emit_retval("IsWeak=FALSE");

	std::string actual = std::string("IsWeak=") + tfstr(edge.GetArc(1).IsWeak());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (edge.GetArc(1).IsWeak()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// AppendArc() is AddArc()'s no-dedupe sibling -- used by Connect()'s bulk-update
// fast paths specifically because `parents`/`children` are already deduplicated by
// the caller, so the scan AddArc() does on every call would be pure waste there.
static bool test_edge_append_arc_no_dedupe() {
	Edge edge;
	edge.AppendArc(1);
	edge.AppendArc(1); // same id -- AppendArc() does not scan/dedupe like AddArc() does

	emit_test("Test Edge::AppendArc() does not dedupe, unlike AddArc()");
	emit_input_header();
	emit_param("AppendArc", "id=1, then id=1 again");

	emit_output_expected_header();
	emit_retval("size=2");

	std::string actual = "size=" + std::to_string(edge.size());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (edge.size() != 2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_get_arc_mutable_reflects_in_const_view() {
	Edge edge;
	edge.AddArc(5, ARC_WEAK);

	DagArc& mutable_ref = edge.GetArc(5);
	mutable_ref.metadata &= ~ARC_WEAK; // upgrade by hand through the mutable overload

	const Edge& const_edge = edge;
	const DagArc& const_ref = const_edge.GetArc(5);

	emit_test("Test Edge::GetArc()'s mutable and const overloads see the same arc");
	emit_input_header();
	emit_param("GetArc", "id=5, mutated through the non-const overload");

	emit_output_expected_header();
	emit_retval("const_ref.IsWeak=FALSE");

	std::string actual = std::string("const_ref.IsWeak=") + tfstr(const_ref.IsWeak());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (const_ref.IsWeak()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_contains() {
	Edge edge;
	edge.AddArc(1);

	emit_test("Test Edge::Contains() reports presence by node id");
	emit_input_header();
	emit_param("Edge", "contains id=1 only");

	bool has_1 = edge.Contains(1);
	bool has_2 = edge.Contains(2);

	emit_output_expected_header();
	emit_retval("Contains(1)=TRUE Contains(2)=FALSE");

	std::string actual = std::string("Contains(1)=") + tfstr(has_1) + " Contains(2)=" + tfstr(has_2);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! has_1 || has_2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_operator_brackets_by_id_and_by_index() {
	Edge edge;
	edge.AddArc(7);
	edge.AddArc(8);

	DagArc& by_id = edge[7];             // operator[](node_id_t)
	DagArc& by_index = edge[(size_t)0];  // operator[](size_t) -- raw slot access

	emit_test("Test Edge::operator[] resolves both by node id and by raw index to the same arc");
	emit_input_header();
	emit_param("Edge", "[7, 8], looked up as edge[7] and edge[(size_t)0]");

	emit_output_expected_header();
	emit_retval("by_id.id=7 same_address=TRUE");

	std::string actual = "by_id.id=" + std::to_string(by_id.id) + " same_address=" + tfstr(&by_id == &by_index);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (by_id.id != 7) { FAIL; }
	if (&by_id != &by_index) { FAIL; } // id 7 was added first, so it lives at index 0

	PASS;
}

//------------------------------------------------------------------------------------
// Proves both operator[] const overloads (by node id, by raw index) resolve through
// a const Edge& and still find the right arc.
static bool test_edge_const_operator_brackets() {
	Edge edge;
	edge.AddArc(7);
	edge.AddArc(8);

	const Edge& const_edge = edge;
	const DagArc& by_id = const_edge[7];            // operator[](node_id_t) const
	const DagArc& by_index = const_edge[(size_t)0]; // operator[](size_t) const

	emit_test("Test Edge::operator[] const overloads (by node id and by raw index) resolve through a const Edge&");
	emit_input_header();
	emit_param("Edge", "[7, 8], looked up through a const Edge& as [7] and [(size_t)0]");

	emit_output_expected_header();
	emit_retval("by_id.id=7 same_address=TRUE");

	std::string actual = "by_id.id=" + std::to_string(by_id.id) + " same_address=" + tfstr(&by_id == &by_index);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (by_id.id != 7) { FAIL; }
	if (&by_id != &by_index) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Reserve()'s whole purpose (mirrors Dag<D, N>::Reserve() -- see OTEST_Dag.cpp): let a
// caller hold a DagArc* across a known-size batch of AppendArc() calls without it being
// invalidated by a mid-batch reallocation.
static bool test_edge_reserve_prevents_pointer_invalidation() {
	Edge edge;
	edge.Reserve(3);

	edge.AppendArc(1);
	DagArc* held = &edge[(size_t)0];

	edge.AppendArc(2);
	edge.AppendArc(3);

	emit_test("Test Edge::Reserve() lets a held DagArc* survive a known-size batch of AppendArc() calls");
	emit_input_header();
	emit_param("Reserve", "3, then AppendArc() id=1, hold &edge[0], then AppendArc() id=2, id=3");

	emit_output_expected_header();
	emit_retval("held==&edge[0]=TRUE held->id=1");

	std::string actual = std::string("held==&edge[0]=") + tfstr(held == &edge[(size_t)0]) +
	                      " held->id=" + std::to_string(held->id);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (held != &edge[(size_t)0]) { FAIL; }
	if (held->id != 1) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_ref_counting() {
	Edge edge;
	edge.SetRefCount(5);
	size_t after_set = edge.GetRefCount();

	++edge;
	size_t after_inc = edge.GetRefCount();

	--edge;
	--edge;
	size_t after_dec = edge.GetRefCount();

	emit_test("Test Edge::SetRefCount()/operator++/operator-- track a plain reference count");
	emit_input_header();
	emit_param("RefCount", "SetRefCount(5), ++edge, --edge, --edge");

	emit_output_expected_header();
	emit_retval("after_set=5 after_inc=6 after_dec=4");

	std::string actual = "after_set=" + std::to_string(after_set) + " after_inc=" + std::to_string(after_inc) +
	                      " after_dec=" + std::to_string(after_dec);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (after_set != 5 || after_inc != 6 || after_dec != 4) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_ref_count_floors_at_zero() {
	Edge edge;
	edge.SetRefCount(0);
	--edge; // must not underflow/wrap -- guarded by `if (m_references > 0)`

	emit_test("Test Edge::operator-- does not decrement below zero");
	emit_input_header();
	emit_param("RefCount", "SetRefCount(0), then --edge");

	emit_output_expected_header();
	emit_retval("GetRefCount=0");

	std::string actual = "GetRefCount=" + std::to_string(edge.GetRefCount());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (edge.GetRefCount() != 0) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// MarkDone()/Reset() back wait edges (see EdgeTable::NewWaitEdge()) -- Reset() must be
// called once after all parent arcs are added (via AddArc), initializing m_waiting to
// the parent count; MarkDone() then ticks it down as each parent completes.
static bool test_edge_mark_done_and_waiting_tick_down() {
	Edge wedge;
	wedge.AddArc(10);
	wedge.AddArc(11);
	wedge.Reset(); // m_waiting = 2

	bool waiting_before = wedge.IsWaiting();
	uint32_t count_before = wedge.Waiting();

	bool first_done_result = wedge.MarkDone(10);  // one of two -- not the last
	bool still_waiting = wedge.IsWaiting();
	uint32_t count_mid = wedge.Waiting();

	bool second_done_result = wedge.MarkDone(11); // the last parent
	bool waiting_after = wedge.IsWaiting();

	emit_test("Test Edge::MarkDone()/IsWaiting()/Waiting() tick down as parents complete, true only on the last one");
	emit_input_header();
	emit_param("WaitEdge", "parents [10, 11], MarkDone(10) then MarkDone(11)");

	emit_output_expected_header();
	emit_retval("before(waiting=TRUE,2) MarkDone(10)=FALSE mid(waiting=TRUE,1) MarkDone(11)=TRUE after(waiting=FALSE)");

	std::string actual = std::string("before(waiting=") + tfstr(waiting_before) + "," + std::to_string(count_before) +
	                      ") MarkDone(10)=" + tfstr(first_done_result) +
	                      " mid(waiting=" + tfstr(still_waiting) + "," + std::to_string(count_mid) +
	                      ") MarkDone(11)=" + tfstr(second_done_result) +
	                      " after(waiting=" + tfstr(waiting_after) + ")";

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! waiting_before || count_before != 2) { FAIL; }
	if (first_done_result) { FAIL; }       // not the last parent yet
	if ( ! still_waiting || count_mid != 1) { FAIL; }
	if ( ! second_done_result) { FAIL; }    // this WAS the last parent
	if (waiting_after) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_mark_done_twice_on_same_parent_is_a_no_op() {
	Edge wedge;
	wedge.AddArc(10);
	wedge.AddArc(11);
	wedge.Reset();

	std::ignore = wedge.MarkDone(10);
	uint32_t after_first = wedge.Waiting();
	bool second_call_result = wedge.MarkDone(10); // already done -- must not decrement again
	uint32_t after_second = wedge.Waiting();

	emit_test("Test Edge::MarkDone() re-marking an already-done parent doesn't double-decrement m_waiting");
	emit_input_header();
	emit_param("WaitEdge", "parents [10, 11], MarkDone(10) called twice");

	emit_output_expected_header();
	emit_retval("after_first=1 second_call=FALSE after_second=1");

	std::string actual = "after_first=" + std::to_string(after_first) +
	                      " second_call=" + tfstr(second_call_result) +
	                      " after_second=" + std::to_string(after_second);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (after_first != 1) { FAIL; }
	if (second_call_result) { FAIL; } // no new completion event
	if (after_second != 1) { FAIL; }  // unchanged, not decremented again

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_mark_done_unknown_parent_is_a_no_op() {
	Edge wedge;
	wedge.AddArc(10);
	wedge.Reset();

	bool result = wedge.MarkDone(999); // never added as a parent

	emit_test("Test Edge::MarkDone() returns false and leaves state untouched for an id that isn't a parent");
	emit_input_header();
	emit_param("WaitEdge", "parents [10], MarkDone(999)");

	emit_output_expected_header();
	emit_retval("result=FALSE Waiting=1");

	std::string actual = std::string("result=") + tfstr(result) + " Waiting=" + std::to_string(wedge.Waiting());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (result) { FAIL; }
	if (wedge.Waiting() != 1) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_reset_clears_done_bits_and_restores_waiting() {
	Edge wedge;
	wedge.AddArc(10);
	wedge.AddArc(11);
	wedge.Reset();

	std::ignore = wedge.MarkDone(10);
	std::ignore = wedge.MarkDone(11);
	bool waiting_before_retry_reset = wedge.IsWaiting(); // false -- both parents done

	wedge.Reset(); // node retry: restore full waiting state

	emit_test("Test Edge::Reset() clears ARC_DONE on every arc and restores m_waiting to the full parent count");
	emit_input_header();
	emit_param("WaitEdge", "parents [10, 11], both MarkDone(), then Reset() again (node retry)");

	emit_output_expected_header();
	emit_retval("waiting_before_retry_reset=FALSE after_reset.Waiting=2 arc10.ARC_DONE=FALSE arc11.ARC_DONE=FALSE");

	bool arc10_done = (wedge.GetArc(10).metadata & ARC_DONE) != 0;
	bool arc11_done = (wedge.GetArc(11).metadata & ARC_DONE) != 0;

	std::string actual = std::string("waiting_before_retry_reset=") + tfstr(waiting_before_retry_reset) +
	                      " after_reset.Waiting=" + std::to_string(wedge.Waiting()) +
	                      " arc10.ARC_DONE=" + tfstr(arc10_done) + " arc11.ARC_DONE=" + tfstr(arc11_done);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (waiting_before_retry_reset) { FAIL; }
	if (wedge.Waiting() != 2) { FAIL; }
	if (arc10_done || arc11_done) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// CompactPool()'s real caller is EdgeTable::CompactDirectPool() (see below), removing
// slots a PromoteDirect() call freed by setting id=NO_ID -- exercised here directly on
// a plain Edge by simulating that same "slot freed" state by hand.
static bool test_edge_compact_pool_removes_freed_slots() {
	Edge edge;
	edge.AppendArc(1);
	edge.AppendArc(2);
	edge.AppendArc(3);

	edge[(size_t)1].id = NO_ID; // simulate slot 1 (node 2) having been promoted away

	std::vector<size_t> mapping = edge.CompactPool();

	emit_test("Test Edge::CompactPool() removes NO_ID slots in place and returns an old->new index mapping");
	emit_input_header();
	emit_param("Edge", "[1, 2, 3] with slot 1 (id=2) freed (id=NO_ID)");

	emit_output_expected_header();
	emit_retval("size=2 mapping=[0,MAX,1] edge[0].id=1 edge[1].id=3");

	std::string actual = "size=" + std::to_string(edge.size()) +
	                      " mapping=[" + std::to_string(mapping[0]) + "," +
	                      (mapping[1] == SIZE_MAX ? "MAX" : std::to_string(mapping[1])) + "," +
	                      std::to_string(mapping[2]) + "]" +
	                      " edge[0].id=" + std::to_string(edge[(size_t)0].id) +
	                      " edge[1].id=" + std::to_string(edge[(size_t)1].id);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (edge.size() != 2) { FAIL; }
	if (mapping.size() != 3 || mapping[0] != 0 || mapping[1] != SIZE_MAX || mapping[2] != 1) { FAIL; }
	if (edge[(size_t)0].id != 1 || edge[(size_t)1].id != 3) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edge_begin_end_iteration_const_and_nonconst() {
	Edge edge;
	edge.AddArc(1);
	edge.AddArc(2);
	edge.AddArc(3);

	size_t count = 0;
	node_id_t sum = 0;
	for (auto& arc : edge) { count++; sum += arc.id; }

	const Edge& const_edge = edge;
	size_t const_count = 0;
	for (const auto& arc : const_edge) { std::ignore = arc; const_count++; }

	emit_test("Test Edge::begin()/end() (both overloads) iterate every arc");
	emit_input_header();
	emit_param("Edge", "[1, 2, 3]");

	emit_output_expected_header();
	emit_retval("count=3 sum=6 const_count=3");

	std::string actual = "count=" + std::to_string(count) + " sum=" + std::to_string(sum) +
	                      " const_count=" + std::to_string(const_count);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (count != 3 || sum != 6 || const_count != 3) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_new_edge_plain() {
	EdgeTable table;
	edge_id_t id = table.NewEdge();

	emit_test("Test EdgeTable::NewEdge() with no duplicate creates an empty Edge with refcount 0");
	emit_input_header();
	emit_param("NewEdge", "no duplicate");

	emit_output_expected_header();
	emit_retval("size=0 refcount=0");

	std::string actual = "size=" + std::to_string(table.GetEdge(id).size()) +
	                      " refcount=" + std::to_string(table.GetEdge(id).GetRefCount());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (table.GetEdge(id).size() != 0 || table.GetEdge(id).GetRefCount() != 0) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// NewEdge(&existing) clones the arcs, resets refcount to 0, then increments to 1 --
// the clone is independent, so mutating one must not affect the other.
static bool test_edgetable_new_edge_cow_duplicate_is_independent() {
	EdgeTable table;
	edge_id_t id1 = table.NewEdge();
	table.GetEdge(id1).AddArc(1);
	table.GetEdge(id1).AddArc(2);
	++table.GetEdge(id1);

	Edge copy = table.GetEdge(id1); // stable local copy, as Connect()'s COW does
	edge_id_t id2 = table.NewEdge(&copy);

	table.GetEdge(id2).AddArc(3); // mutate the clone only

	emit_test("Test EdgeTable::NewEdge(&duplicate) clones arcs, sets refcount to 1, and is independent of the original");
	emit_input_header();
	emit_param("NewEdge", "id1=[1,2] refcount=1, clone into id2, then AddArc(3) on id2 only");

	emit_output_expected_header();
	emit_retval("id2!=id1=TRUE id2.size=3 id2.refcount=1 id1.size=2 id1.refcount=1");

	std::string actual = std::string("id2!=id1=") + tfstr(id2 != id1) +
	                      " id2.size=" + std::to_string(table.GetEdge(id2).size()) +
	                      " id2.refcount=" + std::to_string(table.GetEdge(id2).GetRefCount()) +
	                      " id1.size=" + std::to_string(table.GetEdge(id1).size()) +
	                      " id1.refcount=" + std::to_string(table.GetEdge(id1).GetRefCount());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (id2 == id1) { FAIL; }
	if (table.GetEdge(id2).size() != 3 || table.GetEdge(id2).GetRefCount() != 1) { FAIL; }
	if (table.GetEdge(id1).size() != 2 || table.GetEdge(id1).GetRefCount() != 1) { FAIL; } // untouched by id2's mutation

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_is_direct() {
	emit_test("Test EdgeTable::IsDirect() classifies negative ids as direct, non-negative as real edges");
	emit_input_header();
	emit_param("IsDirect", "-1, 0, 1");

	bool neg = EdgeTable::IsDirect(-1);
	bool zero = EdgeTable::IsDirect(0);
	bool pos = EdgeTable::IsDirect(1);

	emit_output_expected_header();
	emit_retval("IsDirect(-1)=TRUE IsDirect(0)=FALSE IsDirect(1)=FALSE");

	std::string actual = std::string("IsDirect(-1)=") + tfstr(neg) + " IsDirect(0)=" + tfstr(zero) +
	                      " IsDirect(1)=" + tfstr(pos);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! neg || zero || pos) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_direct_offset_id_roundtrip() {
	emit_test("Test EdgeTable::DirectIdToOffset()/DirectOffsetToId() are inverses across several slots");
	emit_input_header();
	emit_param("Offsets", "0, 1, 2");

	bool all_match = true;
	std::string actual;
	for (size_t offset = 0; offset < 3; offset++) {
		edge_id_t id = EdgeTable::DirectOffsetToId(offset);
		size_t back = EdgeTable::DirectIdToOffset(id);
		if (back != offset) { all_match = false; }
		actual += "offset=" + std::to_string(offset) + "->id=" + std::to_string(id) +
		          "->offset=" + std::to_string(back) + " ";
	}

	emit_output_expected_header();
	emit_retval("all offsets round-trip through DirectOffsetToId()/DirectIdToOffset() unchanged");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! all_match) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_add_direct_arc_and_get() {
	EdgeTable table;
	edge_id_t id = table.AddDirectArc(42, ARC_WEAK);

	emit_test("Test EdgeTable::AddDirectArc()/GetDirectArc() round-trip a direct-pool arc");
	emit_input_header();
	emit_param("AddDirectArc", "id=42, ARC_WEAK");

	emit_output_expected_header();
	emit_retval("id.IsDirect=TRUE GetDirectArc.id=42 GetDirectArc.IsWeak=TRUE");

	std::string actual = std::string("id.IsDirect=") + tfstr(EdgeTable::IsDirect(id)) +
	                      " GetDirectArc.id=" + std::to_string(table.GetDirectArc(id).id) +
	                      " GetDirectArc.IsWeak=" + tfstr(table.GetDirectArc(id).IsWeak());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! EdgeTable::IsDirect(id)) { FAIL; }
	if (table.GetDirectArc(id).id != 42 || ! table.GetDirectArc(id).IsWeak()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_promote_direct() {
	EdgeTable table;
	edge_id_t direct_id = table.AddDirectArc(7, ARC_WEAK);

	edge_id_t promoted_id = table.PromoteDirect(direct_id);

	emit_test("Test EdgeTable::PromoteDirect() moves a direct arc into a new real Edge and frees its direct slot");
	emit_input_header();
	emit_param("PromoteDirect", "direct arc id=7, ARC_WEAK");

	emit_output_expected_header();
	emit_retval("promoted.IsDirect=FALSE promoted.size=1 promoted.refcount=1 promoted.arc.id=7 promoted.arc.IsWeak=TRUE freed_slot.id==NO_ID=TRUE");

	Edge& promoted = table.GetEdge(promoted_id);
	bool freed_slot = (table.GetDirectArc(direct_id).id == NO_ID);

	std::string actual = std::string("promoted.IsDirect=") + tfstr(EdgeTable::IsDirect(promoted_id)) +
	                      " promoted.size=" + std::to_string(promoted.size()) +
	                      " promoted.refcount=" + std::to_string(promoted.GetRefCount()) +
	                      " promoted.arc.id=" + std::to_string(promoted.GetArc(7).id) +
	                      " promoted.arc.IsWeak=" + tfstr(promoted.GetArc(7).IsWeak()) +
	                      " freed_slot.id==NO_ID=" + tfstr(freed_slot);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (EdgeTable::IsDirect(promoted_id)) { FAIL; }
	if (promoted.size() != 1 || promoted.GetRefCount() != 1) { FAIL; }
	if (promoted.GetArc(7).id != 7 || ! promoted.GetArc(7).IsWeak()) { FAIL; }
	if ( ! freed_slot) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_compact_direct_pool() {
	EdgeTable table;
	edge_id_t d1 = table.AddDirectArc(1);
	edge_id_t d2 = table.AddDirectArc(2);
	edge_id_t d3 = table.AddDirectArc(3);
	std::ignore = d1; std::ignore = d3;

	std::ignore = table.PromoteDirect(d2); // frees the middle slot (offset 1)

	std::vector<size_t> mapping = table.CompactDirectPool();

	emit_test("Test EdgeTable::CompactDirectPool() removes a promoted middle slot and reindexes the rest");
	emit_input_header();
	emit_param("Direct pool", "[1, 2, 3], slot for id=2 promoted away, then CompactDirectPool()");

	edge_id_t new_id0 = EdgeTable::DirectOffsetToId(0);
	edge_id_t new_id1 = EdgeTable::DirectOffsetToId(1);

	emit_output_expected_header();
	emit_retval("mapping=[0,MAX,1] offset0.id=1 offset1.id=3");

	std::string actual = "mapping=[" + std::to_string(mapping[0]) + "," +
	                      (mapping[1] == SIZE_MAX ? "MAX" : std::to_string(mapping[1])) + "," +
	                      std::to_string(mapping[2]) + "]" +
	                      " offset0.id=" + std::to_string(table.GetDirectArc(new_id0).id) +
	                      " offset1.id=" + std::to_string(table.GetDirectArc(new_id1).id);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (mapping.size() != 3 || mapping[0] != 0 || mapping[1] != SIZE_MAX || mapping[2] != 1) { FAIL; }
	if (table.GetDirectArc(new_id0).id != 1 || table.GetDirectArc(new_id1).id != 3) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_new_wait_edge_sequential_ids_and_reset() {
	EdgeTable table;
	edge_id_t w1 = table.NewWaitEdge();
	edge_id_t w2 = table.NewWaitEdge();

	table.GetWaitEdge(w1).AddArc(100);
	table.GetWaitEdge(w1).AddArc(101);
	table.GetWaitEdge(w2).AddArc(200);

	table.ResetWaitEdges(); // initializes m_waiting on every wait edge, as Connect() requires post-population

	emit_test("Test EdgeTable::NewWaitEdge() assigns sequential 1-based ids, and ResetWaitEdges() inits every one");
	emit_input_header();
	emit_param("WaitEdges", "w1=[100,101], w2=[200]");

	emit_output_expected_header();
	emit_retval("w1=1 w2=2 w1.Waiting=2 w2.Waiting=1 NumWaitEdges=2");

	std::string actual = "w1=" + std::to_string(w1) + " w2=" + std::to_string(w2) +
	                      " w1.Waiting=" + std::to_string(table.GetWaitEdge(w1).Waiting()) +
	                      " w2.Waiting=" + std::to_string(table.GetWaitEdge(w2).Waiting()) +
	                      " NumWaitEdges=" + std::to_string(table.NumWaitEdges());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (w1 != 1 || w2 != 2) { FAIL; }
	if (table.GetWaitEdge(w1).Waiting() != 2 || table.GetWaitEdge(w2).Waiting() != 1) { FAIL; }
	if (table.NumWaitEdges() != 2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_arc_count_sums_direct_and_shared_edges() {
	EdgeTable table;
	std::ignore = table.AddDirectArc(1);
	std::ignore = table.AddDirectArc(2);
	std::ignore = table.AddDirectArc(3); // 3 direct arcs, each its own single-parent/single-child link

	edge_id_t id = table.NewEdge();
	table.GetEdge(id).AddArc(4);
	table.GetEdge(id).AddArc(5); // 2 arcs...
	++table.GetEdge(id);
	++table.GetEdge(id); // ...shared by 2 parents -- counts as 2*2=4 child-arcs total

	emit_test("Test EdgeTable::ArcCount() sums direct-pool arcs plus (arcs * refcount) for every shared Edge");
	emit_input_header();
	emit_param("EdgeTable", "3 direct arcs; one shared Edge with 2 arcs and refcount 2");

	emit_output_expected_header();
	emit_retval("ArcCount=7"); // 3 direct + (2 arcs * 2 refcount)

	std::string actual = "ArcCount=" + std::to_string(table.ArcCount());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (table.ArcCount() != 7) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_edgetable_num_edges_and_size() {
	EdgeTable table;
	std::ignore = table.NewEdge();
	std::ignore = table.NewEdge();
	std::ignore = table.NewWaitEdge();

	emit_test("Test EdgeTable::NumEdges()/NumWaitEdges()/size() count each pool correctly");
	emit_input_header();
	emit_param("EdgeTable", "2 extra real Edges (plus the always-present direct pool at index 0), 1 wait edge");

	emit_output_expected_header();
	emit_retval("NumEdges=3 NumWaitEdges=1 size=4");

	std::string actual = "NumEdges=" + std::to_string(table.NumEdges()) +
	                      " NumWaitEdges=" + std::to_string(table.NumWaitEdges()) +
	                      " size=" + std::to_string(table.size());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	// NumEdges() includes m_edges[0] (the direct-arc pool) plus the 2 real Edges just added.
	if (table.NumEdges() != 3) { FAIL; }
	if (table.NumWaitEdges() != 1) { FAIL; }
	if (table.size() != 4) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Proves GetEdge()/operator[]/GetDirectArc()/GetWaitEdge() all resolve through a
// const EdgeTable&, not just their mutable overloads.
static bool test_edgetable_const_overloads() {
	EdgeTable table;

	edge_id_t real_id = table.NewEdge();
	table.GetEdge(real_id).AddArc(5);

	edge_id_t direct_id = table.AddDirectArc(6);

	edge_id_t wait_id = table.NewWaitEdge();
	table.GetWaitEdge(wait_id).AddArc(7);

	const EdgeTable& const_table = table;

	const Edge& real_via_get = const_table.GetEdge(real_id);
	const Edge& real_via_brackets = const_table[real_id];
	const DagArc& direct_arc = const_table.GetDirectArc(direct_id);
	const Edge& wait_edge = const_table.GetWaitEdge(wait_id);

	emit_test("Test EdgeTable::GetEdge()/operator[]/GetDirectArc()/GetWaitEdge() const overloads all resolve through a const EdgeTable&");
	emit_input_header();
	emit_param("EdgeTable", "one real Edge=[5], one direct arc=6, one wait edge=[7], all read through a const EdgeTable&");

	emit_output_expected_header();
	emit_retval("real_via_get.has_5=TRUE same_edge=TRUE direct_arc.id=6 wait_edge.has_7=TRUE");

	std::string actual = std::string("real_via_get.has_5=") + tfstr(real_via_get.Contains(5)) +
	                      " same_edge=" + tfstr(&real_via_get == &real_via_brackets) +
	                      " direct_arc.id=" + std::to_string(direct_arc.id) +
	                      " wait_edge.has_7=" + tfstr(wait_edge.Contains(7));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! real_via_get.Contains(5)) { FAIL; }
	if (&real_via_get != &real_via_brackets) { FAIL; }
	if (direct_arc.id != 6) { FAIL; }
	if ( ! wait_edge.Contains(7)) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
bool OTEST_Edge() {
	emit_object("DagArc / Edge / EdgeTable");
	emit_comment("Testing the non-template edge-table layer (edge.h/edge.cpp) that Dag<D, N>::Connect() is built on.");

	FunctionDriver driver;

	driver.register_function(test_dagarc_default_construction);
	driver.register_function(test_dagarc_parametrized_construction_and_is_weak);

	driver.register_function(test_edge_add_arc_new);
	driver.register_function(test_edge_add_arc_dedupes_existing_id);
	driver.register_function(test_edge_add_arc_strongest_wins_upgrade);
	driver.register_function(test_edge_add_arc_weak_never_downgrades_strong);
	driver.register_function(test_edge_append_arc_no_dedupe);
	driver.register_function(test_edge_get_arc_mutable_reflects_in_const_view);
	driver.register_function(test_edge_contains);
	driver.register_function(test_edge_operator_brackets_by_id_and_by_index);
	driver.register_function(test_edge_const_operator_brackets);
	driver.register_function(test_edge_reserve_prevents_pointer_invalidation);
	driver.register_function(test_edge_ref_counting);
	driver.register_function(test_edge_ref_count_floors_at_zero);
	driver.register_function(test_edge_begin_end_iteration_const_and_nonconst);
	driver.register_function(test_edge_compact_pool_removes_freed_slots);

	driver.register_function(test_edge_mark_done_and_waiting_tick_down);
	driver.register_function(test_edge_mark_done_twice_on_same_parent_is_a_no_op);
	driver.register_function(test_edge_mark_done_unknown_parent_is_a_no_op);
	driver.register_function(test_edge_reset_clears_done_bits_and_restores_waiting);

	driver.register_function(test_edgetable_new_edge_plain);
	driver.register_function(test_edgetable_new_edge_cow_duplicate_is_independent);
	driver.register_function(test_edgetable_is_direct);
	driver.register_function(test_edgetable_direct_offset_id_roundtrip);
	driver.register_function(test_edgetable_add_direct_arc_and_get);
	driver.register_function(test_edgetable_promote_direct);
	driver.register_function(test_edgetable_compact_direct_pool);
	driver.register_function(test_edgetable_new_wait_edge_sequential_ids_and_reset);
	driver.register_function(test_edgetable_arc_count_sums_direct_and_shared_edges);
	driver.register_function(test_edgetable_num_edges_and_size);
	driver.register_function(test_edgetable_const_overloads);

	return driver.do_all_functions();
}
