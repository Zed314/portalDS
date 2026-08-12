# Builds portalDS.nds with the official BlocksDS toolchain image, so no local
# devkit/BlocksDS installation is needed.
#
#   docker build --output out .        -> out/portalDS.nds
#
# See docker-build.sh for an incremental (bind-mounted) build instead.

ARG BLOCKSDS_IMAGE=skylyrac/blocksds:slim-latest

FROM ${BLOCKSDS_IMAGE} AS builder

# The image already exports BLOCKSDS, BLOCKSDSEXT and WONDERFUL_TOOLCHAIN, and
# the makefiles pick them up through their `?=` defaults.

WORKDIR /project

COPY Makefile Makefile.arm7 Makefile.arm9 logo.png ./
COPY common common
COPY arm7 arm7
COPY arm9 arm9
COPY nitrofiles nitrofiles

RUN make -j"$(nproc)"

# Stage holding nothing but the ROM, so `docker build --output <dir>` drops the
# artifact straight onto the host.
FROM scratch AS artifact

COPY --from=builder /project/portalDS.nds /
