# Docker file to generate Oel-9 container image for generating agent build.

FROM inmageazuresiterecovery.azurecr.io/oracle9:v1

RUN yum-config-manager --enable ol9_UEKR7

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
	xz-devel \
	perl \
    tar \
    tzdata \
    ncurses-devel \
    libuuid-devel \
	libzstd-devel-1.5.1-2.el9.x86_64 \
    rpm-build \
    elfutils-libelf-devel \
    kernel-devel-5.14.0-70.13.1.0.3.el9_0.x86_64 \
	kernel-devel-5.14.0-162.6.1.el9_1.x86_64 \
    kernel-uek-devel-5.15.0-0.30.19.el9uek.x86_64 && \
	mkdir -p /lib/modules/5.14.0-70.13.1.0.3.el9_0.x86_64 && \
    ln -s /usr/src/kernels/5.14.0-70.13.1.0.3.el9_0.x86_64/ /lib/modules/5.14.0-70.13.1.0.3.el9_0.x86_64/build && \
    mkdir -p /lib/modules/5.14.0-162.6.1.el9_1.x86_64 && \
    ln -s /usr/src/kernels/5.14.0-162.6.1.el9_1.x86_64/ /lib/modules/5.14.0-162.6.1.el9_1.x86_64/build && \	
    mkdir -p /lib/modules/5.15.0-0.30.19.el9uek.x86_64 && \
    ln -s /usr/src/kernels/5.15.0-0.30.19.el9uek.x86_64/ /lib/modules/5.15.0-0.30.19.el9uek.x86_64/build



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

