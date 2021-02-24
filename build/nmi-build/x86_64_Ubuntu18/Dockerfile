FROM ubuntu:bionic

# Use the bash shell for commands
SHELL ["/bin/bash", "-c"]

# Ensure that gethostbyaddr() returns our hostname
RUN sed -i -e 's/^hosts:.*/& myhostname/' /etc/nsswitch.conf

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
RUN apt update && apt install -y \
    apt-transport-https \
    gnupg \
    wget \
    && apt clean

# Add HTCondor repository
RUN wget -qO - https://research.cs.wisc.edu/htcondor/debian/HTCondor-Release.gpg.key | apt-key add -
RUN echo "deb [arch=amd64] http://research.cs.wisc.edu/htcondor/repo/ubuntu/8.9 bionic main" > /etc/apt/sources.list.d/htcondor.list

# Need HTCondor to build HTCondor (BatLab style)
RUN apt update && DEBIAN_FRONTEND=noninteractive apt install -y \
    htcondor \
    libnss-myhostname \
    openssh-server \
    && apt clean

# Tools needed for building
RUN apt update && apt install -y \
    build-essential \
    devscripts \
    && apt clean

# HTCondor build dependencies
RUN apt update && DEBIAN_FRONTEND=noninteractive apt install -y \
    autoconf \
    automake \
    bison \
    chrpath \
    cmake \
    debhelper \
    default-jdk \
    dh-autoreconf \
    dh-python \
    fakeroot \
    flex \
    help2man \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-python-dev \
    libboost-regex-dev \
    libboost-system-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libc-ares-dev \
    libcgroup-dev \
    libcurl4-openssl-dev \
    libdistro-info-perl \
    libexpat1-dev \
    libglobus-ftp-client-dev \
    libglobus-gass-server-ez-dev \
    libglobus-gram-client-dev \
    libglobus-gram-protocol-dev \
    libglobus-gss-assist-dev \
    libkrb5-dev \
    libldap2-dev \
    libmunge-dev \
    libpam0g-dev \
    libpcre3-dev \
    libpq-dev \
    libsqlite3-dev \
    libtool \
    libtool-bin \
    libvirt-dev \
    libx11-dev \
    libxml2-dev \
    libxss-dev \
    patchelf \
    pkg-config \
    po-debconf \
    python3-all-dev \
    python3-setuptools \
    python-dev \
    python-setuptools \
    python-sphinx \
    python-sphinx-rtd-theme \
    uuid-dev \
    voms-dev \
    zlib1g-dev \
    && apt clean

RUN apt update && apt install -y \
    python3-pip \
    && apt clean

RUN pip3 install pytest pytest-httpserver
