# Docker file to generate SLES-12 container image for generating agent build.
FROM inmageazuresiterecovery.azurecr.io/sles12sp5:v4

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
