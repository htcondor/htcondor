ARG BASE_IMAGE=htcondor/base:el7
FROM ${BASE_IMAGE}
ARG BASE_IMAGE

LABEL org.opencontainers.image.title="HTCondor Central Manager image derived from ${BASE_IMAGE}"

COPY condor/*.conf /etc/condor/config.d/

EXPOSE 9618
