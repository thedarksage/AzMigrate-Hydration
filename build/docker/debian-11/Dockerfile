# Docker file to generate Debian-10 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/debian:bullseye

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
    libtool \
	libtool-bin \
    zlib1g-dev \
    gettext \
    m4 \
    gcc \
    g++ \
    dc \
    tzdata \
	openssl \
	liblzma-dev \
    rpm \
    libncurses-dev \
    uuid-dev \
    dos2unix && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/*

# Install Automake 1.16.3.
RUN wget http://ftp.gnu.org/gnu/automake/automake-1.16.3.tar.gz -P /Automake && \
	cd /Automake && \
	tar -xvzf automake-1.16.3.tar.gz && \
	cd automake-1.16.3 && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	rm -rf /Automake
	
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

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
