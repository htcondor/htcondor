FROM i386/centos:7

# Use the bash shell for commands
SHELL ["/bin/bash", "-c"]

# Only look for i386 packages
RUN echo "i386" > /etc/yum/vars/basearch

# Get any updates
RUN yum update -y && yum clean all

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
RUN yum install -y \
    epel-release \
    wget \
    && yum clean all

# Add HTCondor repository
RUN wget https://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor && \
    rpm --import RPM-GPG-KEY-HTCondor && \
    rm -f RPM-GPG-KEY-HTCondor

COPY htcondor-cs.repo /etc/yum.repos.d

# Need HTCondor to build and test HTCondor (BaTLab style)
RUN yum install -y \
    condor.i686 \
    java \
    openssh-clients \
    openssh-server \
    && yum clean all

# Tools needed for building
RUN yum install -y \
    make \
    rpm-build \
    && yum clean all

# HTCondor build dependencies
RUN yum install -y \
    'bind-utils' \
    'bison' \
    'bison-devel' \
    'boost-devel' \
    'boost-python' \
    'byacc' \
    'c-ares-devel' \
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
    'openssl' \
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
    'python3-setuptools' \
    'python-devel' \
    'python-setuptools' \
    'python-sphinx' \
    'redhat-rpm-config' \
    'sqlite-devel' \
    'systemd-devel' \
    'which' \
    && yum clean all

