# This is a central manager image for HTCondor
ARG EL
ARG VERSION
ARG SUFFIX

FROM htcondor/base:${VERSION}-el${EL}${SUFFIX}

ARG EL
ARG VERSION
ARG BUILDDATE

# https://label-schema.org/rc1
LABEL org.label-schema.name="htcondor/cm:${VERSION}-el${EL}${SUFFIX}" \
      org.label-schema.description="HTCondor ${VERSION} central manager image for RHEL ${EL}-compatible" \
      org.label-schema.vendor="HTCondor" \
      org.label-schema.license="Apache-2.0"

COPY cm/condor/*.conf /etc/condor/config.d/

EXPOSE 9618

LABEL org.label-schema.build-date="${BUILDDATE}"
