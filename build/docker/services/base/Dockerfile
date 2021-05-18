# This is the base image for HTCondor; no role-specific config
ARG BASE_IMAGE=centos:7
FROM ${BASE_IMAGE}
ARG BASE_IMAGE
# ^^ Previous BASE_IMAGE has gone out of scope

ARG VERSION=latest
ARG SERIES=9.0

LABEL org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.title="HTCondor Base Image derived from ${BASE_IMAGE}" \
      org.opencontainers.image.vendor="HTCondor"

# Unset these from parent
LABEL \
    build-date="" \
    io.buildah.version="" \
    license="" \
    name="" \
    org.label-schema.build-date="" \
    org.label-schema.license="" \
    org.label-schema.name="" \
    org.label-schema.schema-version="" \
    org.label-schema.vendor="" \
    org.opencontainers.image.created="" \
    vendor=""

COPY container-install-htcondor.sh /root/container-install-htcondor.sh
RUN \
  export HTCONDOR_VERSION=${VERSION} HTCONDOR_SERIES=${SERIES}; \
  bash -x /root/container-install-htcondor.sh

COPY condor/*.conf /etc/condor/config.d/
COPY supervisord.conf /etc/supervisord.conf
RUN mkdir -p /root/config
COPY pre-exec.sh /root/config/pre-exec.sh
COPY start.sh /
COPY update-config /
COPY update-secrets /
RUN chmod +x \
  /root/config/pre-exec.sh\
  /start.sh \
  /update-config \
  /update-secrets

CMD ["/bin/bash", "-x", "/start.sh"]

