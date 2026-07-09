#!/bin/bash
# docker-build.sh
# Performs the build inside the pico-sdk Docker container.
#
# Usage:
#   ./docker-build.sh [target]
#
# Examples:
#   ./docker-build.sh          # Build both Pico 1 and Pico 2
#   ./docker-build.sh pico1    # Build only Pico 1
#   ./docker-build.sh pico2    # Build only Pico 2
#   ./docker-build.sh clean    # Clean up build artifacts

set -e

# Resolve directory of this script to support running from anywhere
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Run make inside the pico-sdk docker container
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "${SCRIPT_DIR}:/project" \
    -w /project \
    lukstep/raspberry-pi-pico-sdk:latest \
    make "$@"
