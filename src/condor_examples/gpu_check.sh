#!/bin/bash
#
# gpu_check.sh
#
# This script is executed on the HTCondor execute node.
# It demonstrates how GPU assignments are exposed
# to the job environment.

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
