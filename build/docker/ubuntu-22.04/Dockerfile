# Docker file to generate Ubuntu-20.04 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/ubuntu:22.04

# Set timezone to IST.
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ Asia/Kolkata

# Install all necessary packages.
RUN apt update && apt install -y \
        openssh-client \
        wget \
        iproute2 \
        net-tools \
        gettext \
        zlib1g-dev \
		pkg-config \
        m4 \
        autoconf \
        libtool \
		libtool-bin \
        build-essential \
        dos2unix \
        libncurses-dev \
        dc \
        less \
        linux-headers-generic \
        sudo \
        tzdata \
		liblzma-dev \
        uuid-dev

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
RUN wget https://github.com/git/git/archive/v2.21.0.tar.gz -P /GitBuild && \
        cd /GitBuild && \
        tar -xvzf v2.21.0.tar.gz && \
        cd git-2.21.0 && \
        make configure && \
        ./configure --prefix=/usr && \
        make all && \
        make install && \
        /usr/bin/git --version && \
        rm -rf /GitBuild

# Copy dkh.sh.
ADD dkh.sh /

# Download kernel headers and clean everything.
RUN mkdir -p /DKH && \
    mv -f /dkh.sh /DKH && \
    cd /DKH && chmod +x dkh.sh && ./dkh.sh > /var/log/dkh.log 2>&1 || \
    rm -rf /DKH && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/*

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
