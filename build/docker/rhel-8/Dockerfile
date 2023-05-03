# Docker file to generate RHEL-8 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/almalinux:8.5

RUN yum install -y \
        wget \
        bc \
        net-tools \
        openssh \
        zlib-devel \
        gettext \
	m4 \
        autoconf \
        libtool \
        tar \
        tzdata \
        make \
        gcc \
        openssh-clients \
	openssh-server \
        dos2unix \
        gcc-c++ \
        ncurses-devel \
        libuuid-devel \
	curl-devel \
        rpm-build \
	zlib \
	perl \
	libvirt \
	expat-devel \
	gettext-devel \
	openssl-devel \
	elfutils-libelf-devel \
	xz-devel \
	kernel-devel-4.18.0-425.3.1.el8.x86_64 \
	kernel-devel-4.18.0-425.10.1.el8_7.x86_64
		
RUN yum provides ifconfig

# Download and Install the kernel-devel package for kernel 4.18.0-80.el8.x86_64, 4.18.0-147.el8.x86_64 and 4.18.0-348.el8.x86_64
RUN wget https://vault.centos.org/8.0.1905/BaseOS/x86_64/os/Packages/kernel-devel-4.18.0-80.el8.x86_64.rpm \
         https://vault.centos.org/8.1.1911/BaseOS/x86_64/os/Packages/kernel-devel-4.18.0-147.el8.x86_64.rpm \
		 https://yum.oracle.com/repo/OracleLinux/OL8/baseos/latest/x86_64/getPackage/kernel-devel-4.18.0-305.el8.x86_64.rpm \
		 https://vault.centos.org/8.5.2111/BaseOS/x86_64/os/Packages/kernel-devel-4.18.0-348.el8.x86_64.rpm && \
    rpm -ivh --oldpackage kernel-devel-4.18.0-80.el8.x86_64.rpm kernel-devel-4.18.0-147.el8.x86_64.rpm kernel-devel-4.18.0-305.el8.x86_64.rpm kernel-devel-4.18.0-348.el8.x86_64.rpm 


RUN mkdir -p /lib/modules/4.18.0-80.el8.x86_64 /lib/modules/4.18.0-147.el8.x86_64 /lib/modules/4.18.0-305.el8.x86_64 /lib/modules/4.18.0-348.el8.x86_64 /lib/modules/4.18.0-425.3.1.el8.x86_64 /lib/modules/4.18.0-425.10.1.el8_7.x86_64 && \
	ln -s /usr/src/kernels/4.18.0-80.el8.x86_64 /lib/modules/4.18.0-80.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-147.el8.x86_64 /lib/modules/4.18.0-147.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-305.el8.x86_64 /lib/modules/4.18.0-305.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-348.el8.x86_64 /lib/modules/4.18.0-348.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-425.3.1.el8.x86_64 /lib/modules/4.18.0-425.3.1.el8.x86_64/build && \
	ln -s /usr/src/kernels/4.18.0-425.10.1.el8_7.x86_64 /lib/modules/4.18.0-425.10.1.el8_7.x86_64/build

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
        cd - && \
        rm -rf /GitBuild
		
# Addtional configuraiton changes.
RUN cp /usr/lib/rpm/macros /usr/lib/rpm/macros.orig && \
    sed -i 's/^%_buildrootdir.*/%_buildrootdir\t\t%{_topdir}\//' /usr/lib/rpm/macros && \
    sed -i 's/^%buildroot.*/%buildroot\t\t%{_buildrootdir}\/%{name}-buildroot/' /usr/lib/rpm/macros

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
