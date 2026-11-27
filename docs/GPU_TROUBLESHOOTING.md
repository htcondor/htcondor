# Troubleshooting GPU Allocation and `START=false`

This guide addresses two common points of confusion for users running GPU jobs on HTCondor pools.

## Transient `START=false` on GPU Nodes

When querying a GPU node with `condor_status -server`, you may occasionally see `START = false`. This does **not** necessarily mean the node is drained or in maintenance. It is often a transient state related to **slot retirement**. When another user's job is finishing or being evicted, the slot briefly enters a state where it cannot accept new jobs. Running `condor_q -better-analyze` will typically report `1 slots would match if drained`, indicating that the slot will become available once the current job completes. 

**Recommendation:** Do not assume the node is down. Wait for the current job to finish, or target a different node.

## Targeting GPUs by Compute Capability

Hardcoding specific machine names (e.g., `Machine == "matlab1"`) is fragile. A more robust approach is to use the `GPUs_Capability` attribute. For example, to target modern GPUs like the L40S (capability 8.9) or H100 (capability 9.0), you can use:

```condor
+Production = False
requirements = (GPUs_Capability >= 8)
This ensures your job runs on any node with a sufficiently capable GPU, increasing the chances of a match.
