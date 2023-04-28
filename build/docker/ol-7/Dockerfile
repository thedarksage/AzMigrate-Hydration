# Docker file to generate OEL 7 container image for generating agent build.
FROM inmageazuresiterecovery.azurecr.io/oraclelinux74:v1

# Copy scripts.
ADD dkh.sh /

# Install all necessary packages.
RUN yum install -y \
    vim \
    wget \
    net-tools \
    zlib-devel \
    gettext \
    m4 \
    autoconf \
    make \
    gcc \
    gcc-c++ \
    dos2unix \
    ncurses-devel \
	libuuid-devel \
	xz-devel \
    bc \
    tzdata \
    rpm-build; \
    yum clean all

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
	
#Download, build and install git
RUN wget https://github.com/git/git/archive/v2.21.0.tar.gz -P /GitBuild && \
    cd /GitBuild && \
    tar -xvzf v2.21.0.tar.gz && \
    cd git-2.21.0 && \
    make configure && \
    ./configure --prefix=/usr && \
    make all && \
    make install && \
    /usr/bin/git --version && \
    cd / && \
    rm -rf /GitBuild

# Download kernel headers
RUN mkdir -p /DKH && \
    mv -f /dkh.sh /DKH && \
    cd /DKH && chmod +x dkh.sh && ./dkh.sh > /var/log/dkh.log 2>&1 || \
    rm -rf /DKH

# Make changes in /usr/lib/rpm/macros file
RUN sed -i -e "s|^%_buildrootdir.*|%_buildrootdir\t\t%{_topdir}/|g; s|^%buildroot.*|%buildroot\t\t%{_buildrootdir}\/%{name}-buildroot|g" /usr/lib/rpm/macros

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
