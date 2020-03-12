#!/bin/bash

progdir=${0%/*}

if [[ $TRAVIS_OS_NAME == linux ]]; then
    LIBVIRT=ON
    LIBCGROUP=ON
else
    LIBVIRT=OFF
    LIBCGROUP=OFF
fi
if [[ -n $DOCKER_IMAGE ]]; then
    BUILD_TESTING=TRUE
else
    BUILD_TESTING=FALSE
fi

cat > "$progdir/env.sh" <<__END__
export CMAKE_OPTIONS=(
    -DPROPER:BOOL=$PROPER
    -DUW_BUILD:BOOL=$UW_BUILD
    -D_DEBUG:BOOL=TRUE
    -DCMAKE_SKIP_RPATH:BOOL=ON
    -DHAVE_EXT_GSOAP:BOOL=OFF
    -DWITH_GSOAP:BOOL=OFF
    -DHAVE_EXT_CURL:BOOL=ON
    -DHAVE_EXT_OPENSSL:BOOL=ON
    -DHAVE_EXT_BOOST:BOOL=ON
    -DHAVE_EXT_GLOBUS:BOOL=$GLOBUS
    -DWITH_GLOBUS:BOOL=$GLOBUS
    -DWITH_SCITOKENS=$SCITOKENS
    -DHAVE_EXT_KRB5:BOOL=ON
    -DHAVE_EXT_LIBVIRT:BOOL=$LIBVIRT
    -DHAVE_EXT_LIBXML2:BOOL=ON
    -DHAVE_EXT_OPENSSL:BOOL=ON
    -DHAVE_EXT_PCRE:BOOL=ON
    -DHAVE_EXT_VOMS:BOOL=$VOMS
    -DWITH_VOMS:BOOL=$VOMS
    -DWITH_LIBCGROUP:BOOL=$LIBCGROUP
    -DWANT_CONTRIB:BOOL=OFF
    -DWITH_BOSCO:BOOL=OFF
    -DWITH_PYTHON_BINDINGS:BOOL=OFF
    -DWITH_CAMPUSFACTORY:BOOL=OFF
    -DBUILD_TESTING:BOOL=$BUILD_TESTING
)

export RPM_DEPENDENCIES=(
    systemtap-sdt-devel
    patch
    c-ares-devel
    autoconf
    automake
    libtool
    perl-Time-HiRes
    perl-Archive-Tar
    perl-XML-Parser
    perl-Digest-MD5
    gcc-c++
    make
    cmake
    flex
    byacc
    pcre-devel
    openssl-devel
    krb5-devel
    libvirt-devel
    bind-utils
    m4
    libX11-devel
    libXScrnSaver-devel
    curl-devel
    expat-devel
    openldap-devel
    python-devel
    boost-devel
    redhat-rpm-config
    sqlite-devel
    glibc-static
    libuuid-devel
    bison
    bison-devel
    libtool-ltdl-devel
    pam-devel
    nss-devel
    libxml2-devel
    libstdc++-devel
    libstdc++-static
)

export DOCKER_IMAGE=$DOCKER_IMAGE
__END__
trap 'rm -f "$progdir/env.sh"' ERR EXIT
source "$progdir/env.sh"


quiet_make () {
    (sleep 590; echo "--- ping ---") &  # travis times out after 10m of no output
    ret=0
    time make -j2 "$@" &> quiet_make.log || ret=$?
    # 2 is 'no such target'
    if [[ $ret -ne 0 && $ret -ne 2 ]]; then
        cat quiet_make.log
        return $ret
    fi
}


set -eu

if [[ -z $DOCKER_IMAGE ]]; then
    time cmake "${CMAKE_OPTIONS[@]}"
    if [[ $PROPER == OFF ]]; then
        if [[ $GLOBUS == ON ]]; then
            quiet_make globus
        fi
        quiet_make boost
        time make -j2 externals
    fi
    time make -j2
else
    touch bld_external_rhel bld_external
    sudo docker run --rm=true -w "`pwd`" -v "`pwd`:`pwd`" $DOCKER_IMAGE /bin/bash -x "$progdir/build_inside_docker.sh"
fi

# vim:et:sw=4:sts=4:ts=8
