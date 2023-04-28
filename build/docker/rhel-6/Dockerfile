# Docker file to generate RHEL-6 container image for generating agent build.
FROM inmageazuresiterecovery.azurecr.io/centos610:v2

# Point to the base repo of CentOS 6
RUN mv /etc/yum.repos.d/CentOS-Base.repo /etc/yum.repos.d/CentOS-Base.repo.orig && \
    echo -e "[base]\nname=CentOS-\$releasever - Base\nbaseurl=http://vault.centos.org/6.10/os/x86_64/\ngpgcheck=1\ngpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-6" > /etc/yum.repos.d/CentOS-Base.repo

# Install all necessary packages.
RUN yum clean all && \
    yum install -y \
    vim \
    wget \
    tar \
    tzdata \
    gcc \
    gcc-c++ \
    gettext \
	m4 \
    bc \
    dos2unix \
    rpm-build \
    openssh \
    openssh-clients \
    libacl-devel \
    ncurses-devel \
    zlib-devel \
    libuuid-devel; \
    yum clean all

# Download and Install the kernel-devel package for kernel 2.6.32-71.el6
RUN wget https://yum.oracle.com/repo/OracleLinux/OL6/0/base/x86_64/getPackage/kernel-devel-2.6.32-71.el6.x86_64.rpm -P /KDR && \
    cd /KDR && \
    rpm -ivh kernel-devel-2.6.32-71.el6.x86_64.rpm && \
    mkdir -p /lib/modules/2.6.32-71.el6.x86_64 && \
    ln -s /usr/src/kernels/2.6.32-71.el6.x86_64/ /lib/modules/2.6.32-71.el6.x86_64/build && \
    cd / && \
    rm -rf /KDR

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

# Make changes in /usr/lib/rpm/macros file
RUN sed -i -e "s|^%_buildrootdir.*|%_buildrootdir\t\t%{_topdir}/|g; s|^%buildroot.*|%buildroot\t\t%{_buildrootdir}\/%{name}-buildroot|g" /usr/lib/rpm/macros

# Set timezone to IST.
ENV TZ Asia/Kolkata

# Add any LABELS.
LABEL maintainer=inmagesourcefte@microsoft.com
