#!/bin/sh
# Run make inside the BlocksDS toolchain container against the working tree, so
# builds are incremental and artifacts land directly in this directory.
#
#   ./docker-build.sh              # build portalDS.nds
#   ./docker-build.sh clean
#   ./docker-build.sh dldipatch
#   ./docker-build.sh docs         # generate docs/api/html with doxygen
#   ./docker-build.sh test         # run the host unit tests
#   ./docker-build.sh sh           # interactive shell in the toolchain
#
# Set BLOCKSDS_IMAGE to pin a different toolchain version, or DOXYGEN_IMAGE /
# TEST_IMAGE to pin the (separate) images used for "docs" and "test".

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
		sh -c "apk add --no-cache doxygen >/dev/null \
		       && mkdir -p docs/api \
		       && doxygen Doxyfile \
		       && chown -R $(id -u):$(id -g) docs \
		       && echo 'docs written to docs/api/html/index.html'"
fi

# Tests are a special case for the opposite reason to docs: they are not cross
# compiled at all. They build the pure logic sources for whatever machine they
# run on, so what they need is a plain host compiler - which the toolchain
# image, being a cross toolchain, does not carry. Ubuntu 24.04 to match the
# base the BlocksDS image itself uses. Root is needed to apt-get, hence the
# chown afterwards.
#
# Anything after "test" is passed through to make, so e.g.
#   ./docker-build.sh test SANITIZE=0
if [ "${1-}" = "test" ]; then
	shift
	exec docker run --rm \
		-v "$PROJECT:/project" \
		-w /project \
		-e HOME=/tmp \
		"${TEST_IMAGE:-ubuntu:24.04}" \
		sh -c "set -e; \
		       apt-get update >/dev/null && \
		       DEBIAN_FRONTEND=noninteractive apt-get install -y \
		           gcc libc6-dev make >/dev/null; \
		       set +e; \
		       make test $*; \
		       status=\$?; \
		       chown -R $(id -u):$(id -g) tests/build 2>/dev/null; \
		       exit \$status"
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
