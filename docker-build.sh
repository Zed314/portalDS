#!/bin/sh
# Run make inside the BlocksDS toolchain container against the working tree, so
# builds are incremental and artifacts land directly in this directory.
#
#   ./docker-build.sh              # build portalDS.nds
#   ./docker-build.sh clean
#   ./docker-build.sh dldipatch
#   ./docker-build.sh docs         # generate docs/api/html with doxygen
#   ./docker-build.sh sh           # interactive shell in the toolchain
#
# Set BLOCKSDS_IMAGE to pin a different toolchain version, or DOXYGEN_IMAGE to
# pin the (separate) image used for "docs".

set -eu

IMAGE="${BLOCKSDS_IMAGE:-skylyrac/blocksds:slim-latest}"
PROJECT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

# Docs are a special case: the BlocksDS image has no doxygen, and there is no
# maintained official doxygen image, so this installs it into a throwaway
# Alpine container. That needs root, hence the chown afterwards to hand the
# output back to the invoking user.
if [ "${1-}" = "docs" ]; then
	exec docker run --rm \
		-v "$PROJECT:/project" \
		-w /project \
		-e HOME=/tmp \
		"${DOXYGEN_IMAGE:-alpine:3.20}" \
		sh -c "apk add --no-cache doxygen >/dev/null && doxygen Doxyfile \
		       && chown -R $(id -u):$(id -g) docs/api \
		       && echo 'docs written to docs/api/html/index.html'"
fi

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
