# Docker file to generate RHEL-7 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/centos:7

# Install all necessary packages.
RUN yum install -y \
        wget \
        bc \
        net-tools \
        openssh \
        zlib-devel \
        gettext \
        m4 \
        autoconf \
        tar \
        tzdata \
        make \
        gcc \
        openssh-clients \
        dos2unix \
        gcc-c++ \
        ncurses-devel \
        libuuid-devel \
        rpm-build \
        xz-devel

# Download and Install the kernel-devel package for kernel 3.10.0-123.el7.x86_64 3.10.0-514.el7.x86_64 and 3.10.0-693.el7.x86_64
RUN wget https://vault.centos.org/7.0.1406/os/x86_64/Packages/kernel-devel-3.10.0-123.el7.x86_64.rpm \
		 http://vault.centos.org/7.3.1611/os/x86_64/Packages/kernel-devel-3.10.0-514.el7.x86_64.rpm \
         http://vault.centos.org/7.4.1708/os/x86_64/Packages/kernel-devel-3.10.0-693.el7.x86_64.rpm && \
    rpm -ivh kernel-devel-3.10.0-123.el7.x86_64.rpm kernel-devel-3.10.0-514.el7.x86_64.rpm kernel-devel-3.10.0-693.el7.x86_64.rpm

# Create the symlinks for building involflt module
RUN mkdir -p /lib/modules/3.10.0-123.el7.x86_64 /lib/modules/3.10.0-514.el7.x86_64 /lib/modules/3.10.0-693.el7.x86_64 && \
    ln -s /usr/src/kernels/3.10.0-123.el7.x86_64 /lib/modules/3.10.0-123.el7.x86_64/build && \
    ln -s /usr/src/kernels/3.10.0-514.el7.x86_64 /lib/modules/3.10.0-514.el7.x86_64/build && \
    ln -s /usr/src/kernels/3.10.0-693.el7.x86_64 /lib/modules/3.10.0-693.el7.x86_64/build

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
