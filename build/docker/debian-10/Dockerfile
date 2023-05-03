# Docker file to generate Debian-10 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/debian:buster

# Copy repo list files and scripts.
ADD sources.list snapshot.list dkh.sh /

# Install non-driver related packages.
RUN mv -f /sources.list /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
	ca-certificates \
    ssh \
    sudo \
    vim \
    wget \
    net-tools \
	pkg-config \
    make \
    autoconf \
    zlib1g-dev \
    gettext \
    m4 \
    gcc \
    g++ \
    dc \
    tzdata \
	openssl \
    rpm \
    libncurses-dev \
    uuid-dev \
    linux-compiler-gcc-8-x86 \
    linux-kbuild-4.19 \
    dos2unix && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/* && \
    ln -s /usr/bin/make /usr/bin/gmake

# Install Automake 1.16.3.
RUN wget http://ftp.gnu.org/gnu/automake/automake-1.16.3.tar.gz -P /Automake && \
	cd /Automake && \
	tar -xvzf automake-1.16.3.tar.gz && \
	cd automake-1.16.3 && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	rm -rf /Automake
	
RUN apt-get -o Acquire::Check-Valid-Until=false update && \
    apt-get install -y --no-install-recommends \
    libtool \
	libtool-bin
	
# Install Git v2.21.0.
RUN wget --no-check-certificate https://github.com/git/git/archive/v2.21.0.tar.gz -P /GitBuild && \
    cd /GitBuild && \
    tar -xvzf v2.21.0.tar.gz && \
    cd git-2.21.0 && \
    make configure && \
    ./configure --prefix=/usr && \
    make all && \
    make install && \
    /usr/bin/git --version && \
    rm -rf /GitBuild

# Install driver dependent packages.
RUN mv -f /snapshot.list /etc/apt/sources.list.d/snapshot.list && \
    apt-get -o Acquire::Check-Valid-Until=false update

# Download kernel headers and clean everything.
RUN mkdir -p /DKH && \
    mv -f /dkh.sh /DKH && \
    cd /DKH && chmod +x dkh.sh && ./dkh.sh > /var/log/dkh.log 2>&1 || \
    rm -rf /DKH && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/*

# Installation of 5.8.*-bpo and 5.9.*-bpo kernel headers needs specical handling.
# Adding snapshot repo doesn't help in downloading the kernel header
RUN wget --no-check-certificate https://snapshot.debian.org/archive/debian/20200929T210132Z/pool/main/l/linux/linux-kbuild-5.8_5.8.10-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20200929T210132Z/pool/main/l/linux/linux-headers-5.8.0-0.bpo.2-common_5.8.10-1~bpo10%2B1_all.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20200929T210132Z/pool/main/l/linux/linux-headers-5.8.0-0.bpo.2-amd64_5.8.10-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20200929T210132Z/pool/main/l/linux/linux-headers-5.8.0-0.bpo.2-cloud-amd64_5.8.10-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20201126T153155Z/pool/main/l/linux/linux-kbuild-5.9_5.9.6-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20201125T210657Z/pool/main/l/linux/linux-headers-5.9.0-0.bpo.2-common_5.9.6-1~bpo10%2B1_all.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20201126T153155Z/pool/main/l/linux/linux-headers-5.9.0-0.bpo.2-amd64_5.9.6-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20201126T153155Z/pool/main/l/linux/linux-headers-5.9.0-0.bpo.2-cloud-amd64_5.9.6-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20210108T025743Z/pool/main/l/linux/linux-kbuild-5.9_5.9.15-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20210108T025743Z/pool/main/l/linux/linux-headers-5.9.0-0.bpo.5-common_5.9.15-1~bpo10%2B1_all.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20210108T025743Z/pool/main/l/linux/linux-headers-5.9.0-0.bpo.5-amd64_5.9.15-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20210108T025743Z/pool/main/l/linux/linux-headers-5.9.0-0.bpo.5-cloud-amd64_5.9.15-1~bpo10%2B1_amd64.deb -P /KH-5-bpo && \
    cd /KH-5-bpo && \
    dpkg -i linux-kbuild-5.8_5.8.10-1~bpo10+1_amd64.deb linux-headers-5.8.0-0.bpo.2-common_5.8.10-1~bpo10+1_all.deb linux-headers-5.8.0-0.bpo.2-amd64_5.8.10-1~bpo10+1_amd64.deb linux-headers-5.8.0-0.bpo.2-cloud-amd64_5.8.10-1~bpo10+1_amd64.deb linux-kbuild-5.9_5.9.6-1~bpo10+1_amd64.deb linux-headers-5.9.0-0.bpo.2-common_5.9.6-1~bpo10+1_all.deb linux-headers-5.9.0-0.bpo.2-amd64_5.9.6-1~bpo10+1_amd64.deb linux-headers-5.9.0-0.bpo.2-cloud-amd64_5.9.6-1~bpo10+1_amd64.deb linux-kbuild-5.9_5.9.15-1~bpo10+1_amd64.deb linux-headers-5.9.0-0.bpo.5-common_5.9.15-1~bpo10+1_all.deb linux-headers-5.9.0-0.bpo.5-amd64_5.9.15-1~bpo10+1_amd64.deb linux-headers-5.9.0-0.bpo.5-cloud-amd64_5.9.15-1~bpo10+1_amd64.deb || \
    rm -rf /KH-5-bpo

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
