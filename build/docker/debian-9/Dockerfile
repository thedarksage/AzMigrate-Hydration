# Docker file to generate Debian-9 container image for generating agent build.
FROM inmageazuresiterecovery.azurecr.io/debian9:v4

# Copy repo list files and scripts.
ADD sources.list snapshot.list dkh.sh /

# Install non-driver related packages.
RUN mv -f /sources.list /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
	ca-certificates \
    net-tools \
    ssh \
    sudo \
    dc \
    vim \
    wget \
    m4 \
	pkg-config \
    autoconf \
    gettext \
    zlib1g-dev \
    libncurses-dev \
    uuid-dev \
    make \
    gcc \
    g++ \
	openssl \
	linux-kbuild-4.9 \
    linux-kbuild-4.19 \
    tzdata \
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
	
RUN apt-get update && apt-get install -y \
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
RUN	mkdir -p /DKH && \
    mv -f /dkh.sh /DKH && \
    cd /DKH && chmod +x dkh.sh && ./dkh.sh > /var/log/dkh.log 2>&1 || \
    rm -rf /DKH && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/*
	
# Installation of 4.9.0-10-amd64 kernel headers needs specical handling.
# Adding snapshot repo doesn't help in downloading the kernel header
RUN wget --no-check-certificate https://snapshot.debian.org/archive/debian/20190726T035715Z/pool/main/l/linux/linux-headers-4.9.0-10-amd64_4.9.185-1_amd64.deb -P /KH-4.9.0-10 && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20190726T035715Z/pool/main/l/linux/linux-headers-4.9.0-10-common_4.9.185-1_all.deb -P /KH-4.9.0-10 && \
	wget --no-check-certificate https://snapshot.debian.org/archive/debian/20180515T212227Z/pool/main/l/linux/linux-compiler-gcc-4.9-x86_4.9.88-1%2Bdeb9u1~bpo8%2B1_amd64.deb -P /KH-4.9.0-10 && \
    cd /KH-4.9.0-10 && \
    dpkg -i linux-headers-4.9.0-10-common_4.9.185-1_all.deb linux-headers-4.9.0-10-amd64_4.9.185-1_amd64.deb || \
    rm -rf /KH-4.9.0-10

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS. 
LABEL maintainer=inmagesourcefte@microsoft.com
