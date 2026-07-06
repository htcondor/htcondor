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

#include <vector>

using node_id_t = int; // Specific node id
using edge_id_t = int; // Specific edge id
using connect_id_t = int; // Node or edge id

constexpr node_id_t NO_ID = -1;
constexpr edge_id_t NO_EDGE_ID = 0;

// Arc metadata bitmasks
constexpr unsigned int ARC_DONE = (1u << 0); // Parent has completed (wait arcs only)
constexpr unsigned int ARC_WEAK = (1u << 1); // Child only needs parent to execute, not succeed (children edges only)

// Individual connection between Parent -> Child Node
struct Arc {
	Arc() = default;
	Arc(const node_id_t id, const unsigned int meta) : id(id), metadata(meta) {}

	node_id_t id{NO_ID};
	unsigned int metadata{0};

	bool IsWeak() const { return metadata & ARC_WEAK; }
};

// Collection of shared arcs from Parent(s) to Child(ren)
class Edge {
public:
	Edge() = default;

	size_t AddArc(const Arc& arc);
	size_t AddArc(const node_id_t id, const unsigned int meta = 0);
	size_t AppendArc(const node_id_t id, const unsigned int meta = 0);
	Arc& GetArc(const node_id_t id);

	// Pre-size the backing arc vector before a known-size run of AppendArc() calls,
	// so bulk construction doesn't pay for repeated amortized-doubling reallocation.
	void Reserve(const size_t n) { m_arcs.reserve(n); }

	Arc& operator[](const node_id_t id) { return GetArc(id); }
	Arc& operator[](const size_t idx)   { return m_arcs[idx]; }

	bool Contains(const node_id_t id);

	// Wait edge operations — set ARC_DONE bits on parent arcs as they complete.
	// Call Reset() once after all parents are added via AddArc to initialize m_waiting.
	// Reset() also restores full waiting state on node retry.
	bool MarkDone(node_id_t parent_id); // returns true when last parent finishes
	void Reset();                        // clear ARC_DONE bits; set m_waiting = size()

	// Compact in-place: remove slots where id==NO_ID (promoted arcs).
	// Returns mapping old_index -> new_index (SIZE_MAX for removed slots).
	std::vector<size_t> CompactPool();

	bool IsWaiting() const { return m_waiting > 0; }
	uint32_t Waiting() const { return m_waiting; }

	auto begin()       { return m_arcs.begin(); }
	auto end()         { return m_arcs.end(); }

	auto begin() const { return m_arcs.begin(); }
	auto end()   const { return m_arcs.end(); }

	Edge& operator++() {
		m_references++;
		return *this;
	}

	Edge& operator--() {
		if (m_references > 0) {
			m_references--;
		}
		return *this;
	}

	inline void SetRefCount(const size_t n) { m_references = n; }
	inline size_t GetRefCount() const { return m_references; }

	inline size_t size() const { return m_arcs.size(); }
private:
	std::vector<Arc> m_arcs{};
	size_t m_references{0};
	uint32_t m_waiting{0};
};

// Collection of all Edges in DAG
// Edge 0 is collection of single parent -> child Arcs
// referenced via a negative # edge id
class EdgeTable {
public:
	EdgeTable() = default;

	edge_id_t NewEdge(const Edge* duplicate = nullptr);
	Edge& GetEdge(const edge_id_t id);

	Edge& operator[](const edge_id_t id) { return GetEdge(id); }

	static bool IsDirect(const edge_id_t id);
	static size_t DirectIdToOffset(edge_id_t id);
	static edge_id_t DirectOffsetToId(size_t idx);

	edge_id_t AddDirectArc(const node_id_t id, const unsigned int meta = 0);
	Arc& GetDirectArc(const edge_id_t id);
	edge_id_t PromoteDirect(const edge_id_t id);

	// Compact the direct-arc pool (m_edges[0]), removing promoted slots.
	// Returns mapping old_offset -> new_offset (SIZE_MAX for removed slots).
	std::vector<size_t> CompactDirectPool();

	// Wait edges — one per child node tracking parent completion.
	// IDs are 1-based; NO_EDGE_ID(0) means the node has no parents.
	edge_id_t NewWaitEdge();
	Edge& GetWaitEdge(const edge_id_t id);
	// Initialize m_waiting on all wait edges after Connect() has fully populated them.
	void ResetWaitEdges();

	// Get number of child arcs
	size_t ArcCount() const;
	size_t NumEdges() const { return m_edges.size(); }
	size_t NumWaitEdges() const { return m_wait_edges.size(); }
	size_t size() const { return m_edges.size() + m_wait_edges.size(); }
private:
	std::vector<Edge> m_edges{ {/*Direct Arcs*/} };
	std::vector<Edge> m_wait_edges{};
};
