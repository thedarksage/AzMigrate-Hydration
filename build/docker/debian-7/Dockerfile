# Docker file to generate Debian-7 container image for generating agent build.
FROM inmageazuresiterecovery.azurecr.io/debian75:v1

# Copy repo list files and scripts.
ADD sources.list snapshot.list dkh.sh /

# Install non-driver related packages.
RUN mv -f /sources.list /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --force-yes --no-install-recommends \
	ca-certificates \
    vim \
    wget \
    net-tools \
    sudo \
    openssh-client \
    mlocate \
    dialog \
    perl \
    make \
    gcc \
    g++ \
    gettext \
    m4 \
	pkg-config \
    zlib1g-dev \
    dos2unix \
    tzdata \
    dc \   
	openssl \
    libncurses5-dev \
    uuid-dev && \
    apt-get clean autoclean && \
    rm -rf /var/lib/apt/lists/* && \
    ln -s /usr/bin/make /usr/bin/gmake

# Install autoconf 2.69.
RUN wget --no-check-certificate https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz -P /AutoconfBuild && \
    cd /AutoconfBuild && \
    tar -xvzf autoconf-2.69.tar.gz && \
    cd autoconf-2.69 && \
    ./configure --prefix=/usr && \
    make && \
    make install && \
    rm -rf /AutoconfBuild
	
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
RUN wget --no-check-certificate http://ftpmirror.gnu.org/libtool/libtool-2.4.6.tar.gz --no-check-certificate -P /Libtool && \
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

# Configure driver packages repo.
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
