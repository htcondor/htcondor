ARG BASE_IMAGE=htcondor/base:el7
FROM ${BASE_IMAGE}
ARG BASE_IMAGE

LABEL org.opencontainers.image.title="HTCondor Submit image derived from ${BASE_IMAGE}"

COPY condor/*.conf /etc/condor/config.d/

# Add a test submitter user
RUN useradd -m submituser

EXPOSE 9618
