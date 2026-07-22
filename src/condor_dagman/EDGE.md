# DAG Edges: `edge.h` / `edge.cpp`

This explains how DAGMan stores and uses parent/child relationships
between nodes. The code is dense (bit tricks, negative IDs, copy-on-write)
because it's optimized for DAGs with huge fan-out/fan-in, so this doc
walks through *why* it looks the way it does, not just what each function
does.

For the DAG-file `PARENT`/`CHILD` syntax itself, see
`docs/automated-workflows/dagman-reference.rst`. This doc is about the
internal representation.

## The problem this solves

Before this code existed, every `Node` kept its own `std::vector<Node*>`
of parents and children. That's simple, but wasteful: in a DAG where
10,000 nodes all share the same single parent, or a `PARENT p1 p2 CHILD
c1..c1000` statement connects two big groups, you end up duplicating the
same list of IDs over and over, and building it one arc at a time is slow.

The fix is to store relationships in one shared table (`Dag::edge_table`)
instead of inside each node, so a group of nodes that share the same
parents or children can share the same storage.

## Vocabulary

- **`node_id_t`** — a node's unique ID (handed out by a global counter, so
  IDs are unique even across splices, which are temporarily separate `Dag`
  objects).
- **`edge_id_t`** — an ID that refers to a place in the edge table. It can
  be positive, negative, or zero, and each of those means something
  different (explained below).
- **`DagArc`** — one directed connection: `{ node_id_t id; unsigned int metadata; }`.
  Think of it as "an arrow to this node, with some flags on it."
- **`Edge`** — just a `std::vector<DagArc>`, plus a reference count and a
  "how many am I still waiting on" counter.
- **`EdgeTable`** — owns every `Edge` that exists in the DAG.

`Dag::edge_table` is `static`, meaning there's really only one of these
per process, shared by the main DAG and all its splices. That's necessary
because node IDs are global, so an edge built while parsing a splice still
makes sense after the splice is merged into the parent DAG.

## Two directions, two tables

Every parent/child relationship gets recorded in two different places,
because the two directions get used in different situations:

1. **Children edges** — "who are my children?" Stored on the parent, in
   `Node::m_children`. Used when a node finishes and needs to notify (or
   invalidate) whoever depends on it.
2. **Wait edges** — "who are my parents, and how many are left?" Stored on
   the child, in `Node::m_parents` (with `Node::m_multiple_parents` saying
   how to interpret it). Used when a child needs to know if it's still
   blocked.

These are never the same underlying `Edge` object, even though they
describe the same logical dependency — one is a fan-out list living on
the parent, the other is a "still waiting on N things" list living on the
child. Only children edges get shared between nodes (below); each
multi-parent child gets its own private wait edge, so wait edges never
need copy-on-write.

### Children edges: don't pay for a vector if you don't need one

Most nodes have exactly one child. Allocating a whole `std::vector` just
to hold one arc would be wasteful, so there's a special case:

- `EdgeTable` keeps one big `Edge` called the **direct-arc pool**
  (`m_edges[0]`). A node with a single child gets one slot in this pool
  instead of its own `Edge`.
- That node's `m_children` stores a **negative** number that encodes
  "which slot in the pool." `EdgeTable::IsDirect(id)` is just `id < 0`.
- A **positive** `m_children` value means "a real, possibly shared `Edge`,
  look it up by this ID."
- `0` (`NO_EDGE_ID`) means "no children at all."

If a node with one child later gains a second child, its single arc gets
copied out of the pool into a real `Edge` (`EdgeTable::PromoteDirect`),
and the pool slot is marked empty. Empty slots aren't cleaned up
immediately — that happens once, in `Dag::AdjustEdges()`, after all
parsing is done (see below), because removing a slot early would shift
everyone else's slot numbers out from under them.

## Copy-on-write: why shared children edges need it

A `PARENT p1 p2 p3 CHILD c1 c2 c3` statement gives `p1`, `p2`, and `p3`
the exact same list of children. Instead of storing that list three
times, `Connect()` builds it **once** as a single `Edge` and points all
three parents' `m_children` at the same `edge_id_t`. `Edge::m_references`
counts how many parents currently point at it.

The danger with sharing storage is obvious: if a later DAG-file statement
adds a child to `p2` alone, and `p2`'s `Edge` is mutated in place, `p1`
and `p3` would silently gain that child too, even though the file never
said so. Copy-on-write is what prevents that:

- Before modifying a children `Edge`, `Connect()` checks
  `GetRefCount()` against how many parents from *this* statement are
  pointing at it.
- If every reference to the `Edge` belongs to parents in the current
  statement (nobody outside this group can see it), it's safe to mutate
  in place — no observer would be surprised by the change.
- If some other parent *outside* this statement also shares the `Edge`,
  `Connect()` clones it first (`EdgeTable::NewEdge(&existing)` makes a
  copy with its own fresh `edge_id_t` and `m_references` reset to 0), then
  moves only the parents from the current statement over to the clone
  (`--` the old edge's refcount, `++` the new one's) before modifying the
  clone. The original `Edge` is untouched, so the parents that still point
  at it see no change.

This happens in two places in `Dag::Connect()`, for the same reason but
different shapes of input:

- The **shared-parent-group fast path** (all parents in this statement
  already agree on the same children edge) — one clone-or-mutate decision
  covers the whole group at once.
- The **general per-parent loop** (parents handled one at a time) — the
  `GetRefCount() > 1` check there is the same idea applied to a single
  parent's edge.

The upshot: children edges behave like an immutable, shared, reference
counted list. Multiple parents point at it as long as nobody needs to
diverge; the moment one of them needs a different set of children, it
quietly gets its own private copy and the rest are unaffected.

### Wait edges: a simple countdown

A child with exactly one parent just stores that parent's ID directly —
no `Edge` needed at all (`Node::HasSingleParent()`). A child with more
than one parent gets a wait edge: an `Edge` whose arcs are its parents,
and whose "waiting count" ticks down by one each time a parent finishes
(`Edge::MarkDone()`). As noted above, wait edges are never shared between
nodes, so there's no copy-on-write concern here.

## The metadata bits

`DagArc::metadata` is a small bitmask with two flags that are each only
meaningful on one of the two edge types above:

- **`ARC_DONE`** — only meaningful on wait-edge arcs. Set once that
  particular parent has finished.
- **`ARC_WEAK`** — only meaningful on children-edge arcs. Marks a `WEAK`
  dependency, where a child only needs its parent to finish *running*
  rather than to succeed (see `dagman-reference.rst` for the full
  semantics). The one implementation detail worth knowing here: if the
  same parent/child pair is declared both weak and strong (in either
  order), it always ends up strong — a strong declaration clears an
  existing weak flag, but a weak declaration never clears an existing
  strong one. This "strongest wins" rule is applied everywhere an arc
  might be re-declared: `Edge::AddArc()`, the direct-arc same-child case
  in `Dag::Connect()`, and the shared-parent-group fast path in `Connect()`.

## How `Dag::Connect()` builds edges

`Connect(parents, children, meta)` is called once per `PARENT ... CHILD
...` statement. By the time it's called, `parents` and `children` have
already been deduplicated and sorted by the caller — that guarantee is
what lets several branches below skip an expensive "does this arc already
exist?" scan.

Roughly, for each parent, `Connect()` has to decide what its `m_children`
should become:

- **No children yet, exactly one child** → grab a slot in the direct-arc
  pool. Cheapest case, no `Edge` allocated at all.
- **No children yet, multiple children** → allocate a fresh `Edge`.
- **Already has one child (direct arc), adding the same child again** →
  nothing to do but maybe upgrade weak to strong.
- **Already has one child, adding a different child** → promote the
  direct arc into a real `Edge` first, then add to it.
- **Already has a shared `Edge`, but other nodes also point at it** →
  copy-on-write (see above): make a private copy before modifying it.
- **Already has an `Edge` that's only referenced by this parent (or this
  whole parent group)** → just modify it in place.

There's also a fast path for the common case where a whole group of
parents (e.g. `PARENT p1 p2 p3 CHILD ...`) already shares one `Edge`, or
none of them has any children yet — instead of looping node by node, the
group is handled as a single batch, since they're all going to end up
pointing at the same `Edge` anyway.

Whichever case applies, adding the new children/parents themselves uses
one of two vector operations: a plain append when the target is
known-empty (nothing to collide with), or an "add-or-upgrade" scan when
the target might already contain some of these arcs (existing edge that
could have the same child from an earlier statement, needs the
strongest-wins check).

## `Dag::AdjustEdges()`: cleanup after parsing

Once the whole DAG file (and all its splices) has been fully parsed and
every `Connect()` call has run, `AdjustEdges()` does two housekeeping
passes:

1. Squeezes out the empty slots left behind in the direct-arc pool by
   promotions, and fixes up every node's `m_children` to point at the new,
   compacted slot numbers. (Real `Edge`s aren't touched by this — only the
   direct pool gets compacted, since positive edge IDs need to stay stable
   for the life of the `Dag`.)
2. Initializes each wait edge's "how many parents am I waiting on" counter,
   now that parsing is done and the final parent count for each child is
   known. This can't happen incrementally while parsing, because a wait
   edge might still gain more parents from a later `PARENT`/`CHILD`
   statement.

## Walking the graph at runtime

A handful of `Node` methods (in `node.cpp`) all follow the same shape:
check whether `m_children` is a direct arc or a real `Edge`, then either
handle the one child or loop over the `Edge`'s arcs. If you need to add a
new way of walking children, one of these is a good template:

- **`VisitChildren`** — generic "call this function for each child," used
  for cycle detection, DFS ordering, and `.dot` graph export.
- **`NotifyChildren`** — "I finished successfully, tell my children,"
  used to unblock children whose last outstanding parent was this node.
- **`SetDescendantsToFutile`** / **`CascadeFutile`** — failure-propagation:
  when a node fails, its children are invalidated (`STATUS_FUTILE`) and
  that spreads to their descendants, except that a weak child is treated
  as unblocked instead of futile, since it only needed its parent to run.
- **`AllChildrenWeak`** — true when a node's children are all weak
  dependencies; used to decide whether a failed node needs to re-run in a
  rescue DAG.
- **`PrintParents`/`PrintChildren`** — just produce name lists for
  status/log output; the simplest possible example if you want to see the
  minimal code needed to walk both a wait edge and a children edge.

## Things to keep in mind if you touch this code

- `0` always means "no edge." Direct arcs are negative, real `Edge`s are
  positive — don't mix these up.
- Never mutate a children `Edge` without checking `GetRefCount()` first —
  if it's shared by parents outside the group you're modifying, clone it
  instead (see "Copy-on-write" above).
- The plain-append operations (skip the duplicate check) are only safe
  when you can prove the thing you're adding isn't already there — either
  because the `Edge` is brand new, or because the input list was already
  deduplicated against it. If you add a new bulk-insert path, make sure
  that guarantee still holds.
- Don't initialize the wait-edge "waiting count" until every
  `PARENT`/`CHILD` statement has been processed — doing it early will
  under-count parents that get added afterward.
- Because `edge_table` is shared process-wide, there's an implicit
  assumption that only one DAG run is being built at a time in a given
  process.
