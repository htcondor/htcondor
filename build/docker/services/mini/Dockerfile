# This is a minicondor image; runs all the daemons for a single-node pool
# Designed for interactive use and experimentation;
# also includes man pages and htcondor-restd running on port 8080.

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


# perl is needed for condor_run. Reinstalling condor to get the man pages too.
# git and flask are needed for the restd; jq is too useful not to include.
RUN \
    yum install -y perl  && \
    yum reinstall -y --setopt=tsflags='' "*condor*" && \
    yum install -y --setopt=tsflags='' man "minicondor = ${VERSION}" && \
    \
    yum install -y git-core python3-flask jq &&  \
    \
    yum clean all && \
    rm -rf /var/cache/yum/*

RUN useradd restd
RUN useradd submituser

# flask_restful is not available in yum repos
RUN pip-3 install flask_restful
RUN pip-3 install git+https://github.com/htcondor/htcondor-restd.git@master\#egg=htcondor-restd

COPY mini/supervisord.conf /etc/supervisord.conf
COPY mini/condor_restd.sh /usr/local/bin/condor_restd.sh
COPY mini/start.sh /start.sh

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

LABEL org.label-schema.build-date="${BUILDDATE}"
