#!/bin/bash
# Exit on any error
set -e

# HTConder version
HTCONDOR_VERSION=$1
echo "HTCondor version is $HTCONDOR_VERSION"

# shellcheck disable=SC2206 # don't have to worry abour word splitting
AVERSION=(${HTCONDOR_VERSION//./ })
MAJOR_VER=${AVERSION[0]}
MINOR_VER=${AVERSION[1]}
if [ "$MINOR_VER" -eq 0 ]; then
    REPO_VERSION=$MAJOR_VER.0
else
    REPO_VERSION=$MAJOR_VER.x
fi

# Platform variables
VERSION_CODENAME='none'
. /etc/os-release
echo "Building on $NAME $VERSION"
VERSION_ID=${VERSION_ID%%.*}
ARCH=$(arch)
echo "ID=$ID VERSION_ID=$VERSION_ID VERSION_CODENAME=$VERSION_CODENAME ARCH=$ARCH"

if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    SUDO_GROUP='sudo'
else
    SUDO_GROUP='wheel'
fi

if [ $ID = debian ] || [ $ID = 'ubuntu' ]; then
    apt-get update
    export DEBIAN_FRONTEND='noninteractive'
    INSTALL='apt-get install --yes'
elif [ $ID = 'centos' ]; then
    INSTALL='yum install --assumeyes'
elif [ $ID = 'opensuse-leap' ]; then
    INSTALL='zypper --non-interactive install'
    $INSTALL system-group-wheel system-user-mail
elif [ $ID = 'amzn' ] || [ $ID = 'almalinux' ] || [ $ID = 'fedora' ]; then
    INSTALL='dnf install --assumeyes'
    $INSTALL 'dnf-command(config-manager)'
fi

if [ $ID = 'amzn' ]; then
    $INSTALL -y shadow-utils
fi

# Add users that might be used in CHTC
# The HTCondor that runs inside the container needs to have the user defined
for i in {1..161}; do
    uid=$((i+5000));
    useradd --uid  $uid --gid $SUDO_GROUP --shell /bin/bash --create-home slot$i;
done

for i in {1..161}; do
    uid=$((i+5299));
    useradd --uid  $uid --gid $SUDO_GROUP --shell /bin/bash --create-home slot1_$i;
done

useradd --uid  6004 --gid $SUDO_GROUP --shell /bin/bash --create-home condorauto
useradd --uid 22537 --gid $SUDO_GROUP --shell /bin/bash --create-home bbockelm
useradd --uid 20343 --gid $SUDO_GROUP --shell /bin/bash --create-home blin
useradd --uid 24200 --gid $SUDO_GROUP --shell /bin/bash --create-home cabollig
useradd --uid 20003 --gid $SUDO_GROUP --shell /bin/bash --create-home cat
useradd --uid 20342 --gid $SUDO_GROUP --shell /bin/bash --create-home edquist
useradd --uid 20006 --gid $SUDO_GROUP --shell /bin/bash --create-home gthain
useradd --uid 20839 --gid $SUDO_GROUP --shell /bin/bash --create-home iaross
useradd --uid 21356 --gid $SUDO_GROUP --shell /bin/bash --create-home jcpatton
useradd --uid 20007 --gid $SUDO_GROUP --shell /bin/bash --create-home jfrey
useradd --uid 20018 --gid $SUDO_GROUP --shell /bin/bash --create-home johnkn
useradd --uid 25234 --gid $SUDO_GROUP --shell /bin/bash --create-home jrreuss
useradd --uid 20020 --gid $SUDO_GROUP --shell /bin/bash --create-home matyas
useradd --uid 20013 --gid $SUDO_GROUP --shell /bin/bash --create-home tannenba
useradd --uid 20345 --gid $SUDO_GROUP --shell /bin/bash --create-home tim
useradd --uid 20015 --gid $SUDO_GROUP --shell /bin/bash --create-home tlmiller

# Provide a condor_config.generic
mkdir -p /usr/local/condor/etc/examples
echo 'use SECURITY : HOST_BASED' > /usr/local/condor/etc/examples/condor_config.generic

if [ $ID = 'almalinux' ] || [ $ID = 'centos' ]; then
    $INSTALL epel-release
    if [ $VERSION_ID -eq 7 ]; then
        $INSTALL centos-release-scl
    elif [ $VERSION_ID -eq 8 ]; then
        dnf config-manager --set-enabled powertools
    elif [ $VERSION_ID -eq 9 ]; then
        dnf config-manager --set-enabled crb
    fi
fi

if [ $ID = 'amzn' ]; then
    $INSTALL "https://research.cs.wisc.edu/htcondor/repo/$REPO_VERSION/htcondor-release-current.amzn$VERSION_ID.noarch.rpm"
fi

if [ $ID = 'almalinux' ] || [ $ID = 'centos' ]; then
    $INSTALL "https://research.cs.wisc.edu/htcondor/repo/$REPO_VERSION/htcondor-release-current.el$VERSION_ID.noarch.rpm"
fi

if [ $ID = 'fedora' ]; then
    $INSTALL "https://research.cs.wisc.edu/htcondor/repo/$REPO_VERSION/htcondor-release-current.fc$VERSION_ID.noarch.rpm"
fi

# openSUSE has a zypper command to install a repo from a URL.
# Let's use that in the future. This works for now.
if [ $ID = 'opensuse-leap' ]; then
    zypper --non-interactive --no-gpg-checks install "https://research.cs.wisc.edu/htcondor/repo/$REPO_VERSION/htcondor-release-current.leap$VERSION_ID.noarch.rpm"
    for key in /etc/pki/rpm-gpg/*; do
        rpmkeys --import "$key"
    done
fi

# Setup Debian based repositories
if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    $INSTALL apt-transport-https apt-utils curl gnupg
    mkdir -p /etc/apt/keyrings
    curl -fsSL "https://research.cs.wisc.edu/htcondor/repo/keys/HTCondor-${REPO_VERSION}-Key" -o /etc/apt/keyrings/htcondor.asc
    curl -fsSL "https://research.cs.wisc.edu/htcondor/repo/$ID/htcondor-${REPO_VERSION}-${VERSION_CODENAME}.list" -o /etc/apt/sources.list.d/htcondor.list
    apt-get update
fi

# Use the testing repositories for unreleased software
if [ "$VERSION_CODENAME" = 'future' ] && [ "$ARCH" = 'x86_64' ]; then
    cp -p /etc/apt/sources.list.d/htcondor.list /etc/apt/sources.list.d/htcondor-test.list
    sed -i s+repo/+repo-test/+ /etc/apt/sources.list.d/htcondor-test.list
    apt-get update
fi
if [ $ID = 'future' ]; then
    cp -p /etc/yum.repos.d/htcondor.repo /etc/yum.repos.d/htcondor-test.repo
    sed -i s+repo/+repo-test/+ /etc/yum.repos.d/htcondor-test.repo
    sed -i s/\\[htcondor/[htcondor-test/ /etc/yum.repos.d/htcondor-test.repo
    # ] ] Help out vim syntax highlighting
fi
if [ $ID = 'future-opensuse-leap' ]; then
    cp -p /etc/zypp/repos.d/htcondor.repo /etc/zypp/repos.d/htcondor-test.repo
    sed -i s+repo/+repo-test/+ /etc/zypp/repos.d/htcondor-test.repo
    sed -i s/\\[htcondor/[htcondor-test/ /etc/zypp/repos.d/htcondor-test.repo
    # ] ] Help out vim syntax highlighting
fi

# Install the build dependencies
if [ $ID = 'almalinux' ] || [ $ID = 'amzn' ] || [ $ID = 'centos' ] || [ $ID = 'fedora' ]; then
    $INSTALL make rpm-build yum-utils
    yum-builddep -y /tmp/rpm/condor.spec
fi

# Install the build dependencies
if [ $ID = 'opensuse-leap' ]; then
    $INSTALL make rpm-build
    $INSTALL $(rpmspec --parse /tmp/rpm/condor.spec | grep '^BuildRequires:' | sed -e 's/^BuildRequires://' | sed -e 's/,/ /')
fi

# Need newer cmake on bionic
if [ "$VERSION_CODENAME" = 'bionic' ]; then
    curl -dsSL https://apt.kitware.com/keys/kitware-archive-latest.asc | apt-key add -
    echo 'deb https://apt.kitware.com/ubuntu/ bionic main' > /etc/apt/sources.list.d/cmake.list
    apt-get update
fi

if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    $INSTALL build-essential devscripts equivs gpp
    (cd /tmp/debian; ./prepare-build-files.sh -DUW_BUILD)
    mk-build-deps --install --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' /tmp/debian/control
fi

if [ "$VERSION_CODENAME" = 'focal' ]; then
    # Need to upgrade compiler on this old platform
    $INSTALL gcc-10 g++-10
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 1000 --slave /usr/bin/g++ g++ /usr/bin/g++-10
fi

# Add useful debugging tools
$INSTALL gdb git less nano patchelf python3-pip strace sudo vim
if [ $ID = 'almalinux' ] || [ $ID = 'amzn' ] || [ $ID = 'centos' ] || [ $ID = 'fedora' ] || [ $ID = 'opensuse-leap' ]; then
    $INSTALL iputils rpmlint
fi
if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    $INSTALL lintian net-tools
fi

# Container users can sudo
echo "%$SUDO_GROUP ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/$SUDO_GROUP

# Install HTCondor to build and test BaTLab style
if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    $INSTALL htcondor libnss-myhostname openssh-server
    # Ensure that gethostbyaddr() returns our hostname
    sed -i -e 's/^hosts:.*/& myhostname/' /etc/nsswitch.conf
fi

if [ $ID = 'almalinux' ] || [ $ID = 'amzn' ] || [ $ID = 'centos' ] || [ $ID = 'fedora' ] || [ $ID = 'opensuse-leap' ]; then
    $INSTALL condor hostname java openssh-clients openssh-server openssl
    if [ $ID = 'opensuse-leap' ]; then
        $INSTALL procps wget
        # Install better patchelf
        wget https://github.com/NixOS/patchelf/releases/download/0.18.0/patchelf-0.18.0-x86_64.tar.gz
        (cd /usr; tar xfpz /patchelf-0.18.0-x86_64.tar.gz ./bin/patchelf)
        rm patchelf-0.18.0-x86_64.tar.gz
    else
        $INSTALL procps-ng
    fi
    if [ $ID != 'amzn' ]; then
        $INSTALL apptainer
    fi
    $INSTALL 'perl(Archive::Tar)' 'perl(Data::Dumper)' 'perl(Digest::MD5)' 'perl(Digest::SHA)' 'perl(English)' 'perl(Env)' 'perl(File::Copy)' 'perl(FindBin)' 'perl(Net::Domain)' 'perl(Sys::Hostname)' 'perl(Time::HiRes)' 'perl(XML::Parser)'
fi

# Match the current version. Consult:
# https://apptainer.org/docs/admin/latest/installation.html#install-debian-packages
if [ $ID = 'debian' ] && [ "$ARCH" = 'x86_64' ]; then
    $INSTALL wget
    wget https://github.com/apptainer/apptainer/releases/download/v1.2.5/apptainer_1.2.5_amd64.deb
    $INSTALL ./apptainer_1.2.5_amd64.deb
    rm ./apptainer_1.2.5_amd64.deb
fi

if [ $ID = 'ubuntu' ] && [ "$ARCH" = 'x86_64' ] && [ $VERSION_CODENAME != 'noble' ]; then
    $INSTALL software-properties-common
    add-apt-repository -y ppa:apptainer/ppa
    apt-get update
    $INSTALL apptainer
fi


# Include packages for tarball in the image.
externals_dir="/usr/local/condor/externals"
mkdir -p "$externals_dir"
if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    chown _apt "$externals_dir"
    pushd "$externals_dir"
    apt-get download libgomp1 libmunge2 libpcre2-8-0 libscitokens0 pelican pelican-osdf-compat
    if [ $VERSION_CODENAME = 'bullseye' ]; then
        apt-get download libboost-python1.74.0 libvomsapi1v5
    elif [ $VERSION_CODENAME = 'bookworm' ]; then
        apt-get download libboost-python1.74.0 libvomsapi1v5
    elif [ $VERSION_CODENAME = 'focal' ]; then
        apt-get download libboost-python1.71.0 libvomsapi1v5
    elif [ $VERSION_CODENAME = 'jammy' ]; then
        apt-get download libboost-python1.74.0 libvomsapi1v5
    elif [ $VERSION_CODENAME = 'noble' ]; then
        apt-get download libboost-python1.83.0 libvomsapi1t64
    else
        echo "Unknown codename: $VERSION_CODENAME"
        exit 1
    fi
    popd
fi
if [ $ID = 'almalinux' ] || [ $ID = 'amzn' ] || [ $ID = 'centos' ] || [ $ID = 'fedora' ]; then
    yumdownloader --downloadonly --destdir="$externals_dir" \
        libgomp munge-libs pelican pelican-osdf-compat pcre2 scitokens-cpp
    if [ $ID != 'amzn' ]; then
        yumdownloader --downloadonly --destdir="$externals_dir" voms
    fi
    if [ $ID = 'centos' ] && [ $VERSION_ID -eq 7 ]; then
        yumdownloader --downloadonly --destdir="$externals_dir" \
            boost169-python3 python36-chardet python36-idna python36-pysocks python36-requests python36-six python36-urllib3
    else
        yumdownloader --downloadonly --destdir="$externals_dir" boost-python3
    fi
    # Remove 32-bit x86 packages if any
    rm -f "$externals_dir"/*.i686.rpm
fi
if [ $ID = 'opensuse-leap' ]; then
    zypper --non-interactive --pkg-cache-dir "$externals_dir" download libgomp1 libmunge2 libpcre2-8-0 libSciTokens0 libboost_python-py3-1_75_0 pelican pelican-osdf-compat
fi

# Clean up package caches
if [ $ID = 'centos' ]; then
    yum clean all
    rm -rf /var/cache/yum/*
fi
if [ $ID = 'amzn' ] || [ $ID = 'almalinux' ] || [ $ID = 'fedora' ]; then
    dnf clean all
    rm -rf /var/cache/yum/*
fi
if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    apt-get -y autoremove
    apt-get -y clean
fi

# Install apptainer into externals directory
if [ $ID != 'amzn' ]; then
    if [ $ID != 'ubuntu' ] || [ "$ARCH" != 'ppcle64' ]; then
        mkdir -p "$externals_dir/apptainer"
        if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
            $INSTALL cpio rpm2cpio
        fi
        curl -s https://raw.githubusercontent.com/apptainer/apptainer/main/tools/install-unprivileged.sh | \
            bash -s - "$externals_dir/apptainer"
        rm -r "$externals_dir/apptainer/$ARCH/libexec/apptainer/cni"
        # Move apptainer out of the default path
        mv "$externals_dir/apptainer/bin" "$externals_dir/apptainer/libexec"
    fi
fi

# Install pytest for BaTLab testing
# Install sphinx-mermaid so docs can have images
if [ $ID = 'debian' ] || [ $ID = 'ubuntu' ]; then
    if [ "$VERSION_CODENAME" = 'bullseye' ] || [ "$VERSION_CODENAME" = 'focal' ] || [ "$VERSION_CODENAME" = 'jammy' ]; then
        pip3 install pytest pytest-httpserver sphinxcontrib-mermaid
    else
        pip3 install --break-system-packages pytest pytest-httpserver sphinxcontrib-mermaid
    fi
else
        pip3 install pytest pytest-httpserver sphinxcontrib-mermaid
fi

if [ $ID = 'amzn' ] || [ "$VERSION_CODENAME" = 'bullseye' ] || [ "$VERSION_CODENAME" = 'focal' ]; then
    # Pip installs a updated version of markupsafe that is incompatiable
    # with sphinx on these platforms. Downgrade markupsafe and hope for the best
    pip3 install markupsafe==2.0.1
fi

rm -rf /tmp/*
exit 0
