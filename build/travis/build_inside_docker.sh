#!/bin/bash

progdir=${0%/*}

[[ -e $progdir/env.sh ]] && source "$progdir/env.sh"

set -eu

# Create the `build` group and user.
if ! id -g build > /dev/null 2>&1; then
    groupadd -g ${BUILD_GID:-99} build
fi
if ! id -u build > /dev/null 2>&1; then
    useradd -u ${BUILD_UID:-99} -g ${BUILD_GID:-99} build
fi

mv bld_external bld_external_ubuntu
mv bld_external_rhel bld_external

# Install dependencies inside Docker.
# TODO: replace this with invocation of yum-builddep to prevent repeating
# our list of required RPMs in both the Travis build scripts and the RPM spec.
if [[ $DOCKER_IMAGE = centos* ]]; then
    yum -y install epel-release
fi
yum -y install ${RPM_DEPENDENCIES[@]}
if [[ $DOCKER_IMAGE = centos:centos7 ]]; then
    yum -y install python36-devel boost169-devel boost169-static
fi

# Prepare the build script; could be replaced by a ctest driver.
cat > "$progdir/cmake_driver_script.sh" <<__END__
#!/bin/sh
set -eu
time cmake ${CMAKE_OPTIONS[@]} "-DCMAKE_INSTALL_PREFIX=$PWD/release_dir"
time make -j2 externals
time make -j2 install
ctest -j8 -L travis || (cat Testing/Temporary/LastTest.log && exit 1)
__END__
chmod +x "$progdir/cmake_driver_script.sh"

# Drop privileges and do the actual build
exec runuser -u build "$progdir/cmake_driver_script.sh"

# vim:et:sw=4:sts=4:ts=8
