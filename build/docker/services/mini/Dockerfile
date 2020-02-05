# This is a minicondor image; runs all the daemons for a single-node pool
# Become the "submituser" user to submit jobs.
ARG EL
ARG VERSION

FROM htcondor/base:${VERSION}-el${EL}

ARG EL
ARG VERSION
ARG BUILDDATE

# https://label-schema.org/rc1
LABEL org.label-schema.name="htcondor/mini:${VERSION}-el${EL}" \
      org.label-schema.description="HTCondor ${VERSION} minicondor image for RHEL ${EL}-compatible" \
      org.label-schema.vendor="HTCondor" \
      org.label-schema.license="Apache-2.0"

# perl is needed for condor_run
RUN \
    yum install -y "minicondor = ${VERSION}" perl && \
    yum clean all && \
    rm -rf /var/cache/yum/*

RUN useradd submituser

LABEL org.label-schema.build-date="${BUILDDATE}"
