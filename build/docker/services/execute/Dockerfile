ARG BASE_IMAGE=htcondor/base:el7
FROM ${BASE_IMAGE}
ARG BASE_IMAGE
ARG PACKAGE_LIST

LABEL org.opencontainers.image.title="HTCondor Execute image derived from ${BASE_IMAGE}"

# Create pslots slot users slot1_1 through slot1_64
RUN \
   for n in `seq 1 64`; do \
       useradd slot1_${n} || exit $?; \
   done

COPY $PACKAGE_LIST /root/extra-packagelist.txt
COPY container-install-execute.sh /root/container-install-execute.sh
RUN bash -x /root/container-install-execute.sh /root/extra-packagelist.txt

COPY condor/*.conf /etc/condor/config.d/

