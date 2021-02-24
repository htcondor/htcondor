FROM centos:8

# Use the bash shell for commands
SHELL ["/bin/bash", "-c"]

# Add users that might be used in CHTC
# The HTCondor that runs inside the container needs to have the user defined
RUN for i in {1..161}; do \
        uid=$(($i+5000)); \
        useradd --uid  $uid --shell /bin/bash --create-home slot$i; \
    done

RUN for i in {1..161}; do \
        uid=$(($i+5299)); \
        useradd --uid  $uid --shell /bin/bash --create-home slot1_$i; \
    done

RUN useradd --uid  6004 --shell /bin/bash --create-home condorauto && \
    useradd --uid 22537 --shell /bin/bash --create-home bbockelm && \
    useradd --uid 20343 --shell /bin/bash --create-home blin && \
    useradd --uid 20003 --shell /bin/bash --create-home cat && \
    useradd --uid 20849 --shell /bin/bash --create-home ckoch5 && \
    useradd --uid 21640 --shell /bin/bash --create-home coatsworth && \
    useradd --uid 20342 --shell /bin/bash --create-home edquist && \
    useradd --uid 20006 --shell /bin/bash --create-home gthain && \
    useradd --uid 20839 --shell /bin/bash --create-home iaross && \
    useradd --uid 21356 --shell /bin/bash --create-home jcpatton && \
    useradd --uid 20007 --shell /bin/bash --create-home jfrey && \
    useradd --uid 20018 --shell /bin/bash --create-home johnkn && \
    useradd --uid 22968 --shell /bin/bash --create-home kcramer && \
    useradd --uid 20005 --shell /bin/bash --create-home kupsch && \
    useradd --uid 20383 --shell /bin/bash --create-home lmichael && \
    useradd --uid 20020 --shell /bin/bash --create-home matyas && \
    useradd --uid 10006 --shell /bin/bash --create-home moate && \
    useradd --uid 20013 --shell /bin/bash --create-home tannenba && \
    useradd --uid 20345 --shell /bin/bash --create-home tim && \
    useradd --uid 22109 --shell /bin/bash --create-home tjslauson && \
    useradd --uid 20015 --shell /bin/bash --create-home tlmiller && \
    useradd --uid 20017 --shell /bin/bash --create-home zmiller

# Provide a condor_config.generic
RUN mkdir -p /usr/local/condor/etc/examples && \
    echo 'use SECURITY : HOST_BASED' > /usr/local/condor/etc/examples/condor_config.generic

# Add tools to add the HTCondor repository
RUN dnf install -y \
    'dnf-command(config-manager)' \
    epel-release \
    https://research.cs.wisc.edu/htcondor/repo/current/htcondor-release-current.el8.noarch.rpm \
    && dnf config-manager --set-enabled powertools \
    && dnf clean all

# Need HTCondor to build and test HTCondor (BaTLab style)
RUN dnf install -y \
    condor \
    java \
    openssh-clients \
    openssh-server \
    'perl(Net::Domain)' \
    'perl(Time::HiRes)' \
    which \
    && dnf clean all

# Tools needed for building
RUN dnf install -y \
    make \
    patchelf \
    rpm-build \
    yum-utils \
    && dnf clean all

# HTCondor build dependencies
RUN dnf install -y \
    'bind-utils' \
    'bison' \
    'bison-devel' \
    'boost-devel' \
    'boost-python3-devel' \
    'boost-static' \
    'byacc' \
    'cmake' \
    'expat-devel' \
    'flex' \
    'gcc-c++' \
    'glibc-static' \
    'krb5-devel' \
    'libcgroup-devel' \
    'libcurl-devel' \
    'libstdc++-static' \
    'libtool' \
    'libtool-ltdl-devel' \
    'libuuid-devel' \
    'libvirt-devel' \
    'libX11-devel' \
    'libxml2-devel' \
    'libXScrnSaver-devel' \
    'm4' \
    'nss-devel' \
    'openldap-devel' \
    'openssl-devel' \
    'pam-devel' \
    'patch' \
    'pcre-devel' \
    'perl(Archive::Tar)' \
    'perl(Data::Dumper)' \
    'perl(Digest::MD5)' \
    'perl(Digest::SHA)' \
    'perl(Env)' \
    'perl(XML::Parser)' \
    'python3-devel' \
    'python3-sphinx' \
    'python3-sphinx_rtd_theme' \
    'redhat-rpm-config' \
    'sqlite-devel' \
    'systemd-devel' \
    'systemd-units' \
    && dnf clean all

# Additional dependencies for OSG style build
RUN dnf install -y \
    globus-callout-devel \
    globus-common-devel \
    globus-ftp-client-devel \
    globus-ftp-control-devel \
    globus-gass-server-ez-devel \
    globus-gass-transfer-devel \
    globus-gram-client-devel \
    globus-gram-protocol \
    globus-gsi-callback-devel \
    globus-gsi-cert-utils-devel \
    globus-gsi-credential-devel \
    globus-gsi-openssl-error-devel \
    globus-gsi-proxy-core-devel \
    globus-gsi-proxy-ssl-devel \
    globus-gsi-sysconfig-devel \
    globus-gss-assist-devel \
    globus-gssapi-error-devel \
    globus-gssapi-gsi-devel \
    globus-io-devel \
    globus-openssl-module-devel \
    globus-rsl-devel \
    globus-xio-devel \
    munge-devel \
    scitokens-cpp-devel \
    voms-devel \
    && dnf clean all

RUN pip3 install pytest pytest-httpserver
