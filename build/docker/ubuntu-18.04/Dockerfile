# Docker file to generate Ubuntu-18.04 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/ubuntu:18.04

# Set timezone to IST.
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ Asia/Kolkata

# Install all necessary packages.
RUN apt update && apt install -y \
	build-essential \
	ssh \
	sudo \
	dc \
	vim \
	wget \
	pkg-config \
	m4 \
	autoconf \
	automake- \
	gettext \
	zlib1g-dev \
	libncurses5-dev \
	tzdata \
	uuid-dev \
	cmake \
	curl
	
# Install Automake 1.16.3.
RUN wget http://ftp.gnu.org/gnu/automake/automake-1.16.3.tar.gz -P /Automake && \
	cd /Automake && \
	tar -xvzf automake-1.16.3.tar.gz && \
	cd automake-1.16.3 && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	rm -rf /Automake
	
RUN apt update && apt install -y \
    libtool \
	libtool-bin

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

# Addtional configuraiton changes.
RUN ln -s /usr/bin/make /usr/bin/gmake

# Copy dkh.sh.
ADD dkh.sh /

# Download kernel headers and clean everything.
RUN mkdir -p /DKH && \
    mv -f /dkh.sh /DKH && \
    cd /DKH && chmod +x dkh.sh && ./dkh.sh > /var/log/dkh.log 2>&1 || \
    rm -rf /DKH && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/*
	
# Install mono
RUN wget --no-check-certificate https://download.mono-project.com/sources/mono/mono-6.12.0.182.tar.xz -P /Mono && \
    cd /Mono && \
    tar -xJvf mono-6.12.0.182.tar.xz && \
    cd mono-6.12.0.182 && \
    ./configure --prefix=/usr && \
    make && \
    make install && \
    mono --version && \
    cd / && \
    rm -rf /Mono && \
	curl -L -o /tmp/cacert.pem https://curl.haxx.se/ca/cacert.pem && \
	cert-sync --user /tmp/cacert.pem

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
