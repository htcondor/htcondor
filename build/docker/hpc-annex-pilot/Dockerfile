# HTCondor for EL8 is built on Almalinux 8; use that
FROM library/almalinux:8

LABEL org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.title="HTCondor HPC Annex pilot image" \
      org.opencontainers.image.vendor="HTCondor"

COPY install-pilot.sh /root/install-pilot.sh
RUN chmod +x /root/install-pilot.sh && /root/install-pilot.sh
 