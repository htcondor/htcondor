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

#pragma once

#include "edge.h"

#include <algorithm>
#include <concepts>
#include <deque>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Traversal order for Dag<D, N>::Walk().
enum class WalkOrder { DFS, BFS };

// Detection idiom: does N define CanAddChild()/CanAddParent()? If not,
// Node<N> falls back to "always allowed" below instead of failing to compile.
template <typename N>
concept HasCanAddChild = requires(const N& n) { { n.CanAddChild() } -> std::convertible_to<bool>; };

template <typename N>
concept HasCanAddParent = requires(const N& n) { { n.CanAddParent() } -> std::convertible_to<bool>; };

// Forward declaration for the detection idioms below.
template <typename D, typename N>
class Dag;

// Detection idiom: does N want a live reference back to its Dag<D, N>? If so,
// AddNode() calls N::SetParent(Dag<D, N>&) once, right after construction.
//
// Gotcha: if SetParent()'s body needs Dag<D, N> complete (stores `&dag`, calls
// any of its methods), it can't be defined inline in N -- Dag<D, N> isn't
// complete yet there. Declare it, define the body out-of-line afterward.
// std::addressof(d) sidesteps this if all it does is store the pointer. See knowledge/DAG.md.
template <typename D, typename N>
concept NodeHasSetParent = requires(N& n, Dag<D, N>& dag) { n.SetParent(dag); };

// Same idea for D (Dag<D, N>'s own data): SetParent(Dag<D, N>&), called once
// from the constructor(s). Useful when a Dag<D, N> is nested inside another.
// Same out-of-line-definition gotcha as above.
template <typename D, typename N>
concept DagHasSetParent = requires(D& d, Dag<D, N>& dag) { d.SetParent(dag); };

// A DAG node: caller metadata N plus the id/edge/visited bookkeeping Dag<D, N>
// needs to connect and walk nodes without looking at N itself.
template <typename N>
class Node {
public:
	Node() = default;

	template<typename... Args>
	Node(node_id_t id, Args&&... args) : data(std::forward<Args>(args)...), m_id(id) {}

	N data{};

	node_id_t GetID() const { return m_id; }

	// Defers to N::CanAddChild()/CanAddParent() if N defines them, else always true.
	bool CanAddChild() const {
		if constexpr (HasCanAddChild<N>) { return data.CanAddChild(); }
		else { return true; }
	}
	bool CanAddParent() const {
		if constexpr (HasCanAddParent<N>) { return data.CanAddParent(); }
		else { return true; }
	}

	// This node's children: direct arc, shared Edge id, or NO_EDGE_ID.
	edge_id_t GetChildEdge() const { return m_children; }
	void SetChildEdge(edge_id_t id) { m_children = id; }

	// Single parent: node_id_t. Multiple: edge_id_t of a wait edge.
	connect_id_t GetParentsID() const { return m_parents; }
	void SetSingleParent(node_id_t id) { m_parents = id; m_multiple_parents = false; }
	void SetWaitEdge(edge_id_t id) { m_parents = id; m_multiple_parents = true; }

	bool HasSingleParent() const { return !m_multiple_parents && m_parents != NO_ID; }
	bool HasMultipleParents() const { return m_multiple_parents; }
	bool NoParents() const { return !m_multiple_parents && m_parents == NO_ID; }
	bool NoChildren() const { return m_children == NO_EDGE_ID; }

	bool WasVisited() const { return m_visited; }
	void MarkVisited(bool visited = true) { m_visited = visited; }

private:
	node_id_t m_id{NO_ID};
	edge_id_t m_children{NO_EDGE_ID};
	connect_id_t m_parents{NO_ID};

	bool m_multiple_parents{false};
	bool m_visited{false};
};

// A DAG container: caller metadata D plus a collection of Node<N>, connected
// via EdgeTable (edge.h). Bare bones by design -- Connect()/VisitChildren()/
// Walk()/Cycle() are the only structural operations, and none inspect D or N.
template <typename D, typename N>
class Dag {
public:
	Dag() { WireDataParent(); }

	// Two overloads, not one: a single unconstrained `template<typename...
	// Args> Dag(Args&&...)` is a better match than the compiler-generated
	// copy/move constructors whenever it's called with exactly one Dag
	// lvalue/rvalue argument (forwarding reference binds without the
	// const-adjustment or move-vs-copy tie-break the special members need),
	// so it silently hijacks them -- then tries to build `data` (type D) from
	// the whole Dag, which doesn't compile. Excluding Args=Dag from the
	// single-argument overload lets copy/move construction (e.g. a
	// std::vector<Dag<D, N>>'s reallocation) resolve to the real special
	// members instead.
	template<typename T>
	requires (!std::same_as<std::remove_cvref_t<T>, Dag>)
	Dag(T&& arg) : data(std::forward<T>(arg)) { WireDataParent(); }

	template<typename... Args>
	requires (sizeof...(Args) != 1)
	Dag(Args&&... args) : data(std::forward<Args>(args)...) { WireDataParent(); }

	D data{};

	// Pre-size before a known-size batch of AddNode() calls, so a held
	// Node<N>*/& doesn't get invalidated by a reallocation mid-batch.
	void Reserve(const size_t n) { m_nodes.reserve(n); }

	// Returns an id, not a Node<N>&, since a later AddNode() can reallocate
	// and invalidate a reference. Look the node back up via FindNode()/
	// operator[] whenever you need it.
	template<typename... Args>
	node_id_t AddNode(Args&&... args) {
		node_id_t id = m_node_generator++;
		m_nodes.emplace_back(id, std::forward<Args>(args)...);
		if constexpr (NodeHasSetParent<D, N>) { m_nodes.back().data.SetParent(*this); }
		return id;
	}

	size_t NumNodes() const { return m_nodes.size(); }
	size_t size() const { return m_nodes.size(); }

	// Bounds check only, no dereference -- safe on a stale/never-added id.
	bool contains(node_id_t id) const {
		return id >= 0 && static_cast<size_t>(id) < m_nodes.size();
	}

	// Bounds-checked lookup by node id; nullptr if out of range.
	Node<N>* FindNode(node_id_t id) {
		if ( ! contains(id)) { return nullptr; }
		return &m_nodes[id];
	}
	const Node<N>* FindNode(node_id_t id) const {
		if ( ! contains(id)) { return nullptr; }
		return &m_nodes[id];
	}

	// Same lookup, throws std::out_of_range on an invalid id -- mirrors Edge::operator[](node_id_t).
	Node<N>& operator[](node_id_t id) {
		Node<N>* node = FindNode(id);
		if ( ! node) { throw std::out_of_range("Invalid node id provided"); }
		return *node;
	}
	const Node<N>& operator[](node_id_t id) const {
		const Node<N>* node = FindNode(id);
		if ( ! node) { throw std::out_of_range("Invalid node id provided"); }
		return *node;
	}

	// Iterates every node regardless of reachability -- unlike Walk(), which
	// only visits nodes reachable from a root. Mirrors Edge's begin()/end() (edge.h).
	auto begin()       { return m_nodes.begin(); }
	auto end()         { return m_nodes.end(); }
	auto begin() const { return m_nodes.begin(); }
	auto end()   const { return m_nodes.end(); }

	// Wires a group of parents to a group of children (by id): direct-arc pool
	// for a single parent/child, shared Edge + copy-on-write for everything
	// else, strongest-wins for ARC_WEAK (a strong re-declaration upgrades an
	// existing weak arc; a weak one never downgrades an existing strong one).
	// Assumes `parents`/`children` are already deduplicated by the caller.
	//
	// No node-type validation -- N's optional CanAddChild()/CanAddParent()
	// hook instead of hardcoded FINAL/SERVICE/PROVISIONER checks. Does keep
	// the shared-parent-group fast path (one Edge for a whole group instead
	// of one per parent): dropping it changes behavior, not just performance
	// (see knowledge/EDGE.md).
	//
	// Returns {true, NO_ID, ""} on success, or {false, failed_id, reason} on
	// failure: an empty parent/child group (failed_id == NO_ID), or a
	// CanAddChild()/CanAddParent() rejection (failed_id names the offending
	// node). No logging of its own -- the message is how a caller finds out why.
	// TODO: Use std::expected once in c++23
	std::tuple<bool, node_id_t, std::string> Connect(const std::vector<node_id_t>& parents, const std::vector<node_id_t>& children, unsigned int meta = 0) {
		if (parents.empty() || children.empty()) {
			std::string message = "No ";
			if (parents.empty()) { message += "parent"; }
			if (parents.empty() && children.empty()) { message += " nor "; }
			if (children.empty()) { message += "child"; }
			message += " nodes provided for dependency creation";
			return {false, NO_ID, message};
		}

		// Policy check -- no-op unless N opts in. (*this)[id] throws on an invalid id.
		for (auto pid : parents) {
			if ( ! (*this)[pid].CanAddChild()) {
				return {false, pid, "Unable to add child dependency"};
			}
		}
		for (auto cid : children) {
			if ( ! (*this)[cid].CanAddParent()) {
				return {false, cid, "Unable to add parent dependency"};
			}
		}

		auto update_parent = [this](node_id_t cid, node_id_t pid) {
			Node<N>& c = (*this)[cid];
			if (c.HasSingleParent()) {
				edge_id_t wedge_id = m_edge_table.NewWaitEdge();
				Edge& wedge = m_edge_table.GetWaitEdge(wedge_id);
				std::ignore = wedge.AddArc(c.GetParentsID());
				std::ignore = wedge.AddArc(pid);
				c.SetWaitEdge(wedge_id);
			} else if (c.HasMultipleParents()) {
				std::ignore = m_edge_table.GetWaitEdge(c.GetParentsID()).AddArc(pid);
			} else {
				c.SetSingleParent(pid);
			}
		};

		// Bulk update_parent(): wires a whole parent group at once. A fresh or
		// newly-promoted wait edge uses AppendArc() (no dedupe scan, since
		// `parents` is already deduped) instead of O(parents^2) update_parent()
		// calls. An existing wait edge from an earlier Connect() still goes
		// through update_parent(), since it may already overlap `parents`.
		auto update_parents = [this, &update_parent](node_id_t cid, const std::vector<node_id_t>& parents) {
			Node<N>& c = (*this)[cid];

			if (c.HasMultipleParents()) {
				for (auto pid : parents) { update_parent(cid, pid); }
			} else if (c.HasSingleParent()) {
				node_id_t old_parent = c.GetParentsID();
				edge_id_t wedge_id = m_edge_table.NewWaitEdge();

				Edge& wedge = m_edge_table.GetWaitEdge(wedge_id);
				wedge.Reserve(parents.size() + 1);
				std::ignore = wedge.AppendArc(old_parent);

				for (auto pid : parents) {
					if (pid != old_parent) { std::ignore = wedge.AppendArc(pid); }
				}

				c.SetWaitEdge(wedge_id);
			} else if (parents.size() == 1) {
				c.SetSingleParent(parents[0]);
			} else {
				edge_id_t wedge_id = m_edge_table.NewWaitEdge();
				Edge& wedge = m_edge_table.GetWaitEdge(wedge_id);
				wedge.Reserve(parents.size());

				for (auto pid : parents) { std::ignore = wedge.AppendArc(pid); }

				c.SetWaitEdge(wedge_id);
			}
		};

		if (parents.size() > 1) {
			edge_id_t check_edge_id = (*this)[parents[0]].GetChildEdge();

			bool share_edge = check_edge_id > 0 && std::ranges::all_of(parents, [this, check_edge_id](node_id_t pid) {
				return (*this)[pid].GetChildEdge() == check_edge_id;
			});

			bool no_edges = ! share_edge && std::ranges::all_of(parents, [this](node_id_t pid) {
				return (*this)[pid].GetChildEdge() == NO_EDGE_ID;
			});

			if (share_edge) {
				// Collect new children, plus already-present weak arcs to upgrade (if strong).
				std::vector<node_id_t> new_children;
				std::vector<node_id_t> upgrade_children;
				for (auto cid : children) {
					if ( ! m_edge_table[check_edge_id].Contains(cid)) {
						new_children.push_back(cid);
					} else if ( ! (meta & ARC_WEAK) && m_edge_table[check_edge_id].GetArc(cid).IsWeak()) {
						upgrade_children.push_back(cid);
					}
				}
				if (new_children.empty() && upgrade_children.empty()) { return {true, NO_ID, ""}; }

				// Single COW if other nodes also hold a reference; otherwise extend in-place.
				if (m_edge_table[check_edge_id].GetRefCount() < parents.size()) {
					throw std::logic_error("Edge refcount lower than sharing parent group size");
				}
				edge_id_t id = check_edge_id;
				if (m_edge_table[check_edge_id].GetRefCount() > parents.size()) {
					Edge copy = m_edge_table[check_edge_id]; // local copy -- stable after emplace_back
					id = m_edge_table.NewEdge(&copy);
					m_edge_table[id].SetRefCount(0);
					for (auto pid : parents) {
						--m_edge_table[check_edge_id]; // index re-resolves after any realloc
						(*this)[pid].SetChildEdge(id);
						++m_edge_table[id];
					}
				}

				Edge& target = m_edge_table[id];

				for (auto cid : upgrade_children) {
					target.GetArc(cid).metadata &= ~ARC_WEAK;
				}

				if ( ! new_children.empty()) {
					target.Reserve(target.size() + new_children.size());
					for (auto cid : new_children) {
						std::ignore = target.AppendArc(cid, meta);
						update_parents(cid, parents);
					}
				}

				return {true, NO_ID, ""};
			} else if (no_edges) {
				// All parents have no edges so just make a shared one right now.
				edge_id_t id = m_edge_table.NewEdge();
				Edge& edge = m_edge_table[id];

				for (auto pid : parents) {
					(*this)[pid].SetChildEdge(id);
					++edge;
				}

				edge.Reserve(children.size());
				for (auto cid : children) {
					std::ignore = edge.AppendArc(cid, meta);
					update_parents(cid, parents);
				}

				return {true, NO_ID, ""};
			}
		}

		for (auto pid : parents) {
			Node<N>& p = (*this)[pid];

			edge_id_t curr = p.GetChildEdge();
			edge_id_t id = NO_EDGE_ID;
			bool fresh_edge = false; // true only when `id` was just created with zero prior arcs

			if (curr == NO_EDGE_ID) {
				if (children.size() == 1) {
					id = m_edge_table.AddDirectArc(children[0], meta);
					p.SetChildEdge(id);
					update_parent(children[0], pid);
					continue;
				}
				id = m_edge_table.NewEdge();
				++m_edge_table[id];
				fresh_edge = true;
			} else if (EdgeTable::IsDirect(curr)) {
				if (children.size() == 1) {
					DagArc& direct = m_edge_table.GetDirectArc(curr);
					if (direct.id == children[0]) {
						// Strongest-wins: a strong re-declaration upgrades an existing weak arc.
						if ( ! (meta & ARC_WEAK) && direct.IsWeak()) { direct.metadata &= ~ARC_WEAK; }
						continue;
					}
				}
				id = m_edge_table.PromoteDirect(curr);
			} else if (m_edge_table[curr].GetRefCount() > 1) {
				// Copy-On-Write: other parents also reference this Edge, clone before mutating.
				Edge copy = m_edge_table[curr]; // local copy -- stable after emplace_back
				--m_edge_table[curr];
				id = m_edge_table.NewEdge(&copy);
			} else {
				id = curr;
			}

			p.SetChildEdge(id);
			Edge& edge = m_edge_table[id];
			if (fresh_edge) { edge.Reserve(children.size()); }

			for (auto cid : children) {
				// A child already in `edge` before this call already has `pid` registered
				// as a parent -- update_parent() must only fire for a genuinely new link,
				// else re-declaring it here would wrongly convert its single-parent state
				// into a bogus wait edge (or double-register it on an existing one).
				bool already_present = ! fresh_edge && edge.Contains(cid);

				if (fresh_edge) {
					std::ignore = edge.AppendArc(cid, meta);
				} else {
					std::ignore = edge.AddArc(cid, meta); // strongest-wins handled inside AddArc
				}

				if ( ! already_present) { update_parent(cid, pid); }
			}
		}

		return {true, NO_ID, ""};
	}

	// Direct EdgeTable access for arc metadata (ARC_WEAK, ARC_DONE, ...) that
	// VisitChildren()/Walk()/Cycle() don't expose.
	EdgeTable& GetEdgeTable() { return m_edge_table; }
	const EdgeTable& GetEdgeTable() const { return m_edge_table; }

	// Calls fn(dag, parent, child) per child, summing return values. The one
	// primitive Walk()/Cycle()/etc. all build on.
	int VisitChildren(Node<N>& node, const std::function<int(Dag&, Node<N>&, Node<N>&)>& fn) {
		int result = 0;
		edge_id_t children = node.GetChildEdge();
		if (children == NO_EDGE_ID) { return result; }

		if (EdgeTable::IsDirect(children)) {
			DagArc& direct = m_edge_table.GetDirectArc(children);
			result += fn(*this, node, (*this)[direct.id]); // throws std::out_of_range if NO_ID or stale
		} else {
			for (const auto& [id, _] : m_edge_table[children]) {
				result += fn(*this, node, (*this)[id]); // throws std::out_of_range if stale
			}
		}

		return result;
	}

	// Visits every node reachable from the roots, calling action(dag, node)
	// once each, in BFS or DFS order.
	void Walk(const std::function<void(Dag&, Node<N>&)>& action, WalkOrder order = WalkOrder::BFS) {
		Walk([&action](Dag& dag, Node<N>& node, size_t) { action(dag, node); }, order);
	}

	// Same, but also passes each node's depth (hops from its root). True
	// shortest-path depth for BFS. For DFS, children are visited in ascending
	// id order (not raw EdgeTable order), so a reconverging node's depth is
	// deterministic, independent of parse/insertion order.
	void Walk(const std::function<void(Dag&, Node<N>&, size_t)>& action, WalkOrder order = WalkOrder::BFS) {
		for (auto& node : m_nodes) { node.MarkVisited(false); }

		std::deque<std::pair<node_id_t, size_t>> queue;
		for (auto& node : m_nodes) {
			if (node.NoParents()) { queue.emplace_back(node.GetID(), 0); }
		}

		while ( ! queue.empty()) {
			auto [id, depth] = queue.front();
			Node<N>& node = (*this)[id];
			queue.pop_front();

			if (node.WasVisited()) { continue; }
			action(*this, node, depth);
			node.MarkVisited();

			std::vector<node_id_t> next_ids;
			VisitChildren(node, [&next_ids](Dag&, Node<N>&, Node<N>& child) -> int {
				if ( ! child.WasVisited()) { next_ids.push_back(child.GetID()); }
				return 0;
			});

			if (order == WalkOrder::DFS) { std::ranges::sort(next_ids); }

			std::vector<std::pair<node_id_t, size_t>> next;
			next.reserve(next_ids.size());
			for (auto cid : next_ids) { next.emplace_back(cid, depth + 1); }

			if (order == WalkOrder::DFS) {
				queue.insert(queue.begin(), next.begin(), next.end());
			} else {
				queue.insert(queue.end(), next.begin(), next.end());
			}
		}
	}

	// Cycle detection via DFS with an explicit ancestor stack. A visited child
	// still on the stack is a back edge. The "all nodes visited" fallback catches
	// a fully disjoint cycle (no parentless entry point).
	//
	// `path`, if non-null, gets the back-edge path (e.g. [A, B, C, A]) when
	// found via the root-driven walk. Left untouched if acyclic, or for a
	// disjoint cycle (no path to give).
	bool Cycle(std::vector<node_id_t>* path = nullptr) {
		for (auto& node : m_nodes) { node.MarkVisited(false); }

		for (auto& node : m_nodes) {
			if (node.NoParents()) {
				std::vector<node_id_t> ancestors;
				if (CycleWalk(node, ancestors)) {
					if (path) { *path = std::move(ancestors); }
					return true;
				}
			}
		}

		return std::ranges::any_of(m_nodes, [](const Node<N>& node) { return ! node.WasVisited(); });
	}

private:
	// D-side SetParent hook, called once per constructor -- no-op unless D opts in.
	void WireDataParent() {
		if constexpr (DagHasSetParent<D, N>) { data.SetParent(*this); }
	}

	bool CycleWalk(Node<N>& node, std::vector<node_id_t>& ancestors) {
		if (node.WasVisited()) {
			if (std::ranges::find(ancestors, node.GetID()) != ancestors.end()) {
				ancestors.push_back(node.GetID()); // close the loop for the reported path
				return true;
			}
			return false;
		}

		node.MarkVisited();

		if (node.NoChildren()) { return false; }

		ancestors.push_back(node.GetID());

		bool cycle = false;
		VisitChildren(node, [this, &ancestors, &cycle](Dag&, Node<N>&, Node<N>& child) -> int {
			if ( ! cycle && CycleWalk(child, ancestors)) { cycle = true; }
			return 0;
		});

		// Only pop if this subtree wasn't the culprit -- else `ancestors` must
		// survive intact for Cycle() to return it.
		if ( ! cycle) { ancestors.pop_back(); }

		return cycle;
	}

	std::vector<Node<N>> m_nodes{};
	node_id_t m_node_generator{0};
	EdgeTable m_edge_table{};
};
