#!/bin/bash

#
# Install htcondor
#
for i in 1 2 3 4 5; do
    if curl -s https://research.cs.wisc.edu/htcondor/debian/HTCondor-Release.gpg.key | apt-key add -; then
        break;
    fi;
    sleep 2
done
echo "deb http://research.cs.wisc.edu/htcondor/debian/8.8/buster buster contrib" > /etc/apt/sources.list.d/htcondor.list
echo "deb-src http://research.cs.wisc.edu/htcondor/debian/8.8/buster buster contrib" >> /etc/apt/sources.list.d/htcondor.list
apt-get update
# FIXME: ask Tim how to control which repo this comes from -- the
# default 8.6.8 can't run jobs.
DEBIAN_FRONTEND=noninteractive apt-get -y install htcondor

#
# Write 98-static-annex.config
#
cat > /etc/condor/config.d/98-static-annex.config <<EOF
use role : execute
use security : host_based

LOCAL_HOSTS = \$(FULL_HOSTNAME) \$(IP_ADDRESS) 127.0.0.1 \$(TCP_FORWARDING_HOST)
CONDOR_HOST = condor_pool@*/* \$(LOCAL_HOSTS)

SEC_DEFAULT_AUTHENTICATION = REQUIRED
SEC_DEFAULT_AUTHENTICATION_METHODS = FS PASSWORD
SEC_DEFAULT_INTEGRITY = REQUIRED
SEC_DEFAULT_ENCRYPTION = REQUIRED

SEC_PASSWORD_FILE = /etc/condor/password_file
ALLOW_WRITE = condor_pool@*/* \$(LOCAL_HOSTS)

IsAnnex = TRUE
STARTD_ATTRS = $(STARTD_ATTRS) AnnexName IsAnnex
MASTER_ATTRS = $(MASTER_ATTRS) AnnexName IsAnnex
EOF

#
# Write 99-dynamic-annex.config
#
EXTERNAL_IP=`/usr/bin/curl -s -H "Metadata-Flavor: Google" http://metadata.google.internal/computeMetadata/v1/instance/network-interfaces/0/access-configs/0/external-ip`
ANNEX_NAME=`/usr/bin/curl -s -H "Metadata-Flavor: Google" http://metadata.google.internal/computeMetadata/v1/instance/attributes/htcondor_annex_name`
COLLECTOR_HOST=`/usr/bin/curl -s -H "Metadata-Flavor: Google" http://metadata.google.internal/computeMetadata/v1/instance/attributes/htcondor_pool_collector`

echo "# Generated" > /etc/condor/config.d/99-dynamic-annex.config
echo "TCP_FORWARDING_HOST = ${EXTERNAL_IP}" >> /etc/condor/config.d/99-dynamic-annex.config
echo "AnnexName = \"${ANNEX_NAME}\"" >> /etc/condor/config.d/99-dynamic-annex.config
echo "COLLECTOR_HOST = ${COLLECTOR_HOST}" >> /etc/condor/config.d/99-dynamic-annex.config

touch /etc/condor/password_file.base64
chmod 600 /etc/condor/password_file.base64
touch /etc/condor/password_file
chmod 600 /etc/condor/password_file

/usr/bin/curl -s -H "Metadata-Flavor: Google" http://metadata.google.internal/computeMetadata/v1/instance/attributes/htcondor_pool_password > /etc/condor/password_file.base64
/usr/bin/base64 -d /etc/condor/password_file.base64 > /etc/condor/password_file

#
# Start HTCondor
#
systemctl enable condor
systemctl start condor
