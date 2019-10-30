########################
# Docker file to setup a proper build environment for HTCondor
# Centos
# Todd Tannenbaum Oct 2019

# Starting with Centos7
FROM centos:centos7

# Create a build user, and allow that user to sudo
RUN adduser build && mkdir /condor && usermod -aG wheel build && echo "%wheel  ALL=(ALL)   NOPASSWD:ALL" >> /etc/sudoers

RUN yum install -y epel-release \
 && yum install -y cmake3 ninja-build where which sudo less wget \
 && yum clean all

# Install the minimal things to make a tar build without lots of externals like GSI, man pages, MUNGE
#
RUN yum install -y systemtap-sdt-devel patch c-ares-devel autoconf automake libtool perl-Time-HiRes perl-Archive-Tar perl-XML-Parser perl-Digest-MD5 gcc-c++ make flex byacc pcre-devel openssl-devel krb5-devel libvirt-devel bind-utils m4 libX11-devel libXScrnSaver-devel curl-devel expat-devel openldap-devel python-devel redhat-rpm-config  sqlite-devel glibc-static libuuid-devel bison bison-devel libtool-ltdl-devel pam-devel nss-devel libxml2-devel libstdc++-devel libstdc++-static python36-devel boost169-devel boost169-static boost169-python2 boost169-python3 \
 && yum clean all

# Install all the rest of the dependecies for a UW style RPM build, e.g. GSI, CREAM, etc.
#
# Note: a lot of this stuff is still listed as required in the RPM
# spec file, but really is not longer needed.  Like all the LaTeX stuff.  Sigh.
#
RUN yum install -y rpm-build cmake perl-Data-Dumper glibc-devel libcurl-devel globus-gssapi-gsi-devel globus-gass-server-ez-devel globus-gass-transfer-devel globus-gram-client-devel globus-rsl-devel globus-gram-protocol globus-io-devel globus-xio-devel globus-gssapi-error-devel globus-gss-assist-devel globus-gsi-proxy-core-devel globus-gsi-credential-devel globus-gsi-callback-devel globus-gsi-sysconfig-devel globus-gsi-cert-utils-devel globus-openssl-module-devel globus-gsi-openssl-error-devel globus-gsi-proxy-ssl-devel globus-callout-devel globus-common-devel globus-ftp-client-devel globus-ftp-control-devel munge-devel voms-devel libcgroup-devel glite-ce-cream-client-devel glite-lbjp-common-gsoap-plugin-devel log4cpp-devel gridsite-devel systemd-devel systemd-units boost-devel boost-python transfig latex2html \
 && yum clean all

USER build

# Install some pip package dependencies for the manual and man pages in docs/
#
RUN pip3 --no-cache install --user build sphinx sphinx_rtd_theme nbsphinx htcondor

# Default to running a bash login shell; we do this (the -l) so
# ~/.bash_profile gets sourced, which modifies the PATH for the
# build user so all the pip3 packages we installed above are found.
#
CMD ["/bin/bash", "-l"]

WORKDIR "/home/build/CONDOR_SRC"

