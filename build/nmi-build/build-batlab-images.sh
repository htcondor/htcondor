#!/bin/bash
# Exit on any error
set -e

usage() {
    echo "Usage: $(basename "$0") serial-number (0-99)"
}
if [ $# -ne 1 ]; then
    usage
    exit 1
fi
SERIAL=$1

buildimage() {
    platform=$1
    base_docker_platform=$2
    base_docker_image=$3
    # Always work with the latest base image
    docker pull --platform="$base_docker_platform" "$base_docker_image"
    rm -rf "$platform"
    mkdir -p "$platform/tmp"
    cat << EOF > "$platform/Dockerfile"
FROM $base_docker_image
# Since our build machine is in Madison, Wisconsin, US, set the timezone
ENV TZ="America/Chicago"

# Use the bash shell for commands
SHELL ["/bin/bash", "-c"]

# Run the setup script
COPY tmp/ /tmp/

RUN /tmp/setup.sh $CONDOR_VERSION
EOF
    cp -pr setup.sh ../packaging/{debian,rpm} "$platform/tmp/"
    docker build "$platform" --platform="$base_docker_platform" --tag "htcondor/nmi-build:$platform-$CONTAINER_VERSION" > "$platform.out" 2>&1
    if grep -q 'Successfully tagged htcondor/nmi-build:' "$platform.out"; then
        docker push "htcondor/nmi-build:$platform-$CONTAINER_VERSION"
    fi
    rm -rf "$platform"
}

CONDOR_VERSION=$(grep 'set(VERSION' ../../CMakeLists.txt | sed -e 's/.*"\(.*\)".*/\1/')
# shellcheck disable=SC2206 # don't have to worry about word splitting
AVERSION=(${CONDOR_VERSION//./ })
MAJOR_VER=${AVERSION[0]}
MINOR_VER=${AVERSION[1]}
PATCH_VER=${AVERSION[2]}
CONTAINER_VERSION=$(printf "%02d%02d%02d%02d" "$MAJOR_VER" "$MINOR_VER" "$PATCH_VER" "$SERIAL")

buildimage aarch64_AlmaLinux8 linux/arm64 arm64v8/almalinux:8 &
buildimage aarch64_AlmaLinux9 linux/arm64 arm64v8/almalinux:9 &
buildimage ppc64le_AlmaLinux8 linux/ppc64le ppc64le/almalinux:8 &
buildimage x86_64_AlmaLinux8 linux/x86_64 almalinux:8 &
buildimage x86_64_AlmaLinux9 linux/x86_64 almalinux:9 &
buildimage x86_64_AmazonLinux2023 linux/x86_64 amazonlinux:2023 &
buildimage x86_64_Debian11 linux/x86_64 debian:bullseye &
buildimage x86_64_Debian12 linux/x86_64 debian:bookworm &
buildimage x86_64_Fedora40 linux/x86_64 fedora:40 &
buildimage x86_64_openSUSE15 linux/x86_64 opensuse/leap:15 &
buildimage x86_64_Ubuntu22 linux/x86_64 ubuntu:jammy &
buildimage x86_64_Ubuntu24 linux/x86_64 ubuntu:noble &
wait
tail -n 1 ./*.out
