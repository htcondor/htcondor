FROM centos:7
ENV container docker
RUN (cd /lib/systemd/system/sysinit.target.wants/; for i in *; do [ $i == \
systemd-tmpfiles-setup.service ] || rm -f $i; done); \
rm -f /lib/systemd/system/multi-user.target.wants/*;\
rm -f /etc/systemd/system/*.wants/*;\
rm -f /lib/systemd/system/local-fs.target.wants/*; \
rm -f /lib/systemd/system/sockets.target.wants/*udev*; \
rm -f /lib/systemd/system/sockets.target.wants/*initctl*; \
rm -f /lib/systemd/system/basic.target.wants/*;\
rm -f /lib/systemd/system/anaconda.target.wants/*;
VOLUME [ "/sys/fs/cgroup" ]

# HTCondor User details:
ENV SUBMIT_USER submitter
ENV GID 1000
ENV UID 1000
ENV PASS password

# Build in one RUN
RUN yum -y install \
         yum-utils \
         sudo \
         which \
         openssh-clients && \
    yum -y groupinstall 'Development Tools' 

RUN curl -L -s http://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor > RPM-GPG-KEY-HTCondor && \
    rpm --import RPM-GPG-KEY-HTCondor && \
    yum-config-manager --add-repo https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-development-rhel7.repo && \
    yum clean all && \
    rm -f RPM-GPG-KEY-HTCondor && \
    yum -y install condor && \
    groupadd -g ${GID} ${SUBMIT_USER} && \
    useradd -m -u ${UID} -g ${GID} ${SUBMIT_USER} && \
    usermod -a -G condor ${SUBMIT_USER} && \
    echo ${PASS} | passwd --stdin ${SUBMIT_USER} && \
    sed -i 's/\(^Defaults.*requiretty.*\)/#\1/' /etc/sudoers && \
    systemctl enable condor

RUN yum -y install epel-release httpd mod_wsgi mod_ssl net-tools vim
RUN systemctl enable httpd

COPY docker/hostkey.pem /etc/pki/tls/private/localhost.key
COPY docker/hostcert.pem /etc/pki/tls/certs/localhost.crt
COPY docker/igtf-ca-bundle.crt /etc/pki/tls/certs/server-chain.crt
RUN mv /etc/httpd/conf.d/ssl.conf /etc/httpd/conf.d/ssl.conf.orig && \
    sed 's/#SSLCertificateChainFile/SSLCertificateChainFile/g' /etc/httpd/conf.d/ssl.conf.orig > /etc/httpd/conf.d/ssl.conf && \
    rm -f /etc/httpd/conf.d/ssl.conf.orig

RUN yum -y install python2-pip
RUN pip install --upgrade pip
RUN pip install git+https://github.com/htcondor/scitokens-credmon

# KNOBS
COPY examples/config/condor/50-scitokens-credmon.conf /etc/condor/config.d/
RUN sed -r -i 's/# ?SEC_DEFAULT_ENCRYPTION/SEC_DEFAULT_ENCRYPTION/' /etc/condor/config.d/50-scitokens-credmon.conf
COPY examples/config/apache/scitokens_credmon.conf /etc/httpd/conf.d/
COPY examples/wsgi/scitokens-credmon.wsgi /var/www/wsgi-scripts/scitokens-credmon/
COPY docker/10-docker.conf /etc/condor/config.d/
COPY docker/60-oauth-token-providers.conf.tmpl /etc/condor/config.d/60-oauth-token-providers.conf.tmpl
RUN mkdir -p $(condor_config_val SEC_CREDENTIAL_DIRECTORY) && \
    chgrp condor $(condor_config_val SEC_CREDENTIAL_DIRECTORY) && \
    chmod 2770 $(condor_config_val SEC_CREDENTIAL_DIRECTORY)

ARG SCITOKENS_CLIENT_ID=clientid
ARG SCITOKENS_CLIENT_SECRET=clientsecret
ARG SCITOKENS_RETURN_URL_SUFFIX=/return/scitokens
ARG SCITOKENS_AUTHORIZATION_URL=https://token.server.address:443/scitokens-server/authorize
ARG SCITOKENS_TOKEN_URL=https://token.server.address:443/scitokens-server/token
ARG SCITOKENS_USER_URL=https://token.server.address:443/scitokens-server/userinfo

RUN sed s+__CLIENT_ID__+${SCITOKENS_CLIENT_ID}+g /etc/condor/config.d/60-oauth-token-providers.conf.tmpl | \
    sed s+__RETURN_URL_SUFFIX__+${SCITOKENS_RETURN_URL_SUFFIX}+g | \
    sed s+__AUTHORIZATION_URL__+${SCITOKENS_AUTHORIZATION_URL}+g | \
    sed s+__TOKEN_URL__+${SCITOKENS_TOKEN_URL}+g | \
    sed s+__USER_URL__+${SCITOKENS_USER_URL}+g >> /etc/condor/config.d/60-oauth-token-providers.conf && \
    rm -f /etc/condor/config.d/60-oauth-token-providers.conf.tmpl && \
    mkdir /etc/condor/.secrets && \
    echo ${SCITOKENS_CLIENT_SECRET} > /etc/condor/.secrets/scitokens && \
    chmod 600 /etc/condor/.secrets/scitokens

COPY docker/test.sub /home/${SUBMIT_USER}/test.sub
COPY docker/test.sh /home/${SUBMIT_USER}/test.sh
RUN chown ${UID}:${GID} /home/${SUBMIT_USER}/test.sub
RUN chown ${UID}:${GID} /home/${SUBMIT_USER}/test.sh

CMD ["/usr/sbin/init"]
