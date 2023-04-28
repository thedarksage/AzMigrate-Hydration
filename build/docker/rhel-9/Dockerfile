# Docker file to generate RHEL-9 container image for generating agent build.
FROM mcr.microsoft.com/mirror/docker/library/almalinux:9

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
        bison \
        flex \
		libzstd-devel-1.5.1-2.el9.x86_64

RUN yum provides ifconfig

RUN wget https://yum.oracle.com/repo/OracleLinux/OL9/appstream/x86_64/getPackage/kernel-devel-5.14.0-70.13.1.0.3.el9_0.x86_64.rpm \
        https://yum.oracle.com/repo/OracleLinux/OL9/appstream/x86_64/getPackage/kernel-devel-5.14.0-162.6.1.el9_1.x86_64.rpm && \
    rpm -ivh kernel-devel-5.14.0-70.13.1.0.3.el9_0.x86_64.rpm kernel-devel-5.14.0-162.6.1.el9_1.x86_64.rpm


RUN mkdir -p /lib/modules/5.14.0-70.13.1.0.3.el9_0.x86_64 && \
	ln -s /usr/src/kernels/5.14.0-70.13.1.0.3.el9_0.x86_64 /lib/modules/5.14.0-70.13.1.0.3.el9_0.x86_64/build && \
    mkdir -p /lib/modules/5.14.0-162.6.1.el9_1.x86_64 && \
	ln -s /usr/src/kernels/5.14.0-162.6.1.el9_1.x86_64 /lib/modules/5.14.0-162.6.1.el9_1.x86_64/build

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

