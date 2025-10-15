#!/bin/bash
# Update version across all CNanoLog files
#
# Usage:
#     ./scripts/update_version.sh <new_version>
#
# Example:
#     ./scripts/update_version.sh 1.1.0

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

update_version_file() {
    local new_version="$1"
    echo "$new_version" > "$PROJECT_ROOT/VERSION"
    print_success "Updated VERSION: $new_version"
}

update_vcpkg_json() {
    local new_version="$1"
    local vcpkg_file="$PROJECT_ROOT/vcpkg.json"

    if [ ! -f "$vcpkg_file" ]; then
        print_warning "vcpkg.json not found, skipping"
        return
    fi

    # Update version-string in JSON
    # Use a temporary file to avoid issues with in-place editing
    if command -v jq >/dev/null 2>&1; then
        # Use jq if available (better JSON handling)
        jq ".\"version-string\" = \"$new_version\"" "$vcpkg_file" > "$vcpkg_file.tmp"
        mv "$vcpkg_file.tmp" "$vcpkg_file"
    else
        # Fallback to sed (works but less robust for complex JSON)
        sed -i.bak "s/\"version-string\": \"[^\"]*\"/\"version-string\": \"$new_version\"/" "$vcpkg_file"
        rm -f "$vcpkg_file.bak"
    fi

    print_success "Updated vcpkg.json: $new_version"
}

update_portfile_cmake() {
    local new_version="$1"
    local portfile="$PROJECT_ROOT/portfile.cmake"

    if [ ! -f "$portfile" ]; then
        print_warning "portfile.cmake not found, skipping"
        return
    fi

    # Check if it already uses v${VERSION}
    if grep -q 'REF v${VERSION}' "$portfile"; then
        print_success "portfile.cmake already uses v\${VERSION} (good!)"
        return
    fi

    # Update REF line
    sed -i.bak "s/^\([[:space:]]*REF\).*/\1 v$new_version/" "$portfile"
    rm -f "$portfile.bak"

    print_success "Updated portfile.cmake REF: v$new_version"
}

update_documentation() {
    local new_version="$1"

    # README.md: cnanolog/X.Y.Z and vX.Y.Z
    if [ -f "$PROJECT_ROOT/README.md" ]; then
        sed -i.bak \
            -e "s/cnanolog\/[0-9]\+\.[0-9]\+\.[0-9]\+/cnanolog\/$new_version/g" \
            -e "s/v[0-9]\+\.[0-9]\+\.[0-9]\+/v$new_version/g" \
            "$PROJECT_ROOT/README.md"
        rm -f "$PROJECT_ROOT/README.md.bak"
        print_success "Updated README.md"
    else
        print_warning "README.md not found, skipping"
    fi

    # CONAN.md: cnanolog/X.Y.Z
    if [ -f "$PROJECT_ROOT/docs/CONAN.md" ]; then
        sed -i.bak \
            -e "s/cnanolog\/[0-9]\+\.[0-9]\+\.[0-9]\+/cnanolog\/$new_version/g" \
            "$PROJECT_ROOT/docs/CONAN.md"
        rm -f "$PROJECT_ROOT/docs/CONAN.md.bak"
        print_success "Updated CONAN.md"
    else
        print_warning "CONAN.md not found, skipping"
    fi

    # VCPKG.md: vX.Y.Z
    if [ -f "$PROJECT_ROOT/docs/VCPKG.md" ]; then
        sed -i.bak \
            -e "s/v[0-9]\+\.[0-9]\+\.[0-9]\+/v$new_version/g" \
            "$PROJECT_ROOT/docs/VCPKG.md"
        rm -f "$PROJECT_ROOT/docs/VCPKG.md.bak"
        print_success "Updated VCPKG.md"
    else
        print_warning "VCPKG.md not found, skipping"
    fi
}

main() {
    # Check arguments
    if [ $# -ne 1 ]; then
        echo "Usage: $0 <new_version>"
        echo "Example: $0 1.1.0"
        exit 1
    fi

    local new_version="$1"

    # Validate version format (X.Y.Z)
    if ! echo "$new_version" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
        print_error "Invalid version format '$new_version'"
        echo "Expected format: X.Y.Z (e.g., 1.0.0)"
        exit 1
    fi

    echo ""
    echo "Updating CNanoLog version to $new_version"
    echo ""
    echo "=================================================="

    # Change to project root
    cd "$PROJECT_ROOT"

    # Update all files
    update_version_file "$new_version"
    update_vcpkg_json "$new_version"
    update_portfile_cmake "$new_version"
    update_documentation "$new_version"

    echo "=================================================="
    echo ""
    print_success "All files updated to version $new_version"
    echo ""
    echo "Next steps:"
    echo "1. Review changes with: git diff"
    echo "2. Test the build: cmake -B build && cmake --build build"
    echo "3. Commit: git commit -am 'Bump version to $new_version'"
    echo "4. Tag: git tag v$new_version"
    echo "5. Push: git push origin master --tags"
}

main "$@"
