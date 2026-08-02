#!/bin/sh
# Run make inside the BlocksDS toolchain container against the working tree, so
# builds are incremental and artifacts land directly in this directory.
#
#   ./docker-build.sh              # build portalDS.nds
#   ./docker-build.sh clean
#   ./docker-build.sh dldipatch
#   ./docker-build.sh sh           # interactive shell in the toolchain
#
# Set BLOCKSDS_IMAGE to pin a different toolchain version.

set -eu

IMAGE="${BLOCKSDS_IMAGE:-skylyrac/blocksds:slim-latest}"
PROJECT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

if [ "${1-}" = "sh" ] || [ "${1-}" = "bash" ]; then
	set -- "$@"
	TTY_FLAGS="-it"
else
	TTY_FLAGS=""
	set -- make -j"$(getconf _NPROCESSORS_ONLN)" "$@"
fi

# Run as the invoking user so the ROM and build/ aren't owned by root.
exec docker run --rm $TTY_FLAGS \
	-u "$(id -u):$(id -g)" \
	-v "$PROJECT:/project" \
	-w /project \
	-e HOME=/tmp \
	"$IMAGE" "$@"
