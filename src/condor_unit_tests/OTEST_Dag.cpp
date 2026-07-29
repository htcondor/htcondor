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

// TEST: Generic templated Dag<D, N> / Node<N> container (condor_dagman/dag.hpp)

#include "condor_common.h"
#include "condor_debug.h"
#include "function_test_driver.h"
#include "unit_test_utils.h"
#include "emit.h"
#include "edge.h"
#include "dag.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

//------------------------------------------------------------------------------------
// Plain node metadata: no CanAddChild()/CanAddParent(), used by most tests below to
// prove the container works without N opting into that policy hook.
struct TestData {
	std::string name;
};

using TestDag = Dag<std::string, TestData>;

//------------------------------------------------------------------------------------
static std::vector<node_id_t> ChildIDs(TestDag& dag, Node<TestData>& node) {
	std::vector<node_id_t> ids;
	dag.VisitChildren(node, [&ids](TestDag&, Node<TestData>&, Node<TestData>& child) -> int {
		ids.push_back(child.GetID());
		return 0;
	});
	std::sort(ids.begin(), ids.end());
	return ids;
}

static std::string IDList(const std::vector<node_id_t>& ids) {
	std::string s = "[";
	for (size_t i = 0; i < ids.size(); i++) {
		if (i) { s += ","; }
		s += std::to_string(ids[i]);
	}
	s += "]";
	return s;
}

// dag.hpp has no IsWeakChild() method (see GetEdgeTable() in condor_dagman/knowledge/DAG.md) -- this is what
// a caller builds on top of the exposed EdgeTable to answer the same question, the same
// way VisitChildren() itself resolves a node's child edge.
static bool IsWeakChild(TestDag& dag, node_id_t parent_id, node_id_t child_id) {
	edge_id_t ce = dag[parent_id].GetChildEdge();
	EdgeTable& edges = dag.GetEdgeTable();
	if (EdgeTable::IsDirect(ce)) { return edges.GetDirectArc(ce).IsWeak(); }
	return edges[ce].GetArc(child_id).IsWeak();
}

//------------------------------------------------------------------------------------
static bool test_add_node_and_data_access() {
	TestDag dag;
	dag.data = "my dag";

	// AddNode() returns an id, not a Node<N>& -- so there's no reference here that
	// a later AddNode() call could invalidate by reallocating. Look nodes back up
	// (via operator[]) only after every AddNode() call is done.
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	emit_test("Test AddNode() assigns sequential ids and exposes data as a public field");
	emit_input_header();
	emit_param("Nodes Added", "2");

	emit_output_expected_header();
	emit_retval("dag.data='my dag' A.id=0 A.data.name='A' B.id=1 B.data.name='B' NumNodes=2");

	std::string actual = "dag.data='" + dag.data + "' A.id=" + std::to_string(a) +
	                      " A.data.name='" + dag[a].data.name + "' B.id=" + std::to_string(b) +
	                      " B.data.name='" + dag[b].data.name + "' NumNodes=" + std::to_string(dag.NumNodes());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (dag.data != "my dag") { FAIL; }
	if (a != 0 || dag[a].data.name != "A") { FAIL; }
	if (b != 1 || dag[b].data.name != "B") { FAIL; }
	if (dag.NumNodes() != 2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Reserve()'s whole purpose: let a caller hold a Node<N>* across a known-size batch of
// AddNode() calls without it being invalidated by a mid-batch reallocation.
static bool test_reserve_prevents_pointer_invalidation() {
	TestDag dag;
	dag.Reserve(3);

	node_id_t a = dag.AddNode(TestData{"A"});
	Node<TestData>* a_ptr = &dag[a]; // held across the AddNode() calls below

	std::ignore = dag.AddNode(TestData{"B"});
	std::ignore = dag.AddNode(TestData{"C"});

	emit_test("Test Reserve() lets a held Node<N>* survive a known-size batch of AddNode() calls");
	emit_input_header();
	emit_param("Reserve", "3, then AddNode() A, hold &dag[A], then AddNode() B, C");

	emit_output_expected_header();
	emit_retval("a_ptr==&dag[A]=TRUE a_ptr->data.name='A'");

	std::string actual = std::string("a_ptr==&dag[A]=") + tfstr(a_ptr == &dag[a]) +
	                      " a_ptr->data.name='" + a_ptr->data.name + "'";

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (a_ptr != &dag[a]) { FAIL; }
	if (a_ptr->data.name != "A") { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_find_node_and_operator_brackets() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});

	emit_test("Test FindNode()/operator[] lookup by node id");
	emit_input_header();
	emit_param("Lookup", "id 0 (valid), id 5 (invalid)");

	emit_output_expected_header();
	emit_retval("FindNode(0)==&dag[0] FindNode(5)==nullptr operator[](0).data.name='A'");

	Node<TestData>* found = dag.FindNode(a);
	Node<TestData>* missing = dag.FindNode(5);
	Node<TestData>& via_brackets = dag[a];

	std::string actual = std::string("FindNode(0)==") + (found == &dag[a] ? "&dag[0]" : "other") +
	                      " FindNode(5)==" + (missing == nullptr ? "nullptr" : "non-null") +
	                      " operator[](0).data.name='" + via_brackets.data.name + "'";

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (found != &dag[a]) { FAIL; }
	if (missing != nullptr) { FAIL; }
	if (via_brackets.data.name != "A") { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_contains() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});

	emit_test("Test contains() reports whether a node id exists, without throwing on an invalid one");
	emit_input_header();
	emit_param("Lookup", "id 0 (valid), id 5 (never added), id -1 (NO_ID)");

	bool has_a = dag.contains(a);
	bool has_missing = dag.contains(5);
	bool has_no_id = dag.contains(NO_ID);

	emit_output_expected_header();
	emit_retval("contains(0)=TRUE contains(5)=FALSE contains(NO_ID)=FALSE");

	std::string actual = std::string("contains(0)=") + tfstr(has_a) +
	                      " contains(5)=" + tfstr(has_missing) +
	                      " contains(NO_ID)=" + tfstr(has_no_id);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! has_a || has_missing || has_no_id) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_size_matches_num_nodes() {
	TestDag dag;
	std::ignore = dag.AddNode(TestData{"A"});
	std::ignore = dag.AddNode(TestData{"B"});
	std::ignore = dag.AddNode(TestData{"C"});

	emit_test("Test size() is an alias for NumNodes()");
	emit_input_header();
	emit_param("Nodes Added", "3");

	emit_output_expected_header();
	emit_retval("size=3 NumNodes=3");

	std::string actual = "size=" + std::to_string(dag.size()) + " NumNodes=" + std::to_string(dag.NumNodes());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (dag.size() != 3 || dag.NumNodes() != 3 || dag.size() != dag.NumNodes()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Exercises the const-qualified overloads (FindNode(), operator[], begin()/end(),
// GetEdgeTable()) through an actual const Dag<D, N>&.
static bool test_const_overloads() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	dag.Connect({a}, {b}, ARC_WEAK);

	const TestDag& const_dag = dag;

	emit_test("Test FindNode()/operator[]/begin()/end()/GetEdgeTable() const overloads work through a const Dag<D, N>&");
	emit_input_header();
	emit_param("Graph", "A -[weak]-> B, accessed through a const reference");

	const Node<TestData>* found = const_dag.FindNode(a);
	const Node<TestData>& via_brackets = const_dag[a];

	size_t iterated = 0;
	for (const auto& node : const_dag) { std::ignore = node; iterated++; }

	// EdgeTable::GetDirectArc()/operator[]/Edge::GetArc() are const-qualified (edge.h), so
	// a weak-arc check works through the const EdgeTable& too, not just a mutable one.
	const EdgeTable& edges = const_dag.GetEdgeTable();
	edge_id_t ce = const_dag[a].GetChildEdge();
	bool weak = EdgeTable::IsDirect(ce) ? edges.GetDirectArc(ce).IsWeak() : edges[ce].GetArc(b).IsWeak();

	emit_output_expected_header();
	emit_retval("found.name='A' brackets.name='A' iterated=2 A-B.weak=TRUE");

	std::string actual = std::string("found.name='") + found->data.name + "' brackets.name='" + via_brackets.data.name +
	                      "' iterated=" + std::to_string(iterated) + " A-B.weak=" + tfstr(weak);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! found || found->data.name != "A") { FAIL; }
	if (via_brackets.data.name != "A") { FAIL; }
	if (iterated != 2) { FAIL; }
	if ( ! weak) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_no_parents_no_children_predicates() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	bool a_fresh_no_parents = dag[a].NoParents();
	bool a_fresh_no_children = dag[a].NoChildren();

	dag.Connect({a}, {b});

	emit_test("Test NoParents()/NoChildren() reflect a node's connection state before and after Connect()");
	emit_input_header();
	emit_param("Graph", "A (fresh), then A -> B");

	emit_output_expected_header();
	emit_retval("A.fresh.NoParents=TRUE A.fresh.NoChildren=TRUE A.NoChildren=FALSE A.NoParents=TRUE B.NoParents=FALSE B.NoChildren=TRUE");

	std::string actual = std::string("A.fresh.NoParents=") + tfstr(a_fresh_no_parents) +
	                      " A.fresh.NoChildren=" + tfstr(a_fresh_no_children) +
	                      " A.NoChildren=" + tfstr(dag[a].NoChildren()) +
	                      " A.NoParents=" + tfstr(dag[a].NoParents()) +
	                      " B.NoParents=" + tfstr(dag[b].NoParents()) +
	                      " B.NoChildren=" + tfstr(dag[b].NoChildren());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! a_fresh_no_parents || ! a_fresh_no_children) { FAIL; }
	if (dag[a].NoChildren()) { FAIL; }    // A now has a child (B)
	if ( ! dag[a].NoParents()) { FAIL; }  // A still has no parents
	if (dag[b].NoParents()) { FAIL; }     // B now has a parent (A)
	if ( ! dag[b].NoChildren()) { FAIL; } // B still has no children

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_connect_direct_arc_single_child() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	emit_test("Test Connect() wires a single parent to a single child via the direct-arc pool");
	emit_input_header();
	emit_param("Connect", "A -> B");

	bool ok = std::get<0>(dag.Connect({a}, {b}));

	emit_output_expected_header();
	emit_retval("Connect=TRUE A.IsDirect=TRUE A.children=[1] B.HasSingleParent=TRUE B.ParentsID=0");

	std::string actual = std::string("Connect=") + tfstr(ok) +
	                      " A.IsDirect=" + tfstr(EdgeTable::IsDirect(dag[a].GetChildEdge())) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a])) +
	                      " B.HasSingleParent=" + tfstr(dag[b].HasSingleParent()) +
	                      " B.ParentsID=" + std::to_string(dag[b].GetParentsID());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if ( ! EdgeTable::IsDirect(dag[a].GetChildEdge())) { FAIL; }
	if ( ! dag[b].HasSingleParent() || dag[b].GetParentsID() != a) { FAIL; }
	if (ChildIDs(dag, dag[a]) != std::vector<node_id_t>{b}) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_connect_single_parent_multi_child() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	emit_test("Test Connect() wires a single parent to multiple children via a real Edge");
	emit_input_header();
	emit_param("Connect", "A -> {B, C, D}");

	bool ok = std::get<0>(dag.Connect({a}, {b, c, d}));
	auto children = ChildIDs(dag, dag[a]);

	emit_output_expected_header();
	emit_retval("Connect=TRUE A.IsDirect=FALSE A.children=[1,2,3]");

	std::string actual = std::string("Connect=") + tfstr(ok) +
	                      " A.IsDirect=" + tfstr(EdgeTable::IsDirect(dag[a].GetChildEdge())) +
	                      " A.children=" + IDList(children);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if (EdgeTable::IsDirect(dag[a].GetChildEdge())) { FAIL; } // 3 children can't fit the direct-arc pool
	if (children != (std::vector<node_id_t>{b, c, d})) { FAIL; }
	if ( ! dag[b].HasSingleParent() || ! dag[c].HasSingleParent() || ! dag[d].HasSingleParent()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_weak_arc_direct_and_general_paths() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	emit_test("Test Connect() marks arcs weak, across the direct-arc path and promotion into a real Edge");
	emit_input_header();
	emit_param("Connect", "A -[weak]-> B (direct arc), then A -[weak]-> {C, D} (promotes to a shared Edge)");

	bool ok1 = std::get<0>(dag.Connect({a}, {b}, ARC_WEAK));
	bool ok2 = std::get<0>(dag.Connect({a}, {c, d}, ARC_WEAK));

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE A-B=WEAK A-C=WEAK A-D=WEAK");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " A-B=" + (IsWeakChild(dag, a, b) ? "WEAK" : "STRONG") +
	                      " A-C=" + (IsWeakChild(dag, a, c) ? "WEAK" : "STRONG") +
	                      " A-D=" + (IsWeakChild(dag, a, d) ? "WEAK" : "STRONG");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	if ( ! IsWeakChild(dag, a, b)) { FAIL; } // survived promotion out of the direct-arc pool
	if ( ! IsWeakChild(dag, a, c)) { FAIL; }
	if ( ! IsWeakChild(dag, a, d)) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_weak_arc_single_parent_upgrade() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	emit_test("Test Connect() upgrades a weak arc to strong on re-declaration (single-parent, shared Edge)");
	emit_input_header();
	emit_param("Connect", "A -[weak]-> {B, C}, then A -> B (strong)");

	bool ok1 = std::get<0>(dag.Connect({a}, {b, c}, ARC_WEAK));
	bool ok2 = std::get<0>(dag.Connect({a}, {b}, 0));

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE A-B=STRONG A-C=WEAK");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " A-B=" + (IsWeakChild(dag, a, b) ? "WEAK" : "STRONG") +
	                      " A-C=" + (IsWeakChild(dag, a, c) ? "WEAK" : "STRONG");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	if (IsWeakChild(dag, a, b)) { FAIL; }    // re-declared strong: must upgrade
	if ( ! IsWeakChild(dag, a, c)) { FAIL; } // untouched: must stay weak

	PASS;
}

//------------------------------------------------------------------------------------
// Connect()'s shared-parent-group fast path must upgrade an already-weak arc to
// strong for children already in the group, not just newly-added ones.
static bool test_weak_arc_group_upgrade_and_no_downgrade() {
	TestDag dag;
	node_id_t p1 = dag.AddNode(TestData{"P1"});
	node_id_t p2 = dag.AddNode(TestData{"P2"});
	node_id_t p3 = dag.AddNode(TestData{"P3"});
	node_id_t c1 = dag.AddNode(TestData{"C1"});
	node_id_t c2 = dag.AddNode(TestData{"C2"});

	emit_test("Test Connect()'s shared-parent-group fast path upgrades weak to strong, but never downgrades");
	emit_input_header();
	emit_param("Connect", "{P1,P2,P3} -[weak]-> {C1,C2}, then C1 re-declared strong, then C2 re-declared weak");

	bool ok1 = std::get<0>(dag.Connect({p1, p2, p3}, {c1, c2}, ARC_WEAK)); // fresh group: no_edges path
	bool ok2 = std::get<0>(dag.Connect({p1, p2, p3}, {c1}, 0));            // shared group: share_edge path, upgrade
	bool ok3 = std::get<0>(dag.Connect({p1, p2, p3}, {c2}, ARC_WEAK));     // shared group: share_edge path, no-op (already weak)

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE ok3=TRUE P1-C1=STRONG P2-C1=STRONG P3-C1=STRONG P1-C2=WEAK");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) + " ok3=" + tfstr(ok3) +
	                      " P1-C1=" + (IsWeakChild(dag, p1, c1) ? "WEAK" : "STRONG") +
	                      " P2-C1=" + (IsWeakChild(dag, p2, c1) ? "WEAK" : "STRONG") +
	                      " P3-C1=" + (IsWeakChild(dag, p3, c1) ? "WEAK" : "STRONG") +
	                      " P1-C2=" + (IsWeakChild(dag, p1, c2) ? "WEAK" : "STRONG");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2 || ! ok3) { FAIL; }
	// Shared Edge: the upgrade must be visible from every parent in the group, not just one.
	if (IsWeakChild(dag, p1, c1) || IsWeakChild(dag, p2, c1) || IsWeakChild(dag, p3, c1)) { FAIL; }
	if ( ! IsWeakChild(dag, p1, c2)) { FAIL; } // re-declaring weak over weak must not change anything

	PASS;
}

//------------------------------------------------------------------------------------
// The direct-arc pool's dedupe path (edge.h's `direct.id == children[0]` check in
// Connect()): re-declaring the exact same single parent/single child link a second
// time must be a no-op, not a promotion into a real Edge.
static bool test_connect_direct_arc_redeclared_same_child_is_idempotent() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	bool ok1 = std::get<0>(dag.Connect({a}, {b}));
	edge_id_t edge_after_first = dag[a].GetChildEdge();
	bool ok2 = std::get<0>(dag.Connect({a}, {b})); // same single child again
	edge_id_t edge_after_second = dag[a].GetChildEdge();

	emit_test("Test Connect() re-declaring the same direct-arc child a second time is a no-op, not a promotion");
	emit_input_header();
	emit_param("Connect", "A -> B, then A -> B again");

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE still_direct=TRUE edge_id_unchanged=TRUE A.children=[1]");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " still_direct=" + tfstr(EdgeTable::IsDirect(edge_after_second)) +
	                      " edge_id_unchanged=" + tfstr(edge_after_first == edge_after_second) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a]));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	if ( ! EdgeTable::IsDirect(edge_after_second)) { FAIL; } // must not have promoted into a real Edge
	if (edge_after_first != edge_after_second) { FAIL; }
	if (ChildIDs(dag, dag[a]) != std::vector<node_id_t>{b}) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A single parent's second Connect() call naming a *different* single child must
// promote the existing direct arc into a real Edge (PromoteDirect()), then extend it.
static bool test_connect_direct_arc_promotes_on_different_single_child() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	bool ok1 = std::get<0>(dag.Connect({a}, {b})); // direct arc: A -> B
	bool was_direct = EdgeTable::IsDirect(dag[a].GetChildEdge());
	bool ok2 = std::get<0>(dag.Connect({a}, {c})); // different single child -> forces promotion

	emit_test("Test Connect() promotes a direct arc into a real Edge when a later call names a different single child");
	emit_input_header();
	emit_param("Connect", "A -> B (direct arc), then A -> C (different single child)");

	emit_output_expected_header();
	emit_retval("ok1=TRUE was_direct=TRUE ok2=TRUE now_direct=FALSE A.children=[1,2]");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " was_direct=" + tfstr(was_direct) +
	                      " ok2=" + tfstr(ok2) +
	                      " now_direct=" + tfstr(EdgeTable::IsDirect(dag[a].GetChildEdge())) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a]));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! was_direct || ! ok2) { FAIL; }
	if (EdgeTable::IsDirect(dag[a].GetChildEdge())) { FAIL; } // must have promoted
	if (ChildIDs(dag, dag[a]) != (std::vector<node_id_t>{b, c})) { FAIL; } // B survives the promotion, C is added

	PASS;
}

//------------------------------------------------------------------------------------
// Re-declaring an already-connected child (B) alongside a genuinely new one (C) in the
// same call must leave B's single-parent state untouched -- update_parent() must only
// fire for children genuinely new to this edge, not every child in the list.
static bool test_connect_redeclared_child_keeps_single_parent_state() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	bool ok1 = std::get<0>(dag.Connect({a}, {b}));    // direct arc: A -> B
	bool ok2 = std::get<0>(dag.Connect({a}, {b, c})); // re-declares B, adds C

	emit_test("Test Connect() re-declaring an already-connected child alongside a new one doesn't corrupt its parent state");
	emit_input_header();
	emit_param("Connect", "A -> B, then A -> {B, C}");

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE B.HasSingleParent=TRUE B.ParentsID=A C.HasSingleParent=TRUE C.ParentsID=A");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " B.HasSingleParent=" + tfstr(dag[b].HasSingleParent()) +
	                      " B.ParentsID==A=" + tfstr(dag[b].GetParentsID() == a) +
	                      " C.HasSingleParent=" + tfstr(dag[c].HasSingleParent()) +
	                      " C.ParentsID==A=" + tfstr(dag[c].GetParentsID() == a);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	if ( ! dag[b].HasSingleParent() || dag[b].GetParentsID() != a) { FAIL; } // must NOT have become a wait edge
	if ( ! dag[c].HasSingleParent() || dag[c].GetParentsID() != a) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A parent that solely owns a real Edge (refcount==1) reuses it in place on a later
// Connect() call, instead of promoting or copy-on-write cloning.
static bool test_connect_extends_existing_edge_in_place() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	bool ok1 = std::get<0>(dag.Connect({a}, {b, c})); // real Edge, refcount 1, owned solely by A
	edge_id_t edge_after_first = dag[a].GetChildEdge();
	bool ok2 = std::get<0>(dag.Connect({a}, {d}));    // extend it in place -- no COW needed
	edge_id_t edge_after_second = dag[a].GetChildEdge();

	emit_test("Test Connect() extends a solely-owned real Edge in place rather than cloning it");
	emit_input_header();
	emit_param("Connect", "A -> {B, C}, then A -> D");

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE edge_id_unchanged=TRUE A.children=[1,2,3]");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " edge_id_unchanged=" + tfstr(edge_after_first == edge_after_second) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a]));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	if (edge_after_first != edge_after_second) { FAIL; } // reused the same Edge id, not a new one
	if (ChildIDs(dag, dag[a]) != (std::vector<node_id_t>{b, c, d})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Connect()'s share_edge/no_edges group fast paths only fire when every parent in the
// group has the same starting state. A mixed group -- one parent with its own unrelated
// Edge, one parent fresh -- fails both checks and falls back to the general per-parent loop.
static bool test_connect_mixed_parent_group_falls_back_to_per_parent() {
	TestDag dag;
	node_id_t p1 = dag.AddNode(TestData{"P1"});
	node_id_t p2 = dag.AddNode(TestData{"P2"});
	node_id_t x1 = dag.AddNode(TestData{"X1"});
	node_id_t x2 = dag.AddNode(TestData{"X2"});
	node_id_t y = dag.AddNode(TestData{"Y"});
	node_id_t z = dag.AddNode(TestData{"Z"});

	// Two children (not one) so P1 starts out owning a real Edge, not a direct arc, so
	// its child-edge id stays stable for the "edge_unchanged" assertion below.
	dag.Connect({p1}, {x1, x2});
	edge_id_t p1_edge_before = dag[p1].GetChildEdge(); // P2 stays fresh (NO_EDGE_ID)

	emit_test("Test Connect() falls back to the per-parent loop for a mixed group (neither share_edge nor no_edges applies)");
	emit_input_header();
	emit_param("Connect", "P1 already -> {X1, X2} (own Edge), P2 fresh, then {P1, P2} -> {Y, Z}");

	bool ok = std::get<0>(dag.Connect({p1, p2}, {y, z}));

	auto p1_children = ChildIDs(dag, dag[p1]);
	auto p2_children = ChildIDs(dag, dag[p2]);
	bool p1_edge_unchanged = (dag[p1].GetChildEdge() == p1_edge_before);
	bool p1_p2_share_edge = (dag[p1].GetChildEdge() == dag[p2].GetChildEdge());

	emit_output_expected_header();
	emit_retval("Connect=TRUE P1.children=[2,3,4,5] P2.children=[4,5] P1.edge_unchanged=TRUE P1_P2_share_edge=FALSE");

	std::string actual = std::string("Connect=") + tfstr(ok) +
	                      " P1.children=" + IDList(p1_children) + " P2.children=" + IDList(p2_children) +
	                      " P1.edge_unchanged=" + tfstr(p1_edge_unchanged) +
	                      " P1_P2_share_edge=" + tfstr(p1_p2_share_edge);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if (p1_children != (std::vector<node_id_t>{x1, x2, y, z})) { FAIL; } // P1's own Edge extended in place
	if (p2_children != (std::vector<node_id_t>{y, z})) { FAIL; }        // P2 got its own separate Edge
	if ( ! p1_edge_unchanged) { FAIL; }  // P1 reused its existing Edge (refcount 1, no COW needed)
	if (p1_p2_share_edge) { FAIL; }      // each parent ended up with its own distinct Edge

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_connect_multi_parent_single_child() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	emit_test("Test Connect() wires multiple parents to a single child via a wait edge");
	emit_input_header();
	emit_param("Connect", "{A, B} -> C");

	bool ok = std::get<0>(dag.Connect({a, b}, {c}));

	emit_output_expected_header();
	emit_retval("Connect=TRUE C.HasMultipleParents=TRUE A.children=[2] B.children=[2]");

	std::string actual = std::string("Connect=") + tfstr(ok) +
	                      " C.HasMultipleParents=" + tfstr(dag[c].HasMultipleParents()) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a])) +
	                      " B.children=" + IDList(ChildIDs(dag, dag[b]));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if ( ! dag[c].HasMultipleParents()) { FAIL; }
	if (ChildIDs(dag, dag[a]) != std::vector<node_id_t>{c}) { FAIL; }
	if (ChildIDs(dag, dag[b]) != std::vector<node_id_t>{c}) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// All the other weak-arc tests use a single-parent Connect() -- this is the one that
// exercises ARC_WEAK on a child that ends up with a wait edge (multiple parents).
static bool test_weak_arc_via_wait_edge_multi_parent() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	emit_test("Test Connect() marks a wait-edge arc weak when a child has multiple parents");
	emit_input_header();
	emit_param("Connect", "{A, B} -[weak]-> C");

	bool ok = std::get<0>(dag.Connect({a, b}, {c}, ARC_WEAK));

	emit_output_expected_header();
	emit_retval("Connect=TRUE C.HasMultipleParents=TRUE A-C=WEAK B-C=WEAK");

	std::string actual = std::string("Connect=") + tfstr(ok) +
	                      " C.HasMultipleParents=" + tfstr(dag[c].HasMultipleParents()) +
	                      " A-C=" + (IsWeakChild(dag, a, c) ? "WEAK" : "STRONG") +
	                      " B-C=" + (IsWeakChild(dag, b, c) ? "WEAK" : "STRONG");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if ( ! dag[c].HasMultipleParents()) { FAIL; }
	if ( ! IsWeakChild(dag, a, c) || ! IsWeakChild(dag, b, c)) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A wait edge can also be built one single-parent Connect() call at a time: converting
// a child's existing single parent into a wait edge the first time a second shows up.
static bool test_connect_single_parent_converts_to_wait_edge_via_second_call() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	bool ok1 = std::get<0>(dag.Connect({a}, {c})); // C's first parent: direct arc, single parent
	bool c_single_after_first = dag[c].HasSingleParent();

	emit_test("Test Connect() converts a child's single parent into a wait edge the first time a second single-parent call arrives");
	emit_input_header();
	emit_param("Connect", "A -> C, then B -> C (two separate single-parent calls)");

	bool ok2 = std::get<0>(dag.Connect({b}, {c})); // C's second parent, added one at a time

	emit_output_expected_header();
	emit_retval("ok1=TRUE c_single_after_first=TRUE ok2=TRUE C.HasMultipleParents=TRUE A.IsDirect=TRUE B.IsDirect=TRUE A.children=[2] B.children=[2]");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " c_single_after_first=" + tfstr(c_single_after_first) +
	                      " ok2=" + tfstr(ok2) + " C.HasMultipleParents=" + tfstr(dag[c].HasMultipleParents()) +
	                      " A.IsDirect=" + tfstr(EdgeTable::IsDirect(dag[a].GetChildEdge())) +
	                      " B.IsDirect=" + tfstr(EdgeTable::IsDirect(dag[b].GetChildEdge())) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a])) +
	                      " B.children=" + IDList(ChildIDs(dag, dag[b]));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! c_single_after_first || ! ok2) { FAIL; }
	if ( ! dag[c].HasMultipleParents()) { FAIL; } // must have converted, not stayed single-parent
	// A and B never shared a parent group -- each keeps its own independent direct arc to C.
	if ( ! EdgeTable::IsDirect(dag[a].GetChildEdge()) || ! EdgeTable::IsDirect(dag[b].GetChildEdge())) { FAIL; }
	if (ChildIDs(dag, dag[a]) != std::vector<node_id_t>{c} || ChildIDs(dag, dag[b]) != std::vector<node_id_t>{c}) { FAIL; }

	const Edge& wedge = dag.GetEdgeTable().GetWaitEdge(dag[c].GetParentsID());
	bool wedge_has_both = wedge.Contains(a) && wedge.Contains(b);

	if ( ! wedge_has_both || wedge.size() != 2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A child that already has a wait edge (2+ parents) can gain yet another parent via a
// single-parent Connect() call, extending the existing wait edge rather than rebuilding it.
static bool test_connect_extends_existing_wait_edge_with_new_single_parent() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	bool ok1 = std::get<0>(dag.Connect({a, b}, {c})); // wait edge with 2 parents, built in one group call
	connect_id_t wedge_id_before = dag[c].GetParentsID();

	emit_test("Test Connect() extends an existing wait edge with one more parent via a later single-parent call");
	emit_input_header();
	emit_param("Connect", "{A, B} -> C (wait edge), then D -> C (third parent, single-parent call)");

	bool ok2 = std::get<0>(dag.Connect({d}, {c}));

	connect_id_t wedge_id_after = dag[c].GetParentsID();
	const Edge& wedge = dag.GetEdgeTable().GetWaitEdge(wedge_id_after);

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE C.HasMultipleParents=TRUE wedge_id_unchanged=TRUE wedge.size=3 wedge.has_a=TRUE wedge.has_b=TRUE wedge.has_d=TRUE");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " C.HasMultipleParents=" + tfstr(dag[c].HasMultipleParents()) +
	                      " wedge_id_unchanged=" + tfstr(wedge_id_before == wedge_id_after) +
	                      " wedge.size=" + std::to_string(wedge.size()) +
	                      " wedge.has_a=" + tfstr(wedge.Contains(a)) +
	                      " wedge.has_b=" + tfstr(wedge.Contains(b)) +
	                      " wedge.has_d=" + tfstr(wedge.Contains(d));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	if ( ! dag[c].HasMultipleParents()) { FAIL; }
	if (wedge_id_before != wedge_id_after) { FAIL; } // extended in place, not rebuilt
	if (wedge.size() != 3 || ! wedge.Contains(a) || ! wedge.Contains(b) || ! wedge.Contains(d)) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A fresh parent group shares one Edge (the "no_edges" fast path); when one parent
// later diverges, it must clone its own Edge rather than mutating the shared one.
static bool test_connect_group_then_diverge_cow() {
	TestDag dag;
	node_id_t p1 = dag.AddNode(TestData{"P1"});
	node_id_t p2 = dag.AddNode(TestData{"P2"});
	node_id_t p3 = dag.AddNode(TestData{"P3"});
	node_id_t c1 = dag.AddNode(TestData{"C1"});
	node_id_t c2 = dag.AddNode(TestData{"C2"});
	node_id_t c3 = dag.AddNode(TestData{"C3"});

	emit_test("Test Connect() shares one Edge across a fresh parent group, then COWs on divergence");
	emit_input_header();
	emit_param("Connect", "{P1,P2,P3} -> {C1,C2}, then {P2} -> {C3}");

	bool ok1 = std::get<0>(dag.Connect({p1, p2, p3}, {c1, c2}));
	bool ok2 = std::get<0>(dag.Connect({p2}, {c3}));

	auto p1_children = ChildIDs(dag, dag[p1]);
	auto p2_children = ChildIDs(dag, dag[p2]);
	auto p3_children = ChildIDs(dag, dag[p3]);

	emit_output_expected_header();
	emit_retval("ok1=TRUE ok2=TRUE P1=[3,4] P2=[3,4,5] P3=[3,4]");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " ok2=" + tfstr(ok2) +
	                      " P1=" + IDList(p1_children) + " P2=" + IDList(p2_children) +
	                      " P3=" + IDList(p3_children);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok1 || ! ok2) { FAIL; }
	// P2 diverged and must see all three children; P1/P3 must be untouched by that divergence.
	if (p1_children != (std::vector<node_id_t>{c1, c2})) { FAIL; }
	if (p2_children != (std::vector<node_id_t>{c1, c2, c3})) { FAIL; }
	if (p3_children != (std::vector<node_id_t>{c1, c2})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Every other test's VisitChildren() callback (via the ChildIDs() helper) always
// returns 0 -- this is the one that actually exercises "return values are summed".
static bool test_visit_children_sums_return_values() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	dag.Connect({a}, {b, c, d});

	emit_test("Test VisitChildren() sums each callback's return value across every child");
	emit_input_header();
	emit_param("Graph", "A -> {B, C, D}, fn returns 10 per child; also called on leaf B (no children)");

	int total = dag.VisitChildren(dag[a], [](TestDag&, Node<TestData>&, Node<TestData>&) -> int { return 10; });
	int leaf_total = dag.VisitChildren(dag[b], [](TestDag&, Node<TestData>&, Node<TestData>&) -> int { return 10; });

	emit_output_expected_header();
	emit_retval("total=30 leaf_total=0");

	std::string actual = "total=" + std::to_string(total) + " leaf_total=" + std::to_string(leaf_total);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (total != 30) { FAIL; }
	if (leaf_total != 0) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_connect_empty_fails() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});

	emit_test("Test Connect() returns {false, NO_ID, reason} for an empty parent or child list");
	emit_input_header();
	emit_param("Connect", "{} -> {A}, and {A} -> {}");

	auto [ok1, failed1, msg1] = dag.Connect({}, {a});
	auto [ok2, failed2, msg2] = dag.Connect({a}, {});

	emit_output_expected_header();
	emit_retval("ok1=FALSE failed1=NO_ID msg1!=empty ok2=FALSE failed2=NO_ID msg2!=empty");

	std::string actual = std::string("ok1=") + tfstr(ok1) + " failed1" + (failed1 == NO_ID ? "==NO_ID" : "!=NO_ID") +
	                      " msg1" + (msg1.empty() ? "==empty" : "!=empty") +
	                      " ok2=" + tfstr(ok2) + " failed2" + (failed2 == NO_ID ? "==NO_ID" : "!=NO_ID") +
	                      " msg2" + (msg2.empty() ? "==empty" : "!=empty");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (ok1 || ok2) { FAIL; }
	if (failed1 != NO_ID || failed2 != NO_ID) { FAIL; } // no specific node to blame for an empty group
	if (msg1.empty() || msg2.empty()) { FAIL; } // caller needs a reason since there's no debug_printf() to fall back on

	PASS;
}

//------------------------------------------------------------------------------------
// Nothing in Connect() rejects a node naming itself as its own parent/child. The
// resulting node ends up as its own single parent, so NoParents() is false afterward.
static bool test_connect_self_loop_single_node() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});

	emit_test("Test Connect() allows a self-loop (A -> A)");
	emit_input_header();
	emit_param("Connect", "A -> A");

	bool ok = std::get<0>(dag.Connect({a}, {a}));

	emit_output_expected_header();
	emit_retval("Connect=TRUE A.HasSingleParent=TRUE A.ParentsID==A=TRUE A.NoParents=FALSE A.children=[0]");

	std::string actual = std::string("Connect=") + tfstr(ok) +
	                      " A.HasSingleParent=" + tfstr(dag[a].HasSingleParent()) +
	                      " A.ParentsID==A=" + tfstr(dag[a].GetParentsID() == a) +
	                      " A.NoParents=" + tfstr(dag[a].NoParents()) +
	                      " A.children=" + IDList(ChildIDs(dag, dag[a]));

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if ( ! dag[a].HasSingleParent() || dag[a].GetParentsID() != a) { FAIL; }
	if (dag[a].NoParents()) { FAIL; } // A has a parent (itself) -- not root-eligible for Walk()/Cycle()
	if (ChildIDs(dag, dag[a]) != std::vector<node_id_t>{a}) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A self-loop node is its own parent, so NoParents() is false for it -- Cycle()'s
// root-driven walk never reaches it. Only the disjoint fallback catches it.
static bool test_cycle_detects_self_loop_via_disjoint_fallback() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	dag.Connect({a}, {a});

	emit_test("Test Cycle() detects a self-loop only via its disjoint fallback (A is never a walk root)");
	emit_input_header();
	emit_param("Graph", "A -> A (self-loop)");

	std::vector<node_id_t> path;
	bool cycle = dag.Cycle(&path);

	emit_output_expected_header();
	emit_retval("Cycle=TRUE path.empty=TRUE");

	std::string actual = std::string("Cycle=") + tfstr(cycle) + " path.empty=" + tfstr(path.empty());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! cycle) { FAIL; }
	if ( ! path.empty()) { FAIL; } // no root-driven walk ever ran, so there's no back-edge path to report

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_cycle_none() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	dag.Connect({a}, {b});
	dag.Connect({a}, {c});
	dag.Connect({b}, {c});

	emit_test("Test Cycle() returns false for an acyclic graph, and leaves an out-param path untouched");
	emit_input_header();
	emit_param("Graph", "A->B, A->C, B->C");

	std::vector<node_id_t> path;
	bool cycle = dag.Cycle(&path);

	emit_output_expected_header();
	emit_retval("Cycle=FALSE path.empty=TRUE");

	emit_output_actual_header();
	std::string actual = std::string("Cycle=") + tfstr(cycle) + " path.empty=" + tfstr(path.empty());
	emit_retval(actual.c_str());

	if (cycle) { FAIL; }
	if ( ! path.empty()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A cycle reachable from a real root: the ancestor-stack walk must catch the back edge
// to B while B is still on the current path. The reported path must survive intact
// back up to Cycle(), not get popped away as the recursion unwinds.
static bool test_cycle_root_reachable_backedge() {
	TestDag dag;
	node_id_t r = dag.AddNode(TestData{"R"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	dag.Connect({r}, {a});
	dag.Connect({a}, {b});
	dag.Connect({b}, {a}); // back edge: A is still on the current DFS path

	emit_test("Test Cycle() detects a back edge reachable from a root, and reports its path");
	emit_input_header();
	emit_param("Graph", "R->A->B->A");

	std::vector<node_id_t> path;
	bool cycle = dag.Cycle(&path);

	emit_output_expected_header();
	emit_retval("Cycle=TRUE path=[0,1,2,1]"); // R -> A -> B -> A (closing the loop back to A)

	emit_output_actual_header();
	std::string actual = std::string("Cycle=") + tfstr(cycle) + " path=" + IDList(path);
	emit_retval(actual.c_str());

	if ( ! cycle) { FAIL; }
	if (path != (std::vector<node_id_t>{r, a, b, a})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// A fully-closed ring has no parentless node, so the root-driven walk never starts --
// only the disjoint fallback catches it, and there's no path to report (stays empty).
static bool test_cycle_fully_disjoint_ring() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});

	dag.Connect({a}, {b});
	dag.Connect({b}, {c});
	dag.Connect({c}, {a}); // closes the ring -- no node here has NoParents()

	emit_test("Test Cycle() detects a fully disjoint ring with no entry point, and leaves path untouched");
	emit_input_header();
	emit_param("Graph", "A->B->C->A (no root)");

	std::vector<node_id_t> path;
	bool cycle = dag.Cycle(&path);

	emit_output_expected_header();
	emit_retval("Cycle=TRUE path.empty=TRUE");

	emit_output_actual_header();
	std::string actual = std::string("Cycle=") + tfstr(cycle) + " path.empty=" + tfstr(path.empty());
	emit_retval(actual.c_str());

	if ( ! cycle) { FAIL; }
	if ( ! path.empty()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Calls Cycle() with the default path=nullptr on a graph where a cycle IS found via the
// root-driven walk, proving the "if (path)" guard is actually reached.
static bool test_cycle_default_path_argument_is_nullptr_safe() {
	TestDag dag;
	node_id_t r = dag.AddNode(TestData{"R"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	dag.Connect({r}, {a});
	dag.Connect({a}, {b});
	dag.Connect({b}, {a}); // back edge, found via the root-driven walk from R

	emit_test("Test Cycle() with the default path=nullptr doesn't crash when a cycle is found via the root-driven walk");
	emit_input_header();
	emit_param("Graph", "R->A->B->A, Cycle() called with no path argument");

	bool cycle = dag.Cycle(); // path defaults to nullptr

	emit_output_expected_header();
	emit_retval("Cycle=TRUE");

	emit_output_actual_header();
	std::string actual = std::string("Cycle=") + tfstr(cycle);
	emit_retval(actual.c_str());

	if ( ! cycle) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_walk_bfs_order() {
	TestDag dag;
	node_id_t r = dag.AddNode(TestData{"R"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	dag.Connect({r}, {a, b});
	dag.Connect({a}, {c});
	dag.Connect({b}, {d});

	std::vector<node_id_t> order;
	dag.Walk([&order](TestDag&, Node<TestData>& node) { order.push_back(node.GetID()); }, WalkOrder::BFS);

	emit_test("Test Walk() visits nodes in breadth-first order");
	emit_input_header();
	emit_param("Graph", "R->{A,B}, A->C, B->D");

	emit_output_expected_header();
	emit_retval("[0,1,2,3,4]");

	emit_output_actual_header();
	std::string actual = IDList(order);
	emit_retval(actual.c_str());

	if (order != (std::vector<node_id_t>{r, a, b, c, d})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_walk_dfs_order() {
	TestDag dag;
	node_id_t r = dag.AddNode(TestData{"R"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	dag.Connect({r}, {a, b});
	dag.Connect({a}, {c});
	dag.Connect({b}, {d});

	std::vector<node_id_t> order;
	dag.Walk([&order](TestDag&, Node<TestData>& node) { order.push_back(node.GetID()); }, WalkOrder::DFS);

	emit_test("Test Walk() visits nodes in depth-first order");
	emit_input_header();
	emit_param("Graph", "R->{A,B}, A->C, B->D");

	emit_output_expected_header();
	emit_retval("[0,1,3,2,4]");

	emit_output_actual_header();
	std::string actual = IDList(order);
	emit_retval(actual.c_str());

	if (order != (std::vector<node_id_t>{r, a, c, b, d})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// The 3-arg Walk() overload also hands back each node's depth (hops from its root) --
// verify it's threaded through correctly, not just that traversal order is right.
static bool test_walk_tracks_depth() {
	TestDag dag;
	node_id_t r = dag.AddNode(TestData{"R"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});

	dag.Connect({r}, {a, b});
	dag.Connect({a}, {c});
	dag.Connect({b}, {d});

	std::map<node_id_t, size_t> depths;
	dag.Walk([&depths](TestDag&, Node<TestData>& node, size_t depth) { depths[node.GetID()] = depth; }, WalkOrder::BFS);

	emit_test("Test Walk()'s depth-aware overload reports each node's hop-count from its root");
	emit_input_header();
	emit_param("Graph", "R->{A,B}, A->C, B->D");

	emit_output_expected_header();
	emit_retval("R=0 A=1 B=1 C=2 D=2");

	std::string actual = "R=" + std::to_string(depths[r]) + " A=" + std::to_string(depths[a]) +
	                      " B=" + std::to_string(depths[b]) + " C=" + std::to_string(depths[c]) +
	                      " D=" + std::to_string(depths[d]);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (depths[r] != 0 || depths[a] != 1 || depths[b] != 1 || depths[c] != 2 || depths[d] != 2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Diamond graph where the two paths to D differ in length (A->B->D vs. A->C->E->D).
// Connect() deliberately declares children in descending id order ({c, b}) to prove
// Walk()'s DFS child order comes from sorting by id, not insertion order: without the
// sort, D would be reached via C->E first (depth 3); with it, via B first (depth 2).
static bool test_walk_dfs_diamond_depth_sorted_by_id() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});
	node_id_t c = dag.AddNode(TestData{"C"});
	node_id_t d = dag.AddNode(TestData{"D"});
	node_id_t node_e = dag.AddNode(TestData{"E"}); // not `e` -- shadows FAIL/PASS's global Emit `e`

	dag.Connect({a}, {c, b}); // deliberately descending id order
	dag.Connect({b}, {d});
	dag.Connect({c}, {node_e});
	dag.Connect({node_e}, {d});

	std::map<node_id_t, size_t> depths;
	dag.Walk([&depths](TestDag&, Node<TestData>& node, size_t depth) { depths[node.GetID()] = depth; }, WalkOrder::DFS);

	emit_test("Test Walk()'s DFS order sorts children by id, not Connect() call/insertion order");
	emit_input_header();
	emit_param("Graph", "A->{C,B} (declared in that order), B->D, C->E->D");

	emit_output_expected_header();
	emit_retval("D=2"); // via A->B->D, since B's id < C's id

	std::string actual = "D=" + std::to_string(depths[d]);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (depths[d] != 2) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Walk()'s queue is seeded from every NoParents() node, so two fully independent trees
// must both be reachable from a single Walk() call, interleaved in id order.
static bool test_walk_multiple_independent_roots() {
	TestDag dag;
	node_id_t r1 = dag.AddNode(TestData{"R1"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t r2 = dag.AddNode(TestData{"R2"});
	node_id_t b = dag.AddNode(TestData{"B"});

	dag.Connect({r1}, {a}); // two entirely separate trees, R1->A and R2->B
	dag.Connect({r2}, {b});

	std::vector<node_id_t> order;
	dag.Walk([&order](TestDag&, Node<TestData>& node) { order.push_back(node.GetID()); }, WalkOrder::BFS);

	emit_test("Test Walk() visits every independent root, not just the first one found");
	emit_input_header();
	emit_param("Graph", "R1->A and R2->B (two disjoint trees, both with their own root)");

	emit_output_expected_header();
	emit_retval("[0,2,1,3]"); // both roots seeded at depth 0 in id order, then each root's child

	std::string actual = IDList(order);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (order != (std::vector<node_id_t>{r1, r2, a, b})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_walk_on_empty_dag_is_a_no_op() {
	TestDag dag; // no AddNode() calls at all

	size_t visited = 0;
	dag.Walk([&visited](TestDag&, Node<TestData>&) { visited++; });

	emit_test("Test Walk() on an empty Dag<D, N> visits nothing and doesn't crash");
	emit_input_header();
	emit_param("Dag", "no nodes added");

	emit_output_expected_header();
	emit_retval("visited=0");

	std::string actual = "visited=" + std::to_string(visited);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (visited != 0) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_cycle_on_empty_dag_returns_false() {
	TestDag dag; // no AddNode() calls at all

	std::vector<node_id_t> path;
	bool cycle = dag.Cycle(&path);

	emit_test("Test Cycle() on an empty Dag<D, N> returns false and leaves path untouched");
	emit_input_header();
	emit_param("Dag", "no nodes added");

	emit_output_expected_header();
	emit_retval("Cycle=FALSE path.empty=TRUE");

	std::string actual = std::string("Cycle=") + tfstr(cycle) + " path.empty=" + tfstr(path.empty());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (cycle) { FAIL; } // the "not everything visited" fallback is vacuously false over zero nodes
	if ( ! path.empty()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Walk() only visits nodes reachable from a root -- a disjoint cycle (no parentless
// entry point) is invisible to it. begin()/end() sees it anyway.
static bool test_begin_end_sees_unreachable_nodes() {
	TestDag dag;
	node_id_t r = dag.AddNode(TestData{"R"});
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t x = dag.AddNode(TestData{"X"});
	node_id_t y = dag.AddNode(TestData{"Y"});

	dag.Connect({r}, {a});
	dag.Connect({x}, {y});
	dag.Connect({y}, {x}); // X<->Y: a disjoint 2-cycle, no root -- invisible to Walk()

	std::vector<node_id_t> walked;
	dag.Walk([&walked](TestDag&, Node<TestData>& node) { walked.push_back(node.GetID()); });
	std::sort(walked.begin(), walked.end());

	std::vector<node_id_t> all;
	for (auto& node : dag) { all.push_back(node.GetID()); }
	std::sort(all.begin(), all.end());

	emit_test("Test begin()/end() see every node regardless of connectivity, unlike Walk()");
	emit_input_header();
	emit_param("Graph", "R->A (reachable), X<->Y (disjoint 2-cycle, no root)");

	emit_output_expected_header();
	emit_retval("Walk()=[0,1] begin/end=[0,1,2,3]");

	std::string actual = "Walk()=" + IDList(walked) + " begin/end=" + IDList(all);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (walked != (std::vector<node_id_t>{r, a})) { FAIL; }
	if (all != (std::vector<node_id_t>{r, a, x, y})) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_can_add_child_parent_defaults_true() {
	// TestData defines neither CanAddChild() nor CanAddParent() -- the detection
	// idiom backing Node<N>::CanAddChild()/CanAddParent() must fall back to "always
	// allowed" instead of failing to compile.
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	emit_test("Test CanAddChild()/CanAddParent() default to true when N doesn't define them");
	emit_input_header();
	emit_param("N", "TestData (no CanAddChild/CanAddParent members)");

	bool can_add_child = dag[a].CanAddChild();
	bool can_add_parent = dag[b].CanAddParent();
	bool ok = std::get<0>(dag.Connect({a}, {b}));

	emit_output_expected_header();
	emit_retval("A.CanAddChild=TRUE B.CanAddParent=TRUE Connect=TRUE");

	std::string actual = std::string("A.CanAddChild=") + tfstr(can_add_child) +
	                      " B.CanAddParent=" + tfstr(can_add_parent) +
	                      " Connect=" + tfstr(ok);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! can_add_child || ! can_add_parent || ! ok) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Metadata that opts into the CanAddChild()/CanAddParent() policy hook.
struct PolicyData {
	std::string name;
	bool allow_child{true};
	bool allow_parent{true};

	bool CanAddChild() const { return allow_child; }
	bool CanAddParent() const { return allow_parent; }
};

using PolicyDag = Dag<std::string, PolicyData>;

static bool test_can_add_child_parent_honors_policy() {
	PolicyDag dag;
	node_id_t a = dag.AddNode(PolicyData{"A", false, true}); // A refuses another child
	node_id_t b = dag.AddNode(PolicyData{"B", true, false}); // B refuses another parent
	node_id_t c = dag.AddNode(PolicyData{"C", true, true});  // unrestricted

	emit_test("Test Connect() honors N::CanAddChild()/CanAddParent() when N defines them");
	emit_input_header();
	emit_param("Policy", "A.CanAddChild=false, B.CanAddParent=false, C=unrestricted");

	auto [ok_a, failed_a, msg_a] = dag.Connect({a}, {b}); // blocked by A's CanAddChild()==false
	auto [ok_b, failed_b, msg_b] = dag.Connect({c}, {b}); // blocked by B's CanAddParent()==false

	emit_output_expected_header();
	emit_retval("ok_a=FALSE failed_a=A msg_a!=empty ok_b=FALSE failed_b=B msg_b!=empty");

	std::string actual = std::string("ok_a=") + tfstr(ok_a) + " failed_a=" + (failed_a == a ? "A" : "?") +
	                      " msg_a" + (msg_a.empty() ? "==empty" : "!=empty") +
	                      " ok_b=" + tfstr(ok_b) + " failed_b=" + (failed_b == b ? "B" : "?") +
	                      " msg_b" + (msg_b.empty() ? "==empty" : "!=empty");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (ok_a || ok_b) { FAIL; }
	// failed_a/failed_b must name the specific node whose policy hook rejected the
	// request -- A (the parent) for CanAddChild()==false, B (the child) for
	// CanAddParent()==false, not just "some node failed".
	if (failed_a != a || failed_b != b) { FAIL; }
	if (msg_a.empty() || msg_b.empty()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Defines only CanAddChild(), proving HasCanAddChild/HasCanAddParent are independent
// detection idioms -- opting into one doesn't affect the other's default.
struct ChildOnlyPolicyData {
	std::string name;
	bool allow_child{true};

	bool CanAddChild() const { return allow_child; }
	// no CanAddParent() defined -- must still default to true, same as TestData
};

using ChildOnlyPolicyDag = Dag<std::string, ChildOnlyPolicyData>;

static bool test_can_add_child_only_policy_leaves_parent_hook_defaulted() {
	ChildOnlyPolicyDag dag;
	node_id_t a = dag.AddNode(ChildOnlyPolicyData{"A", false}); // refuses another child
	node_id_t b = dag.AddNode(ChildOnlyPolicyData{"B", true});

	emit_test("Test a type defining only CanAddChild() still defaults CanAddParent() to true");
	emit_input_header();
	emit_param("N", "ChildOnlyPolicyData (defines CanAddChild() only), A.allow_child=false");

	bool a_can_add_parent = dag[a].CanAddParent(); // no CanAddParent() on N -- must default true
	auto [ok, failed_id, msg] = dag.Connect({a}, {b}); // still blocked by A's CanAddChild()==false

	emit_output_expected_header();
	emit_retval("A.CanAddParent=TRUE Connect=FALSE failed_id=A msg!=empty");

	std::string actual = std::string("A.CanAddParent=") + tfstr(a_can_add_parent) +
	                      " Connect=" + tfstr(ok) + " failed_id=" + (failed_id == a ? "A" : "?") +
	                      " msg" + (msg.empty() ? "==empty" : "!=empty");

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! a_can_add_parent) { FAIL; } // CanAddParent() must default true even though CanAddChild() is defined
	if (ok || failed_id != a || msg.empty()) { FAIL; } // CanAddChild()==false must still be honored

	PASS;
}

//------------------------------------------------------------------------------------
// N doesn't have to be a struct/class -- Node<N>::data just needs to be default-
// constructible and direct-initializable from AddNode()'s args, which a plain built-in
// like int satisfies fine. CanAddChild()/CanAddParent() must still default to true for
// a type with no member functions at all to detect.
using IntDag = Dag<std::string, int>;

static bool test_dag_of_plain_ints() {
	IntDag dag;
	dag.data = "int dag";

	node_id_t a = dag.AddNode(10);
	node_id_t b = dag.AddNode(20);
	node_id_t c = dag.AddNode(30);

	emit_test("Test Dag<D, N> works with a plain built-in N (int), not just a struct");
	emit_input_header();
	emit_param("Nodes", "a=10, b=20, c=30; Connect a -> {b, c}");

	bool ok = std::get<0>(dag.Connect({a}, {b, c}));

	int sum = 0;
	dag.Walk([&sum](IntDag&, Node<int>& node) { sum += node.data; });

	emit_output_expected_header();
	emit_retval("Connect=TRUE sum=60 A.CanAddChild=TRUE C.CanAddParent=TRUE");

	std::string actual = std::string("Connect=") + tfstr(ok) + " sum=" + std::to_string(sum) +
	                      " A.CanAddChild=" + tfstr(dag[a].CanAddChild()) +
	                      " C.CanAddParent=" + tfstr(dag[c].CanAddParent());

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! ok) { FAIL; }
	if (sum != 60) { FAIL; }
	if ( ! dag[a].CanAddChild() || ! dag[c].CanAddParent()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Dag<D, N>'s constructor and AddNode() are both variadic, forwarding straight to
// D's/N's own constructor -- individual member values work directly, no need to
// pre-build a D/N value. For a plain aggregate like these, this relies on C++20's
// paren-init-for-aggregates (P0960).
struct MultiFieldDagData {
	std::string name;
	int priority;
	bool enabled;
};

struct MultiFieldNodeData {
	std::string name;
	int weight;
	double cost;
};

using MultiFieldDag = Dag<MultiFieldDagData, MultiFieldNodeData>;

static bool test_variadic_construction_from_individual_members() {
	MultiFieldDag dag("my workflow", 5, true); // Dag(Args&&...) -> D("my workflow", 5, true)

	node_id_t a = dag.AddNode("A", 10, 2.5); // AddNode(Args&&...) -> N("A", 10, 2.5)

	emit_test("Test Dag(...)/AddNode(...) forward individual arguments straight to D's/N's own constructor");
	emit_input_header();
	emit_param("Dag", "(\"my workflow\", 5, true)");
	emit_param("AddNode", "(\"A\", 10, 2.5)");

	emit_output_expected_header();
	emit_retval("dag.data={name='my workflow' priority=5 enabled=TRUE} A.data={name='A' weight=10 cost=2.5}");

	std::string actual = "dag.data={name='" + dag.data.name + "' priority=" + std::to_string(dag.data.priority) +
	                      " enabled=" + tfstr(dag.data.enabled) + "} A.data={name='" + dag[a].data.name +
	                      "' weight=" + std::to_string(dag[a].data.weight) +
	                      " cost=" + std::to_string(dag[a].data.cost) + "}";

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (dag.data.name != "my workflow" || dag.data.priority != 5 || ! dag.data.enabled) { FAIL; }
	if (dag[a].data.name != "A" || dag[a].data.weight != 10 || ! floats_close(dag[a].data.cost, 2.5)) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_operator_brackets_throws_on_invalid_id() {
	TestDag dag;
	std::ignore = dag.AddNode(TestData{"A"}); // only exists so `dag` isn't empty

	emit_test("Test operator[] throws std::out_of_range for an id that was never added");
	emit_input_header();
	emit_param("Lookup", "id 5 (only id 0 exists)");

	bool threw = false;
	try {
		std::ignore = dag[5];
	} catch (const std::out_of_range&) {
		threw = true;
	}

	emit_output_expected_header();
	emit_retval("threw std::out_of_range=TRUE");

	emit_output_actual_header();
	std::string actual = std::string("threw std::out_of_range=") + tfstr(threw);
	emit_retval(actual.c_str());

	if ( ! threw) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Connect()'s id-validity checks route through (*this)[id], so an invalid parent or
// child id throws the same std::out_of_range as operator[].
static bool test_connect_throws_on_invalid_node_id() {
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});
	node_id_t b = dag.AddNode(TestData{"B"});

	emit_test("Test Connect() throws std::out_of_range for a parent or child id that was never added");
	emit_input_header();
	emit_param("Connect", "{99} -> {b}, and {a} -> {99}");

	bool threw_bad_parent = false;
	try {
		std::ignore = dag.Connect({99}, {b});
	} catch (const std::out_of_range&) {
		threw_bad_parent = true;
	}

	bool threw_bad_child = false;
	try {
		std::ignore = dag.Connect({a}, {99});
	} catch (const std::out_of_range&) {
		threw_bad_child = true;
	}

	emit_output_expected_header();
	emit_retval("threw_bad_parent=TRUE threw_bad_child=TRUE");

	emit_output_actual_header();
	std::string actual = std::string("threw_bad_parent=") + tfstr(threw_bad_parent) +
	                      " threw_bad_child=" + tfstr(threw_bad_child);
	emit_retval(actual.c_str());

	if ( ! threw_bad_parent || ! threw_bad_child) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
// Node/Dag metadata that opt into the SetParent() hook (dag.hpp) -- confirms Dag<D, N>
// wires up a live parent reference on both sides, and that the full public API is
// reachable through it, not just a non-null pointer.
//
// ParentAwareNodeData::SetParent() is defined inline since ParentAwareDagData is
// already complete by this point. ParentAwareDagData::SetParent() can't do the same
// (it needs ParentAwareNodeData, and therefore Dag<D, N>, complete first), so it's
// declared here and defined out-of-line below. See condor_dagman/knowledge/DAG.md's "Parent-reference hook".
struct ParentAwareNodeData; // forward declare -- needed by ParentAwareDagData below
struct ParentAwareDagData;
using ParentAwareDag = Dag<ParentAwareDagData, ParentAwareNodeData>;

struct ParentAwareDagData {
	ParentAwareDag* self{nullptr};
	void SetParent(ParentAwareDag& dag); // defined out-of-line below
};

struct ParentAwareNodeData {
	std::string name;
	ParentAwareDag* parent{nullptr};
	void SetParent(ParentAwareDag& dag) { parent = &dag; }
};

inline void ParentAwareDagData::SetParent(ParentAwareDag& dag) { self = &dag; }

//------------------------------------------------------------------------------------
static bool test_set_parent_hook_wires_node_data() {
	ParentAwareDag dag;
	node_id_t a = dag.AddNode(ParentAwareNodeData{"A"});

	emit_test("Test AddNode() auto-wires Node<N>::data.SetParent() when N defines it");
	emit_input_header();
	emit_param("N", "ParentAwareNodeData (defines SetParent(Dag<D, N>&))");

	bool points_at_dag = (dag[a].data.parent == &dag);

	// Not just non-null -- actually usable: reach the full public API through the
	// stored pointer, the same as any other caller holding a Dag<D, N>&.
	node_id_t b = dag[a].data.parent->AddNode(ParentAwareNodeData{"B"});
	auto [connected, failed_id, why] = dag[a].data.parent->Connect({a}, {b});

	emit_output_expected_header();
	emit_retval("parent==&dag=TRUE NumNodes=2 Connect=TRUE failed_id=NO_ID why=''");

	std::string actual = std::string("parent==&dag=") + tfstr(points_at_dag) +
	                      " NumNodes=" + std::to_string(dag.NumNodes()) +
	                      " Connect=" + tfstr(connected) +
	                      " failed_id" + (failed_id == NO_ID ? "==NO_ID" : "!=NO_ID") +
	                      " why='" + why + "'";

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! points_at_dag) { FAIL; }
	if (dag.NumNodes() != 2) { FAIL; }
	if ( ! connected || failed_id != NO_ID || ! why.empty()) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_set_parent_hook_wires_dag_data() {
	ParentAwareDag dag;

	emit_test("Test Dag<D, N>'s constructor auto-wires D::SetParent() when D defines it");
	emit_input_header();
	emit_param("D", "ParentAwareDagData (defines SetParent(Dag<D, N>&))");

	bool points_at_self = (dag.data.self == &dag);

	// Same check as above: usable, not just non-null.
	node_id_t a = dag.data.self->AddNode(ParentAwareNodeData{"A"});

	emit_output_expected_header();
	emit_retval("self==&dag=TRUE NumNodes=1 a==0=TRUE");

	std::string actual = std::string("self==&dag=") + tfstr(points_at_self) +
	                      " NumNodes=" + std::to_string(dag.NumNodes()) +
	                      " a==0=" + tfstr(a == 0);

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if ( ! points_at_self) { FAIL; }
	if (dag.NumNodes() != 1) { FAIL; }
	if (a != 0) { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
static bool test_set_parent_hook_absent_is_fine() {
	// TestData/std::string (TestDag's N/D) define neither SetParent() -- AddNode() and
	// Dag<D, N>'s constructor must not require it, mirroring CanAddChild/CanAddParent's
	// default-true behavior when N doesn't opt in.
	TestDag dag;
	node_id_t a = dag.AddNode(TestData{"A"});

	emit_test("Test AddNode()/Dag<D, N> construction don't require SetParent() when N/D don't define it");
	emit_input_header();
	emit_param("N, D", "TestData, std::string (neither defines SetParent)");

	emit_output_expected_header();
	emit_retval("NumNodes=1 A.data.name='A'");

	std::string actual = "NumNodes=" + std::to_string(dag.NumNodes()) + " A.data.name='" + dag[a].data.name + "'";

	emit_output_actual_header();
	emit_retval(actual.c_str());

	if (dag.NumNodes() != 1) { FAIL; }
	if (dag[a].data.name != "A") { FAIL; }

	PASS;
}

//------------------------------------------------------------------------------------
bool OTEST_Dag() {
	emit_object("Dag<D, N> / Node<N>");
	emit_comment("Testing the generic templated DAG container (dag.hpp).");

	FunctionDriver driver;

	driver.register_function(test_add_node_and_data_access);
	driver.register_function(test_reserve_prevents_pointer_invalidation);
	driver.register_function(test_find_node_and_operator_brackets);
	driver.register_function(test_contains);
	driver.register_function(test_size_matches_num_nodes);
	driver.register_function(test_const_overloads);

	driver.register_function(test_no_parents_no_children_predicates);
	driver.register_function(test_connect_direct_arc_single_child);
	driver.register_function(test_connect_direct_arc_redeclared_same_child_is_idempotent);
	driver.register_function(test_connect_direct_arc_promotes_on_different_single_child);
	driver.register_function(test_connect_redeclared_child_keeps_single_parent_state);
	driver.register_function(test_connect_extends_existing_edge_in_place);
	driver.register_function(test_connect_mixed_parent_group_falls_back_to_per_parent);
	driver.register_function(test_connect_single_parent_multi_child);
	driver.register_function(test_weak_arc_direct_and_general_paths);
	driver.register_function(test_weak_arc_single_parent_upgrade);
	driver.register_function(test_weak_arc_group_upgrade_and_no_downgrade);
	driver.register_function(test_connect_multi_parent_single_child);
	driver.register_function(test_weak_arc_via_wait_edge_multi_parent);
	driver.register_function(test_connect_single_parent_converts_to_wait_edge_via_second_call);
	driver.register_function(test_connect_extends_existing_wait_edge_with_new_single_parent);
	driver.register_function(test_connect_group_then_diverge_cow);
	driver.register_function(test_visit_children_sums_return_values);
	driver.register_function(test_connect_empty_fails);
	driver.register_function(test_connect_self_loop_single_node);

	driver.register_function(test_cycle_none);
	driver.register_function(test_cycle_root_reachable_backedge);
	driver.register_function(test_cycle_fully_disjoint_ring);
	driver.register_function(test_cycle_default_path_argument_is_nullptr_safe);
	driver.register_function(test_cycle_detects_self_loop_via_disjoint_fallback);
	driver.register_function(test_cycle_on_empty_dag_returns_false);

	driver.register_function(test_walk_bfs_order);
	driver.register_function(test_walk_dfs_order);
	driver.register_function(test_walk_tracks_depth);
	driver.register_function(test_walk_dfs_diamond_depth_sorted_by_id);
	driver.register_function(test_walk_multiple_independent_roots);
	driver.register_function(test_walk_on_empty_dag_is_a_no_op);
	driver.register_function(test_begin_end_sees_unreachable_nodes);

	driver.register_function(test_can_add_child_parent_defaults_true);
	driver.register_function(test_can_add_child_parent_honors_policy);
	driver.register_function(test_can_add_child_only_policy_leaves_parent_hook_defaulted);

	driver.register_function(test_dag_of_plain_ints);
	driver.register_function(test_variadic_construction_from_individual_members);

	driver.register_function(test_operator_brackets_throws_on_invalid_id);
	driver.register_function(test_connect_throws_on_invalid_node_id);

	driver.register_function(test_set_parent_hook_wires_node_data);
	driver.register_function(test_set_parent_hook_wires_dag_data);
	driver.register_function(test_set_parent_hook_absent_is_fine);

	return driver.do_all_functions();
}
