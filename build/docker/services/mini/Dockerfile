ARG BASE_IMAGE=htcondor/base:el7
FROM ${BASE_IMAGE}
ARG BASE_IMAGE

LABEL org.opencontainers.image.title="HTCondor Minicondor image derived from ${BASE_IMAGE}"


COPY 00-minicondor-ubuntu /tmp/00-minicondor-ubuntu
COPY container-install-minicondor.sh /root/container-install-minicondor.sh
RUN bash -x /root/container-install-minicondor.sh

RUN $(command -v pip-3 || command -v pip3) install git+https://github.com/htcondor/htcondor-restd.git@master\#egg=htcondor-restd

COPY supervisord.conf /etc/supervisord.conf
COPY start.sh /start.sh
COPY condor_restd.sh /usr/local/bin/condor_restd.sh
RUN chmod +x \
    /start.sh \
    /usr/local/bin/condor_restd.sh

# Remove files from base image that are unused
RUN rm -f \
    /update-config \
    /update-secrets \
    /etc/condor/config.d/01-security.conf

EXPOSE 8080

CMD ["/start.sh"]

