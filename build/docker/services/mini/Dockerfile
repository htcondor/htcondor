# This is a minicondor image; runs all the daemons for a single-node pool
# Become the "submituser" user to submit jobs.
ARG EL
ARG VERSION
ARG SUFFIX

FROM htcondor/base:${VERSION}-el${EL}${SUFFIX}

ARG EL
ARG VERSION
ARG BUILDDATE
ARG SUFFIX

# https://label-schema.org/rc1
LABEL org.label-schema.name="htcondor/mini:${VERSION}-el${EL}${SUFFIX}" \
      org.label-schema.description="HTCondor ${VERSION} minicondor image for RHEL ${EL}-compatible" \
      org.label-schema.vendor="HTCondor" \
      org.label-schema.license="Apache-2.0"

# perl is needed for condor_run
RUN \
    yum install -y "minicondor = ${VERSION}" perl && \
    yum clean all && \
    rm -rf /var/cache/yum/*

RUN useradd submituser

# Remove files from base condor that are unused
RUN rm -f \
    /start.sh \
    /update-config \
    /update-secrets \
    /etc/condor/config.d/01-security.conf

# Call supervisord directly (we got rid of start.sh)
CMD ["/usr/bin/supervisord", "-c", "/etc/supervisord.conf"]

LABEL org.label-schema.build-date="${BUILDDATE}"
