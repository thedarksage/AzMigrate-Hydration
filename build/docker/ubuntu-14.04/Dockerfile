# Docker file to generate Ubuntu-14.04 container image for generating agent build.

FROM inmageazuresiterecovery.azurecr.io/ubuntu14.04:v1

# Install all necessary packages.
RUN apt update && apt install -y \
	net-tools \
        ssh \
        dc \
        vim \
        wget \
		pkg-config \
        m4 \
        autoconf \
		automake- \
        gettext \
        zlib1g-dev \
        libncurses-dev \
        uuid-dev \
	xz-utils \
	rpm \
        make \
        gcc \
        g++ \
        tzdata \
        dos2unix

# Install Automake 1.16.3.
RUN wget http://ftp.gnu.org/gnu/automake/automake-1.16.3.tar.gz -P /Automake && \
	cd /Automake && \
	tar -xvzf automake-1.16.3.tar.gz && \
	cd automake-1.16.3 && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	rm -rf /Automake
	
# Install libtool 2.4.6.
RUN wget https://ftpmirror.gnu.org/libtool/libtool-2.4.6.tar.gz --no-check-certificate -P /Libtool && \
	cd /Libtool && \
	tar -xvzf libtool-2.4.6.tar.gz && \
	cd libtool-2.4.6 && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	rm -rf /Libtool
	
# Clean everything.
RUN apt-get clean autoclean

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
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /usr/share/doc/*

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
