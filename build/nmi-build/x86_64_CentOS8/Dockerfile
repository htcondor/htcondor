FROM centos:8

# Add users that might be used in CHTC
# The HTCondor that runs inside the container needs to have the user defined
RUN useradd --uid  5001 --shell /bin/bash --create-home slot1 && \
    useradd --uid  5002 --shell /bin/bash --create-home slot2 && \
    useradd --uid  5003 --shell /bin/bash --create-home slot3 && \
    useradd --uid  5004 --shell /bin/bash --create-home slot4 && \
    useradd --uid  5005 --shell /bin/bash --create-home slot5 && \
    useradd --uid  5006 --shell /bin/bash --create-home slot6 && \
    useradd --uid  5007 --shell /bin/bash --create-home slot7 && \
    useradd --uid  5008 --shell /bin/bash --create-home slot8 && \
    useradd --uid  5009 --shell /bin/bash --create-home slot9 && \
    useradd --uid  5010 --shell /bin/bash --create-home slot10 && \
    useradd --uid  5011 --shell /bin/bash --create-home slot11 && \
    useradd --uid  5012 --shell /bin/bash --create-home slot12 && \
    useradd --uid  5013 --shell /bin/bash --create-home slot13 && \
    useradd --uid  5014 --shell /bin/bash --create-home slot14 && \
    useradd --uid  5015 --shell /bin/bash --create-home slot15 && \
    useradd --uid  5016 --shell /bin/bash --create-home slot16 && \
    useradd --uid  5300 --shell /bin/bash --create-home slot1_1 && \
    useradd --uid  5301 --shell /bin/bash --create-home slot1_2 && \
    useradd --uid  5302 --shell /bin/bash --create-home slot1_3 && \
    useradd --uid  5303 --shell /bin/bash --create-home slot1_4 && \
    useradd --uid  5304 --shell /bin/bash --create-home slot1_5 && \
    useradd --uid  5305 --shell /bin/bash --create-home slot1_6 && \
    useradd --uid  5306 --shell /bin/bash --create-home slot1_7 && \
    useradd --uid  5307 --shell /bin/bash --create-home slot1_8 && \
    useradd --uid  5308 --shell /bin/bash --create-home slot1_9 && \
    useradd --uid  5309 --shell /bin/bash --create-home slot1_10 && \
    useradd --uid  5310 --shell /bin/bash --create-home slot1_11 && \
    useradd --uid  5311 --shell /bin/bash --create-home slot1_12 && \
    useradd --uid  5312 --shell /bin/bash --create-home slot1_13 && \
    useradd --uid  5313 --shell /bin/bash --create-home slot1_14 && \
    useradd --uid  5314 --shell /bin/bash --create-home slot1_15 && \
    useradd --uid  5315 --shell /bin/bash --create-home slot1_16 && \
    useradd --uid  5316 --shell /bin/bash --create-home slot1_17 && \
    useradd --uid  5317 --shell /bin/bash --create-home slot1_18 && \
    useradd --uid  5318 --shell /bin/bash --create-home slot1_19 && \
    useradd --uid  5319 --shell /bin/bash --create-home slot1_20 && \
    useradd --uid  5320 --shell /bin/bash --create-home slot1_21 && \
    useradd --uid  5321 --shell /bin/bash --create-home slot1_22 && \
    useradd --uid  5322 --shell /bin/bash --create-home slot1_23 && \
    useradd --uid  5323 --shell /bin/bash --create-home slot1_24 && \
    useradd --uid  5324 --shell /bin/bash --create-home slot1_25 && \
    useradd --uid  5325 --shell /bin/bash --create-home slot1_26 && \
    useradd --uid  5326 --shell /bin/bash --create-home slot1_27 && \
    useradd --uid  5327 --shell /bin/bash --create-home slot1_28 && \
    useradd --uid  5328 --shell /bin/bash --create-home slot1_29 && \
    useradd --uid  5329 --shell /bin/bash --create-home slot1_30 && \
    useradd --uid  5330 --shell /bin/bash --create-home slot1_31 && \
    useradd --uid  5331 --shell /bin/bash --create-home slot1_32 && \
    useradd --uid  5332 --shell /bin/bash --create-home slot1_33 && \
    useradd --uid  5333 --shell /bin/bash --create-home slot1_34 && \
    useradd --uid  5334 --shell /bin/bash --create-home slot1_35 && \
    useradd --uid  5335 --shell /bin/bash --create-home slot1_36 && \
    useradd --uid  5336 --shell /bin/bash --create-home slot1_37 && \
    useradd --uid  5337 --shell /bin/bash --create-home slot1_38 && \
    useradd --uid  5338 --shell /bin/bash --create-home slot1_39 && \
    useradd --uid  5339 --shell /bin/bash --create-home slot1_40 && \
    useradd --uid  5340 --shell /bin/bash --create-home slot1_41 && \
    useradd --uid  5341 --shell /bin/bash --create-home slot1_42 && \
    useradd --uid  5342 --shell /bin/bash --create-home slot1_43 && \
    useradd --uid  5343 --shell /bin/bash --create-home slot1_44 && \
    useradd --uid  5344 --shell /bin/bash --create-home slot1_45 && \
    useradd --uid  5345 --shell /bin/bash --create-home slot1_46 && \
    useradd --uid  5346 --shell /bin/bash --create-home slot1_47 && \
    useradd --uid  5347 --shell /bin/bash --create-home slot1_48 && \
    useradd --uid  5348 --shell /bin/bash --create-home slot1_49 && \
    useradd --uid  5349 --shell /bin/bash --create-home slot1_50 && \
    useradd --uid  5350 --shell /bin/bash --create-home slot1_51 && \
    useradd --uid  5351 --shell /bin/bash --create-home slot1_52 && \
    useradd --uid  5352 --shell /bin/bash --create-home slot1_53 && \
    useradd --uid  5353 --shell /bin/bash --create-home slot1_54 && \
    useradd --uid  5354 --shell /bin/bash --create-home slot1_55 && \
    useradd --uid  5355 --shell /bin/bash --create-home slot1_56 && \
    useradd --uid  5356 --shell /bin/bash --create-home slot1_57 && \
    useradd --uid  5357 --shell /bin/bash --create-home slot1_58 && \
    useradd --uid  5358 --shell /bin/bash --create-home slot1_59 && \
    useradd --uid  5359 --shell /bin/bash --create-home slot1_60 && \
    useradd --uid  5360 --shell /bin/bash --create-home slot1_61 && \
    useradd --uid  5361 --shell /bin/bash --create-home slot1_62 && \
    useradd --uid  5362 --shell /bin/bash --create-home slot1_63 && \
    useradd --uid  5363 --shell /bin/bash --create-home slot1_64 && \
    useradd --uid  5364 --shell /bin/bash --create-home slot1_65 && \
    useradd --uid  5365 --shell /bin/bash --create-home slot1_66 && \
    useradd --uid  5366 --shell /bin/bash --create-home slot1_67 && \
    useradd --uid  5367 --shell /bin/bash --create-home slot1_68 && \
    useradd --uid  5368 --shell /bin/bash --create-home slot1_69 && \
    useradd --uid  5369 --shell /bin/bash --create-home slot1_70 && \
    useradd --uid  5370 --shell /bin/bash --create-home slot1_71 && \
    useradd --uid  5371 --shell /bin/bash --create-home slot1_72 && \
    useradd --uid  5372 --shell /bin/bash --create-home slot1_73 && \
    useradd --uid  5373 --shell /bin/bash --create-home slot1_74 && \
    useradd --uid  5374 --shell /bin/bash --create-home slot1_75 && \
    useradd --uid  5375 --shell /bin/bash --create-home slot1_76 && \
    useradd --uid  5376 --shell /bin/bash --create-home slot1_77 && \
    useradd --uid  5377 --shell /bin/bash --create-home slot1_78 && \
    useradd --uid  5378 --shell /bin/bash --create-home slot1_79 && \
    useradd --uid  5379 --shell /bin/bash --create-home slot1_80 && \
    useradd --uid  5380 --shell /bin/bash --create-home slot1_81 && \
    useradd --uid  5381 --shell /bin/bash --create-home slot1_82 && \
    useradd --uid  5382 --shell /bin/bash --create-home slot1_83 && \
    useradd --uid  5383 --shell /bin/bash --create-home slot1_84 && \
    useradd --uid  5384 --shell /bin/bash --create-home slot1_85 && \
    useradd --uid  5385 --shell /bin/bash --create-home slot1_86 && \
    useradd --uid  5386 --shell /bin/bash --create-home slot1_87 && \
    useradd --uid  5387 --shell /bin/bash --create-home slot1_88 && \
    useradd --uid  5388 --shell /bin/bash --create-home slot1_89 && \
    useradd --uid  5389 --shell /bin/bash --create-home slot1_90 && \
    useradd --uid  5390 --shell /bin/bash --create-home slot1_91 && \
    useradd --uid  5391 --shell /bin/bash --create-home slot1_92 && \
    useradd --uid  5392 --shell /bin/bash --create-home slot1_93 && \
    useradd --uid  5393 --shell /bin/bash --create-home slot1_94 && \
    useradd --uid  5394 --shell /bin/bash --create-home slot1_95 && \
    useradd --uid  5395 --shell /bin/bash --create-home slot1_96 && \
    useradd --uid  5396 --shell /bin/bash --create-home slot1_97 && \
    useradd --uid  5397 --shell /bin/bash --create-home slot1_98 && \
    useradd --uid  5398 --shell /bin/bash --create-home slot1_99 && \
    useradd --uid  5399 --shell /bin/bash --create-home slot1_100 && \
    useradd --uid  5400 --shell /bin/bash --create-home slot1_101 && \
    useradd --uid  5401 --shell /bin/bash --create-home slot1_102 && \
    useradd --uid  5402 --shell /bin/bash --create-home slot1_103 && \
    useradd --uid  5403 --shell /bin/bash --create-home slot1_104 && \
    useradd --uid  5404 --shell /bin/bash --create-home slot1_105 && \
    useradd --uid  5405 --shell /bin/bash --create-home slot1_106 && \
    useradd --uid  5406 --shell /bin/bash --create-home slot1_107 && \
    useradd --uid  5407 --shell /bin/bash --create-home slot1_108 && \
    useradd --uid  5408 --shell /bin/bash --create-home slot1_109 && \
    useradd --uid  5409 --shell /bin/bash --create-home slot1_110 && \
    useradd --uid  5410 --shell /bin/bash --create-home slot1_111 && \
    useradd --uid  5411 --shell /bin/bash --create-home slot1_112 && \
    useradd --uid  5412 --shell /bin/bash --create-home slot1_113 && \
    useradd --uid  5413 --shell /bin/bash --create-home slot1_114 && \
    useradd --uid  5414 --shell /bin/bash --create-home slot1_115 && \
    useradd --uid  5415 --shell /bin/bash --create-home slot1_116 && \
    useradd --uid  5416 --shell /bin/bash --create-home slot1_117 && \
    useradd --uid  5417 --shell /bin/bash --create-home slot1_118 && \
    useradd --uid  5418 --shell /bin/bash --create-home slot1_119 && \
    useradd --uid  5419 --shell /bin/bash --create-home slot1_120 && \
    useradd --uid  5420 --shell /bin/bash --create-home slot1_121 && \
    useradd --uid  5421 --shell /bin/bash --create-home slot1_122 && \
    useradd --uid  5422 --shell /bin/bash --create-home slot1_123 && \
    useradd --uid  5423 --shell /bin/bash --create-home slot1_124 && \
    useradd --uid  5424 --shell /bin/bash --create-home slot1_125 && \
    useradd --uid  5425 --shell /bin/bash --create-home slot1_126 && \
    useradd --uid  5426 --shell /bin/bash --create-home slot1_127 && \
    useradd --uid  5427 --shell /bin/bash --create-home slot1_128 && \
    useradd --uid  5428 --shell /bin/bash --create-home slot1_129 && \
    useradd --uid  5429 --shell /bin/bash --create-home slot1_130 && \
    useradd --uid  5430 --shell /bin/bash --create-home slot1_131 && \
    useradd --uid  5431 --shell /bin/bash --create-home slot1_132 && \
    useradd --uid  5432 --shell /bin/bash --create-home slot1_133 && \
    useradd --uid  5433 --shell /bin/bash --create-home slot1_134 && \
    useradd --uid  5434 --shell /bin/bash --create-home slot1_135 && \
    useradd --uid  5435 --shell /bin/bash --create-home slot1_136 && \
    useradd --uid  5436 --shell /bin/bash --create-home slot1_137 && \
    useradd --uid  5437 --shell /bin/bash --create-home slot1_138 && \
    useradd --uid  5438 --shell /bin/bash --create-home slot1_139 && \
    useradd --uid  5439 --shell /bin/bash --create-home slot1_140 && \
    useradd --uid  5440 --shell /bin/bash --create-home slot1_141 && \
    useradd --uid  5441 --shell /bin/bash --create-home slot1_142 && \
    useradd --uid  5442 --shell /bin/bash --create-home slot1_143 && \
    useradd --uid  5443 --shell /bin/bash --create-home slot1_144 && \
    useradd --uid  5444 --shell /bin/bash --create-home slot1_145 && \
    useradd --uid  5445 --shell /bin/bash --create-home slot1_146 && \
    useradd --uid  5446 --shell /bin/bash --create-home slot1_147 && \
    useradd --uid  5447 --shell /bin/bash --create-home slot1_148 && \
    useradd --uid  5448 --shell /bin/bash --create-home slot1_149 && \
    useradd --uid  5449 --shell /bin/bash --create-home slot1_150 && \
    useradd --uid  5450 --shell /bin/bash --create-home slot1_151 && \
    useradd --uid  5451 --shell /bin/bash --create-home slot1_152 && \
    useradd --uid  5452 --shell /bin/bash --create-home slot1_153 && \
    useradd --uid  5453 --shell /bin/bash --create-home slot1_154 && \
    useradd --uid  5454 --shell /bin/bash --create-home slot1_155 && \
    useradd --uid  5455 --shell /bin/bash --create-home slot1_156 && \
    useradd --uid  5456 --shell /bin/bash --create-home slot1_157 && \
    useradd --uid  5457 --shell /bin/bash --create-home slot1_158 && \
    useradd --uid  5458 --shell /bin/bash --create-home slot1_159 && \
    useradd --uid  5459 --shell /bin/bash --create-home slot1_160 && \
    useradd --uid  5460 --shell /bin/bash --create-home slot1_161 && \
    useradd --uid  6004 --shell /bin/bash --create-home condorauto && \
    useradd --uid 22537 --shell /bin/bash --create-home bbockelm && \
    useradd --uid 20343 --shell /bin/bash --create-home blin && \
    useradd --uid 20003 --shell /bin/bash --create-home cat && \
    useradd --uid 20849 --shell /bin/bash --create-home ckoch5 && \
    useradd --uid 21640 --shell /bin/bash --create-home coatsworth && \
    useradd --uid 20342 --shell /bin/bash --create-home edquist && \
    useradd --uid 20006 --shell /bin/bash --create-home gthain && \
    useradd --uid 20839 --shell /bin/bash --create-home iaross && \
    useradd --uid 21356 --shell /bin/bash --create-home jcpatton && \
    useradd --uid 20007 --shell /bin/bash --create-home jfrey && \
    useradd --uid 23056 --shell /bin/bash --create-home jmvera && \
    useradd --uid 20018 --shell /bin/bash --create-home johnkn && \
    useradd --uid 22968 --shell /bin/bash --create-home kcramer && \
    useradd --uid 20005 --shell /bin/bash --create-home kupsch && \
    useradd --uid 20383 --shell /bin/bash --create-home lmichael && \
    useradd --uid 20020 --shell /bin/bash --create-home matyas && \
    useradd --uid 10006 --shell /bin/bash --create-home moate && \
    useradd --uid 20013 --shell /bin/bash --create-home tannenba && \
    useradd --uid 20345 --shell /bin/bash --create-home tim && \
    useradd --uid 22109 --shell /bin/bash --create-home tjslauson && \
    useradd --uid 20015 --shell /bin/bash --create-home tlmiller && \
    useradd --uid 20017 --shell /bin/bash --create-home zmiller

# Fake package id
RUN mkdir /home/batlab-admin && \
    echo 0 > /home/batlab-admin/package_id

# Add tools to add the HTCondor repository
RUN dnf install -y \
    'dnf-command(config-manager)' \
    epel-release \
    wget \
    && dnf config-manager --set-enabled PowerTools \
    && dnf clean all

# Add HTCondor repository
RUN wget https://research.cs.wisc.edu/htcondor/yum/RPM-GPG-KEY-HTCondor && \
    rpm --import RPM-GPG-KEY-HTCondor && \
    cd /etc/yum.repos.d && \
    wget https://research.cs.wisc.edu/htcondor/yum/repo.d/htcondor-stable-rhel8.repo

# Need HTCondor to build and test HTCondor (BaTLab style)
RUN dnf install -y \
    condor \
    java \
    openssh-clients \
    openssh-server \
    'perl(Net::Domain)' \
    'perl(Time::HiRes)' \
    which \
    && dnf clean all

# Tools needed for building
RUN dnf install -y \
    rpm-build \
    make \
    && dnf clean all

# HTCondor build dependencies
RUN dnf install -y \
    'bind-utils' \
    'bison' \
    'bison-devel' \
    'boost-devel' \
    'boost-python3-devel' \
    'boost-static' \
    'byacc' \
    'cmake' \
    'expat-devel' \
    'flex' \
    'gcc-c++' \
    'glibc-static' \
    'krb5-devel' \
    'libcgroup' \
    'libcurl-devel' \
    'libstdc++-static' \
    'libtool' \
    'libtool-ltdl-devel' \
    'libuuid-devel' \
    'libvirt-devel' \
    'libX11-devel' \
    'libxml2-devel' \
    'libXScrnSaver-devel' \
    'm4' \
    'nss-devel' \
    'openldap-devel' \
    'openssl-devel' \
    'pam-devel' \
    'patch' \
    'pcre-devel' \
    'perl(Archive::Tar)' \
    'perl(Data::Dumper)' \
    'perl(Digest::MD5)' \
    'perl(Digest::SHA)' \
    'perl(Env)' \
    'perl(XML::Parser)' \
    'python3-devel' \
    'python3-sphinx' \
    'python3-sphinx_rtd_theme' \
    'redhat-rpm-config' \
    'sqlite-devel' \
    'systemd-devel' \
    'systemd-units' \
    && dnf clean all

RUN pip3 install pytest pytest-httpserver
