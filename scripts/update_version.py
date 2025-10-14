#!/usr/bin/env python3
"""
Update version across all CNanoLog files.

Usage:
    python scripts/update_version.py <new_version>

Example:
    python scripts/update_version.py 1.1.0
"""

import sys
import json
import os
from pathlib import Path


def update_version_file(new_version: str):
    """Update the VERSION file"""
    version_file = Path("VERSION")
    version_file.write_text(new_version)
    print(f"✓ Updated VERSION: {new_version}")


def update_vcpkg_json(new_version: str):
    """Update vcpkg.json version-string"""
    vcpkg_file = Path("vcpkg.json")

    if not vcpkg_file.exists():
        print("⚠ vcpkg.json not found, skipping")
        return

    with open(vcpkg_file, 'r') as f:
        data = json.load(f)

    data["version-string"] = new_version

    with open(vcpkg_file, 'w') as f:
        json.dump(data, f, indent=2)
        f.write('\n')  # Add newline at end

    print(f"✓ Updated vcpkg.json: {new_version}")


def update_portfile_cmake(new_version: str):
    """Update portfile.cmake REF"""
    portfile = Path("portfile.cmake")

    if not portfile.exists():
        print("⚠ portfile.cmake not found, skipping")
        return

    content = portfile.read_text()

    # Update REF line
    lines = content.split('\n')
    for i, line in enumerate(lines):
        if 'REF v${VERSION}' in line:
            print(f"✓ portfile.cmake already uses v${{VERSION}} (good!)")
            return
        elif line.strip().startswith('REF'):
            lines[i] = f'    REF v{new_version}'
            print(f"✓ Updated portfile.cmake REF: v{new_version}")
            break

    portfile.write_text('\n'.join(lines))


def update_documentation(new_version: str):
    """Update version references in documentation"""
    docs = {
        "README.md": [
            (r"cnanolog/\d+\.\d+\.\d+", f"cnanolog/{new_version}"),
            (r"v\d+\.\d+\.\d+", f"v{new_version}"),
        ],
        "CONAN.md": [
            (r"cnanolog/\d+\.\d+\.\d+", f"cnanolog/{new_version}"),
        ],
        "VCPKG.md": [
            (r"v\d+\.\d+\.\d+", f"v{new_version}"),
        ],
    }

    for doc_file, replacements in docs.items():
        doc_path = Path(doc_file)
        if not doc_path.exists():
            print(f"⚠ {doc_file} not found, skipping")
            continue

        content = doc_path.read_text()
        modified = False

        for pattern, replacement in replacements:
            import re
            if re.search(pattern, content):
                content = re.sub(pattern, replacement, content)
                modified = True

        if modified:
            doc_path.write_text(content)
            print(f"✓ Updated {doc_file}")


def main():
    if len(sys.argv) != 2:
        print("Usage: python scripts/update_version.py <new_version>")
        print("Example: python scripts/update_version.py 1.1.0")
        sys.exit(1)

    new_version = sys.argv[1].strip()

    # Validate version format
    import re
    if not re.match(r'^\d+\.\d+\.\d+$', new_version):
        print(f"Error: Invalid version format '{new_version}'")
        print("Expected format: X.Y.Z (e.g., 1.0.0)")
        sys.exit(1)

    print(f"\nUpdating CNanoLog version to {new_version}\n")
    print("=" * 50)

    # Update all files
    update_version_file(new_version)
    update_vcpkg_json(new_version)
    update_portfile_cmake(new_version)
    update_documentation(new_version)

    print("=" * 50)
    print(f"\n✓ All files updated to version {new_version}")
    print("\nNext steps:")
    print("1. Review changes with: git diff")
    print("2. Test the build: cmake -B build && cmake --build build")
    print("3. Commit: git commit -am 'Bump version to {new_version}'")
    print(f"4. Tag: git tag v{new_version}")
    print("5. Push: git push origin master --tags")


if __name__ == "__main__":
    main()
