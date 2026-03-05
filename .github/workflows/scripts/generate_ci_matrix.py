#!/usr/bin/env python3
"""
Generate GitHub Actions matrix from nmi-build-platforms CSV file.
Reads nmi_tools/nmi-build-platforms and outputs the matrix configuration
for .github/workflows/ci-build-test.yml as JSON.
"""

import sys
import json


def parse_csv_file(csv_path):
    """Parse the nmi-build-platforms CSV file and return matrix entries."""
    matrix_entries = []
    
    with open(csv_path, 'r') as f:
        for line in f:
            line = line.strip()
            
            # Skip comments and blank lines
            if not line or line.startswith('#'):
                continue
            
            # Parse CSV line
            parts = [p.strip() for p in line.split(',')]
            if len(parts) < 3:
                continue
            
            image_raw = parts[0]
            abbrev = parts[1]
            vm = parts[2]
            
            # Skip batlab entries
            if vm.lower() == 'batlab':
                continue
            
            # Extract container image from docker:// prefix
            if image_raw.startswith('docker://'):
                container = image_raw[9:]  # Remove 'docker://' prefix
                
                matrix_entries.append({
                    'img': vm,
                    'abbrev': abbrev,
                    'container': container
                })
    
    return matrix_entries


def main():
    csv_path = 'nmi_tools/nmi-build-platforms'
    
    try:
        matrix_entries = parse_csv_file(csv_path)
        
        if not matrix_entries:
            print("Warning: No valid entries found in CSV file", file=sys.stderr)
            return 1
        
        # Output as JSON for GitHub Actions
        print(json.dumps({'instance': matrix_entries}))
        
        return 0
        
    except FileNotFoundError:
        print(f"Error: CSV file not found at {csv_path}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
