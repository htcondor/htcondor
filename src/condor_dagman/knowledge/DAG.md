# Generic DAG Container: `dag.hpp`

`Dag<D, N>` / `Node<N>` is a small, reusable templated DAG container: nodes
carrying caller-supplied metadata, connected via parent/child edges. It
carries no DAGMan runtime state (job tracking, scripts, retries, ...) --
just structure, so it can be reused anywhere that needs "a DAG of things
with parent/child relationships." Built on `EdgeTable`/`Edge`/`DagArc` --
read `EDGE.md` first for that layer.

## Vocabulary

- **`Node<N>`** -- one node. `N` is caller-supplied metadata (a job's data,
  a task's data, whatever you need); `Node<N>` also carries the id/edge
  bookkeeping needed to connect and walk it.
- **`Dag<D, N>`** -- the container: caller-supplied metadata `D`, plus a
  collection of `Node<N>` and the `EdgeTable` connecting them.
- **`node_id_t`** -- a node's stable index into `Dag<D, N>`'s backing
  storage (same type as in `edge.h`).

## Building the graph

```cpp
struct JobData { std::string name; };

Dag<std::string, JobData> dag; // D = std::string, N = JobData
dag.data = "my workflow";

node_id_t a = dag.AddNode(JobData{"A"});
node_id_t b = dag.AddNode(JobData{"B"});
node_id_t c = dag.AddNode(JobData{"C"});

auto [ok, failed_id, why] = dag.Connect({a}, {b, c}); // A -> B, A -> C
dag.Connect({b, c}, {a}, ARC_WEAK); // B,C -> A, weakly (ARC_WEAK is from edge.h)
```

**`AddNode(...)` returns a `node_id_t`, not a `Node<N>&`.** A reference
would be invalidated by a later `AddNode()` call that reallocates the
backing storage. Hold ids across any `AddNode()` call you don't control the
timing of, and look the node back up when you need it. Call
`dag.Reserve(n)` first if you need to hold a `Node<N>*`/`&` across a
known-size batch of `AddNode()` calls.

Look nodes up by id via:
- **`operator[]`** -- throws `std::out_of_range` on an invalid id.
- **`FindNode(id)`** -- bounds-checked, returns `nullptr` instead of throwing.
- **`contains(id)`** -- bounds check only, no dereference.

`NumNodes()`/`size()` give the total node count; `begin()`/`end()` iterate
every node regardless of connectivity (see "Walking the graph" below for
the reachability-limited alternative).

## `Connect(parents, children, meta = 0)`

Wires a group of parents to a group of children by id:

- A direct-arc pool for a single parent/single child.
- A shared `Edge` + copy-on-write for everything else.
- "Strongest wins" for `ARC_WEAK`: a strong re-declaration upgrades an
  existing weak arc; a weak one never downgrades an existing strong one.

Returns `std::tuple<bool, node_id_t, std::string>`:
- `{true, NO_ID, ""}` on success.
- `{false, NO_ID, reason}` if `parents`/`children` is empty.
- `{false, failed_id, reason}` if a `CanAddChild()`/`CanAddParent()` policy
  hook rejects the request -- `failed_id` is the specific node that
  rejected it.

No node-type validation (see the policy hook below for a generic
replacement). Assumes `parents`/`children` are already deduplicated by the
caller. No logging of its own -- the returned message is the only way to
find out why a call failed.

## Policy hook: `CanAddChild()` / `CanAddParent()`

`Connect()` calls `Node<N>::CanAddChild()`/`CanAddParent()` on every
parent/child before wiring anything, failing the whole call if either
returns false. Defaults to `true` (unrestricted). Opt in by defining either
on `N`:

```cpp
struct RestrictedData {
    bool sealed{false};
    bool CanAddChild() const { return !sealed; } // once sealed, no new children
    // no CanAddParent() defined -- defaults to true
};
```

## Parent-reference hook: `SetParent()`

Gives `N` and/or `D` a live reference back to their own `Dag<D, N>`, instead
of threading it through every call. Define `void SetParent(Dag<D, N>&)` on
`N` and/or `D`; `AddNode()`/the constructor calls it once, automatically.
Optional -- nothing breaks if you don't define it.

```cpp
struct MyNodeData {
    std::string name;
    Dag<MyDagData, MyNodeData>* parent{nullptr};
    void SetParent(Dag<MyDagData, MyNodeData>& d) { parent = &d; }
};
```

Once stored, the full `Dag<D, N>` public API (`AddNode()`/`Connect()`/
`GetEdgeTable()`/`operator[]`/`begin()`/`end()`/...) is reachable through
that pointer/reference, same as any other caller holding one.

**Gotcha:** if `SetParent()`'s body does more than store the pointer (calls
a method on `d`, etc.), it can't be defined *inline* in `N`'s/`D`'s own
class -- `Dag<D, N>` isn't complete there yet. Two ways around it:

- **Use `std::addressof(d)` instead of `&d`** if all you need is to store
  the pointer -- works even while `Dag<D, N>` is incomplete, no out-of-line
  definition needed.
- **Otherwise, declare it in the class and define the body out-of-line**,
  once `Dag<D, N>` is complete:

```cpp
struct MyDagData;
struct MyNodeData {
    Dag<MyDagData, MyNodeData>* parent{nullptr};
    void SetParent(Dag<MyDagData, MyNodeData>& d); // declared here...
};
struct MyDagData { /* ... */ };
// ...defined here, once Dag<MyDagData, MyNodeData> is complete.
inline void MyNodeData::SetParent(Dag<MyDagData, MyNodeData>& d) { parent = &d; }
```

## Walking the graph

- **`VisitChildren(node, fn)`** -- calls `fn(dag, parent, child)` once per
  child, summing return values. Never looks at `N`/`D`/`ARC_WEAK` -- pure
  structure.
- **`Walk(action, order = WalkOrder::BFS)`** -- visits every node reachable
  from the roots (parentless nodes), calling `action(dag, node)` once each,
  in BFS or DFS order.
- **`Walk(action, order)`**, where `action` takes `(Dag&, Node<N>&, size_t
  depth)` -- same, but also passes each node's depth (hops from its root).
  True shortest-path depth for BFS. For DFS, children are visited in
  ascending id order, so a re-converging node's recorded depth is
  deterministic regardless of `Connect()` call order.
- **`Cycle(path = nullptr)`** -- detects a cycle via an ancestor-stack DFS,
  also catching a fully disjoint cycle (no parentless entry point). If
  `path` is non-null, it's filled with the back-edge path (e.g.
  `[A, B, C, A]`) when found via the root-driven walk; left untouched
  otherwise (acyclic, or a disjoint cycle with no path to report).

```cpp
std::vector<node_id_t> path;
bool has_cycle = dag.Cycle(&path);

dag.Walk([](Dag<std::string, JobData>& dag, Node<JobData>& node) {
    printf("visiting %s\n", node.data.name.c_str());
});

dag.Walk([](Dag<std::string, JobData>& dag, Node<JobData>& node, size_t depth) {
    printf("visiting %s at depth %zu\n", node.data.name.c_str(), depth);
});
```

`Walk()`/`Cycle()` only see nodes reachable from a root -- a disconnected or
fully-cyclic subgraph is invisible to them. For every node regardless of
connectivity, use `begin()`/`end()`:

```cpp
for (auto& node : dag) { printf("%s\n", node.data.name.c_str()); }
```

## Arc metadata: `GetEdgeTable()`

`ARC_WEAK` marks a child that only needs its parent to *run*, not succeed;
`ARC_DONE` marks a finished wait-edge parent. Neither is exposed by
`VisitChildren()`/`Walk()`/`Cycle()` -- get it from the `EdgeTable` directly:

```cpp
edge_id_t ce = dag[parent_id].GetChildEdge();
bool weak = EdgeTable::IsDirect(ce)
    ? dag.GetEdgeTable().GetDirectArc(ce).IsWeak()
    : dag.GetEdgeTable()[ce].GetArc(child_id).IsWeak(); // throws std::out_of_range if not connected
```

## Notes

- **Throws instead of aborting.** No `EXCEPT()`/`ASSERT()` --
  `std::out_of_range` for an invalid id, `std::logic_error` for an internal
  invariant violation.
- **No dependency on `condor_common.h`/`condor_debug.h`** -- usable outside
  a running DAGMan process.
- **`edge_table` is a per-`Dag<D, N>` instance member**, not shared/static.
