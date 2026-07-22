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

#include "condor_common.h"
#include "condor_debug.h"
#include "edge.h"

#include <algorithm>
#include <tuple>
#include <ranges>
#include <numeric>

size_t
Edge::AddArc(const DagArc& arc) {
	return AddArc(arc.id, arc.metadata);
}


size_t
Edge::AddArc(const node_id_t id, const unsigned int meta) {
	ASSERT(id != NO_ID);
	auto it = std::ranges::find(m_arcs, id, &DagArc::id);

	if (it == m_arcs.end()) {
		m_arcs.emplace_back(id, meta);
		return m_arcs.size() - 1;
	}

	// Strongest-wins: a strong (re-)declaration upgrades an existing weak arc.
	// A weak (re-)declaration never downgrades an existing strong arc.
	if ( ! (meta & ARC_WEAK) && (it->metadata & ARC_WEAK)) {
		it->metadata &= ~ARC_WEAK;
	}

	return static_cast<size_t>(it - m_arcs.begin());
}


size_t
Edge::AppendArc(const node_id_t id, const unsigned int meta) {
	size_t idx = m_arcs.size();
	m_arcs.emplace_back(id, meta);
	return idx;
}


DagArc&
Edge::GetArc(const node_id_t id) {
	auto it = std::ranges::find(m_arcs, id, &DagArc::id);

	if (it != m_arcs.end()) {
		return *it;
	}

	EXCEPT("Invalid node id provided");
}


bool
Edge::Contains(const node_id_t id) {
	auto it = std::ranges::find(m_arcs, id, &DagArc::id);
	return it != m_arcs.end();
}


bool
Edge::MarkDone(node_id_t parent_id) {
	auto it = std::ranges::find(m_arcs, parent_id, &DagArc::id);
	if (it == m_arcs.end()) { return false; }
	if ( ! (it->metadata & ARC_DONE)) {
		it->metadata |= ARC_DONE;
		if (m_waiting > 0) { --m_waiting; }
		return m_waiting == 0;
	}
	return false; // already marked done, no new completion event
}


void
Edge::Reset() {
	for (auto& arc : m_arcs) { arc.metadata &= ~ARC_DONE; }
	m_waiting = static_cast<uint32_t>(m_arcs.size());
}


edge_id_t
EdgeTable::NewEdge(const Edge* duplicate) {
	size_t idx = m_edges.size();

	if (duplicate) { // Copy-On-Write (COW) is occurring
		auto& edge = m_edges.emplace_back(*duplicate);
		edge.SetRefCount(0);
		++edge;
	} else { // Actually new up an edge
		m_edges.emplace_back();
	}

	return static_cast<edge_id_t>(idx);
}


Edge&
EdgeTable::GetEdge(const edge_id_t id) {
	if (id < 0) {
		return m_edges[0];
	} else if (id > 0) {
		return m_edges[id];
	}

	EXCEPT("Invalid edge id provided");
}


bool
EdgeTable::IsDirect(const edge_id_t id) {
	return id < 0;
}


size_t
EdgeTable::DirectIdToOffset(edge_id_t id) {
	ASSERT(EdgeTable::IsDirect(id));
	return static_cast<size_t>((id + 1) * -1);
}


edge_id_t
EdgeTable::DirectOffsetToId(size_t idx) {
	return (static_cast<edge_id_t>(idx) * -1) - 1;
}


edge_id_t
EdgeTable::AddDirectArc(const node_id_t id, const unsigned int meta) {
	ASSERT(id != NO_ID);
	size_t idx = m_edges[0].AppendArc(id, meta);
	return EdgeTable::DirectOffsetToId(idx);
}


DagArc&
EdgeTable::GetDirectArc(const edge_id_t id) {
	ASSERT(EdgeTable::IsDirect(id));
	size_t idx = EdgeTable::DirectIdToOffset(id);
	return m_edges[0][idx];
}


edge_id_t
EdgeTable::NewWaitEdge() {
	edge_id_t id = static_cast<edge_id_t>(m_wait_edges.size()) + 1;
	m_wait_edges.emplace_back();
	return id;
}


Edge&
EdgeTable::GetWaitEdge(const edge_id_t id) {
	ASSERT(id >= 1 && id <= static_cast<edge_id_t>(m_wait_edges.size()));
	return m_wait_edges[id - 1];
}


std::vector<size_t>
Edge::CompactPool() {
	std::vector<size_t> mapping(m_arcs.size(), SIZE_MAX);
	std::vector<DagArc> compacted;
	for (size_t i = 0; i < m_arcs.size(); ++i) {
		if (m_arcs[i].id != NO_ID) {
			mapping[i] = compacted.size();
			compacted.push_back(m_arcs[i]);
		}
	}
	m_arcs = std::move(compacted);
	return mapping;
}


std::vector<size_t>
EdgeTable::CompactDirectPool() {
	return m_edges[0].CompactPool();
}


void
EdgeTable::ResetWaitEdges() {
	for (auto& e : m_wait_edges) { e.Reset(); }
}


edge_id_t
EdgeTable::PromoteDirect(const edge_id_t id) {
	DagArc& direct = GetDirectArc(id);
	edge_id_t new_eid = NewEdge();
	Edge& edge = GetEdge(new_eid);
	std::ignore = edge.AddArc(direct);
	++edge;
	direct.id = NO_ID;
	return new_eid;
}


size_t
EdgeTable::ArcCount() const {
	// Get all non-direct edge arc counts (num arcs * number of parents)
	auto view = m_edges
	          | std::views::drop(1)
	          | std::views::transform([](const auto& edge) { return edge.size() * edge.GetRefCount(); });

	// Total up all counts including all direct arcs (i.e. Edge 0)
	return std::accumulate(view.begin(), view.end(), size_t{0}) + m_edges[0].size();
}
