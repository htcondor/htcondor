# This is the base image for HTCondor; no role-specific config
ARG EL

FROM centos:${EL}

# "ARG EL" needs to be here again because the previous instance has gone out of scope.
ARG EL
ARG BUILDDATE
ARG VERSION
ARG SUFFIX

# https://label-schema.org/rc1
LABEL org.label-schema.name="htcondor/base:${VERSION}-el${EL}${SUFFIX}" \
      org.label-schema.description="HTCondor ${VERSION} base image for RHEL ${EL}-compatible" \
      org.label-schema.vendor="HTCondor" \
      org.label-schema.license="Apache-2.0"

# EPEL is needed for supervisor
# As of 2020-01-03, the GPG key must be downloaded separately
# openssh-clients and openssh-server are for condor_ssh_to_job
RUN \
    # Get the release series based on the middle version
    # odd numbers = development; even numbers = stable
    tmp=${VERSION%.*}; \
    tmp=${tmp#*.}; \
    if (( (tmp % 2) == 1 )); then \
        SERIES=development; \
    else \
        SERIES=stable; \
    fi; \
    \
    yum -y update && \
    yum -y install epel-release yum-plugin-priorities && \
    \
    (curl -sSL https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-${SERIES}-rhel${EL}.repo && \
     echo "gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-HTCondor" \
     ) > /etc/yum.repos.d/htcondor-${SERIES}-rhel${EL}.repo  && \
    curl -sSL https://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor \
      -o /etc/pki/rpm-gpg/RPM-GPG-KEY-HTCondor  && \
    rpm --import /etc/pki/rpm-gpg/*  && \
    \
    yum -y install "condor = ${VERSION}" supervisor openssh-clients openssh-server && \
    yum clean all && \
    rm -rf /var/cache/yum/*

# Create passwords directories for token or pool password auth.
#
# Only root needs to know the pool password but other condor daemons
# need access to the tokens.
#
# In the future, the RPMs will take care of this step.
RUN \
    install -m 0700 -o root -g root -d /etc/condor/passwords.d && \
    install -m 0700 -o condor -g condor -d /etc/condor/tokens.d

# We use supervisor for two reasons:
# - As of 8.9.4, the master can't run as pid 1 because it immediately dies
#   trying to send a signal to itself.
# - We want something to restart the master in case it dies.
#
# A wrapper script is used to copy configs from a config volume mounted
# at /root/config

COPY base/supervisord.conf /etc/supervisord.conf
COPY base/condor/*.conf /etc/condor/config.d/
COPY base/start.sh base/update-config base/update-secrets /

CMD ["/bin/bash", "-x", "/start.sh"]

LABEL org.label-schema.build-date="${BUILDDATE}"
