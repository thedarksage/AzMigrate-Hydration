# Docker file to generate Oel-8 container image for generating agent build.

FROM inmageazuresiterecovery.azurecr.io/oraclelinux8:v1

# Copy repo list files and scripts.
ADD uek.ol8.repo /etc/yum.repos.d

# Install all necessary packages.
RUN yum install -y --skip-broken \
    net-tools \
    sudo \
    vim \
    wget \
	pkg-config \
    m4 \
    autoconf \
	libtool \
    gettext \
    make \
    gcc \
    gcc-c++ \
    dos2unix \
    bc \
    tar \
    tzdata \
    ncurses-devel \
    libuuid-devel \
    rpm-build \
    elfutils-libelf-devel \
    kernel-devel-4.18.0-80.el8.x86_64 \
	kernel-devel-4.18.0-147.el8.x86_64 \
	kernel-devel-4.18.0-305.el8.x86_64 \
    kernel-uek-devel-5.4.17-2011.1.2.el8uek.x86_64 && \
	mkdir -p /lib/modules/4.18.0-80.el8.x86_64 && \
    ln -s /usr/src/kernels/4.18.0-80.el8.x86_64/ /lib/modules/4.18.0-80.el8.x86_64/build && \
    mkdir -p /lib/modules/4.18.0-147.el8.x86_64 && \
    ln -s /usr/src/kernels/4.18.0-147.el8.x86_64/ /lib/modules/4.18.0-147.el8.x86_64/build && \	
	mkdir -p /lib/modules/4.18.0-305.el8.x86_64 && \
    ln -s /usr/src/kernels/4.18.0-305.el8.x86_64/ /lib/modules/4.18.0-305.el8.x86_64/build && \
    mkdir -p /lib/modules/5.4.17-2011.1.2.el8uek.x86_64 && \
    ln -s /usr/src/kernels/5.4.17-2011.1.2.el8uek.x86_64/ /lib/modules/5.4.17-2011.1.2.el8uek.x86_64/build
	
RUN yum install -y --skip-broken \
	kernel-uek-devel-5.15.0-0.30.19.el8uek.x86_64 && \
	mkdir -p /lib/modules/5.15.0-0.30.19.el8uek.x86_64 && \
    ln -s /usr/src/kernels/5.15.0-0.30.19.el8uek.x86_64/ /lib/modules/5.15.0-0.30.19.el8uek.x86_64/build
	
# Download and Install the kernel-devel package for kernel 4.18.0-348.el8.x86_64, 4.18.0-425.3.1.el8.x86_64 and 4.18.0-425.10.1.el8_7.x86_64
RUN wget https://yum.oracle.com/repo/OracleLinux/OL8/baseos/latest/x86_64/getPackage/kernel-devel-4.18.0-348.el8.x86_64.rpm \
         https://yum.oracle.com/repo/OracleLinux/OL8/baseos/latest/x86_64/getPackage/kernel-devel-4.18.0-425.3.1.el8.x86_64.rpm \
		 https://yum.oracle.com/repo/OracleLinux/OL8/baseos/latest/x86_64/getPackage/kernel-devel-4.18.0-425.10.1.el8_7.x86_64.rpm && \
    rpm -ivh kernel-devel-4.18.0-348.el8.x86_64.rpm kernel-devel-4.18.0-425.3.1.el8.x86_64.rpm kernel-devel-4.18.0-425.10.1.el8_7.x86_64.rpm


RUN mkdir -p /lib/modules/4.18.0-348.el8.x86_64 /lib/modules/4.18.0-425.3.1.el8.x86_64 /lib/modules/4.18.0-425.10.1.el8_7.x86_64 && \
	ln -s /usr/src/kernels/4.18.0-348.el8.x86_64 /lib/modules/4.18.0-348.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-425.3.1.el8.x86_64 /lib/modules/4.18.0-425.3.1.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-425.10.1.el8_7.x86_64 /lib/modules/4.18.0-425.10.1.el8_7.x86_64/build

# Make changes in /usr/lib/rpm/macros file
RUN sed -i 's/^%_buildrootdir.*/%_buildrootdir\t\t%{_topdir}\//g' /usr/lib/rpm/macros
RUN sed -i 's/^%buildroot.*/%buildroot\t\t%{_buildrootdir}\/%{name}-buildroot/g' /usr/lib/rpm/macros
	
# Clean everything.
RUN yum clean all

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
    cd / && \
    rm -rf /GitBuild

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS. 
LABEL maintainer=inmagesourcefte@microsoft.com
