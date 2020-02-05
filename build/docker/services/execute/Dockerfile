# This is an execute node image for HTCondor
ARG EL
ARG VERSION

FROM htcondor/base:${VERSION}-el${EL}

ARG EL
ARG VERSION
ARG BUILDDATE

# https://label-schema.org/rc1
LABEL org.label-schema.name="htcondor/execute:${VERSION}-el${EL}" \
      org.label-schema.description="HTCondor ${VERSION} execute image for RHEL ${EL}-compatible" \
      org.label-schema.vendor="HTCondor" \
      org.label-schema.license="Apache-2.0"

# Create slot users slot1 through slot64
RUN \
   for n in `seq 1 64`; do \
       useradd slot${n}; \
   done

# Install extra packages commonly used by jobs
COPY execute/packagelist.txt /tmp/packagelist.txt
RUN \
    grep '^[A-Za-z]' /tmp/packagelist.txt \
    | xargs -r yum -y install; \
    yum clean all; \
    rm -rf /var/cache/yum/*; \
    rm /tmp/packagelist.txt

COPY execute/condor/*.conf /etc/condor/config.d/

LABEL org.label-schema.build-date="${BUILDDATE}"
