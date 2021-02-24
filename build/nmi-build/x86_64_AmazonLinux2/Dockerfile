FROM amazonlinux:2

# Use the bash shell for commands
SHELL ["/bin/bash", "-c"]

# need the useradd program
RUN yum install -y \
    shadow-utils \
    && yum clean all

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

# Add in EPEL
RUN yum install -y \
    https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm \
    && yum clean all

# Add tools to add the HTCondor repository
RUN yum install -y \
    wget \
    && yum clean all

# Add HTCondor repository
RUN wget https://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor && \
    rpm --import RPM-GPG-KEY-HTCondor && \
    cd /etc/yum.repos.d && \
    wget https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-prerelease-amzn2.repo

# Need HTCondor to build and test HTCondor (BaTLab style)
RUN yum install -y \
    condor \
    java \
    openssh-clients \
    openssh-server \
    openssl \
    'perl(Digest::SHA)' \
    'perl(Env)' \
    which \
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
    'boost169-python2-devel' \
    'boost169-python3-devel' \
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
    'openssl-devel' \
    'pam-devel' \
    'pcre-devel' \
    'perl(Archive::Tar)' \
    'perl(Data::Dumper)' \
    'perl(Digest::MD5)' \
    'perl(XML::Parser)' \
    'python3-devel' \
    'python3-setuptools' \
    'python-devel' \
    'python-sphinx' \
    'python-sphinx_rtd_theme' \
    'sqlite-devel' \
    'systemd-devel' \
    && yum clean all

