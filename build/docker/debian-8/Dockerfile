# Docker file to generate Debian-8 container image for generating agent build.
FROM inmageazuresiterecovery.azurecr.io/debian8:v1

# Copy repo list files and scripts.
ADD sources.list snapshot.list dkh.sh /

# Install non-driver related packages.
RUN mv -f /sources.list /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --force-yes --no-install-recommends \
	ca-certificates \
    build-essential \
    net-tools \
    ssh \
    sudo \
    dc \
    vim \
    wget \
	pkg-config \
    m4 \
    autoconf \
    dialog \
    gettext \
    zlib1g-dev \
    libncurses5-dev \
    uuid-dev \
    make \
    gcc \
    g++ \
	openssl \
	linux-kbuild-4.9 \
    locate \
    libgnutls-openssl27 \
    libssl-dev \
    curl \
    libcurl3 \
    libcurl4-openssl-dev \
    libexpat1-dev \
    less \
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
	
# Install libtool 2.4.6.
RUN wget https://ftpmirror.gnu.org/libtool/libtool-2.4.6.tar.gz --no-check-certificate -P /Libtool && \
	cd /Libtool && \
	tar -xvzf libtool-2.4.6.tar.gz && \
	cd libtool-2.4.6 && \
	./configure --prefix=/usr && \
	make && \
	make install && \
	rm -rf /Libtool
	
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

# Download kernel headers and clean everything.
RUN mv -f /snapshot.list /etc/apt/sources.list.d/snapshot.list && \
    mkdir -p /DKH && \
    mv -f /dkh.sh /DKH && \
    cd /DKH && chmod +x dkh.sh && ./dkh.sh > /var/log/dkh.log 2>&1 || \
    rm -rf /DKH && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/*

# Installation of 4.9.0-0.bpo.6-amd64 kernel headers needs specical handling.
# Installation through snapshot repo is upgrading OS from 8.0 to 8.13.
RUN wget --no-check-certificate https://snapshot.debian.org/archive/debian/20180420T165136Z/pool/main/l/linux/linux-headers-4.9.0-0.bpo.6-amd64_4.9.82-1%2Bdeb9u3~bpo8%2B1_amd64.deb -P /KH-4.9.0-0.bpo.6 && \
    wget --no-check-certificate https://snapshot.debian.org/archive/debian/20180326T163159Z/pool/main/l/linux/linux-headers-4.9.0-0.bpo.6-common_4.9.82-1%2Bdeb9u3~bpo8%2B1_all.deb -P /KH-4.9.0-0.bpo.6 && \
	wget --no-check-certificate https://snapshot.debian.org/archive/debian/20180515T212227Z/pool/main/l/linux/linux-compiler-gcc-4.9-x86_4.9.88-1%2Bdeb9u1~bpo8%2B1_amd64.deb -P /KH-4.9.0-0.bpo.6 && \
    cd /KH-4.9.0-0.bpo.6 && \
    dpkg -i linux-compiler-gcc-4.9-x86_4.9.88-1+deb9u1~bpo8+1_amd64.deb linux-headers-4.9.0-0.bpo.6-common_4.9.82-1+deb9u3~bpo8+1_all.deb linux-headers-4.9.0-0.bpo.6-amd64_4.9.82-1+deb9u3~bpo8+1_amd64.deb || \
    rm -rf /KH-4.9.0-0.bpo.6

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS. 
LABEL maintainer=inmagesourcefte@microsoft.com
