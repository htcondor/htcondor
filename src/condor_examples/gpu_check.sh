#!/bin/bash
# This script runs on the execute node.
# It prints the CUDA_VISIBLE_DEVICES environment variable
# to show which GPU HTCondor assigned to the job.
#
# This script does not perform any real GPU computation.

echo "=== HTCondor GPU Job Check ==="
echo "Hostname: $(hostname)"
echo "Date: $(date)"

echo
echo "CUDA_VISIBLE_DEVICES = $CUDA_VISIBLE_DEVICES"
echo

# If NVIDIA utilities are available, show GPU info
if command -v nvidia-smi >/dev/null 2>&1; then
    echo "nvidia-smi output:"
    nvidia-smi
else
    echo "nvidia-smi not found (this is OK on non-NVIDIA systems)"
fi

echo
echo "Job completed successfully."
