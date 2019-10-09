#!/bin/bash

progdir=${0%/*}

[[ -e $progdir/env.sh ]] && source "$progdir/env.sh"

set -eu

if [ `id -u` -eq 0 ]; then
    # Create the `build` group and user.
    if ! id -g build > /dev/null 2>&1; then
        groupadd -g ${BUILD_GID:-99} build
    fi
    if ! id -u build > /dev/null 2>&1; then
        useradd -u ${BUILD_UID:-99} -g ${BUILD_GID:-99} build
    fi

    mkdir -p /srv/build
    chown build:build /srv/build

    if [ -e bld_external ]; then
        mv bld_external bld_external_ubuntu
    fi
    if [ -e bld_external_rhel ]; then
        mv bld_external_rhel bld_external
    fi

    # Install dependencies inside Docker.
    # TODO: replace this with invocation of yum-builddep to prevent repeating
    # our list of required RPMs in both the Travis build scripts and the RPM spec.
    if [[ $DOCKER_IMAGE = centos* ]]; then
        yum -y install epel-release
    fi
    if [ -z "${RPM_DEPENDENCIES+x}" ]; then
        # Ensure yum-utils (for yum-builddep) and git (for ctest rev checking) are present
        yum -y install yum-utils git
        yum-builddep -y ${CONDOR_SRC}/build/packaging/srpm/condor.spec
    else
        yum -y install ${RPM_DEPENDENCIES[@]}
        if [[ $DOCKER_IMAGE = centos:centos7 ]]; then
            yum -y install python36-devel boost169-devel boost169-static
        fi
    fi
fi

if [ -z "${USE_CDASH}" ]; then
    # Prepare the build script; could be replaced by a ctest driver.
    # For now, test failures do not cause overall build failures.
    cat > "$progdir/cmake_driver_script.sh" <<__END__
    #!/bin/sh
    set -eu
    time cmake ${CMAKE_OPTIONS[@]} "-DCMAKE_INSTALL_PREFIX=$PWD/release_dir"
    time make -j2 externals
    time make -j2 install
    ctest -j4 --timeout 240 -L travis || :
__END__
    chmod +x "$progdir/cmake_driver_script.sh"

    # Drop privileges and do the actual build
    if [ `id -u` -eq 0 ]; then
        exec runuser -u build "$progdir/cmake_driver_script.sh"
    else
        exec "$progdir/cmake_driver_script.sh"
    fi
else
    cd /srv/build
    # Note that verbose level leaks the token into stdout; filter out that header line!
    ctest --verbose -S ${CONDOR_SRC}/build/travis/Dashboard.cmake | grep -v 'Authorization: Bearer'
fi

# vim:et:sw=4:sts=4:ts=8
