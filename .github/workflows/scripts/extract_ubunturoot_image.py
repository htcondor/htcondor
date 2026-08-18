#!/usr/bin/env python3
"""
Extract the docker image tagged 'ubunturoot' from nmi_tools/nmi-build-platforms.
Outputs the image name (without docker:// prefix) for use in GitHub Actions.
"""

import sys


def main():
    csv_path = 'nmi_tools/nmi-build-platforms'

    with open(csv_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            parts = [p.strip() for p in line.split(',')]
            if len(parts) < 4:
                continue

            tags = parts[3].split(':')
            if 'ubunturoot' in tags:
                image = parts[0]
                if image.startswith('docker://'):
                    image = image[9:]
                print(image)
                return 0

    print("Error: No entry with 'ubunturoot' tag found", file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
