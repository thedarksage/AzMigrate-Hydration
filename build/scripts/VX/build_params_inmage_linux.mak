VX_NAME_IN_SPEC=$(shell grep Name:   ../build/scripts/VX/templates/vxinmage.spec | awk '{print $$2}')
VX_VER_IN_SPEC=$(shell grep PROD_VERSION common/version.h | awk '{print $$3}' | tr -d "\"")
VX_REL_IN_SPEC=$(shell grep Release: ../build/scripts/VX/templates/vxinmage.spec | awk '{print $$2}')
RPM_ARCH_LEVEL=$(shell rpm -q --queryformat "%{ARCH}" rpm)
VX_BUILT_RPM_PACKAGE="${VX_NAME_IN_SPEC}-${VX_VER_IN_SPEC}-${VX_REL_IN_SPEC}"
VX_BUILT_RPM_FILE="${VX_BUILT_RPM_PACKAGE}.${RPM_ARCH_LEVEL}.rpm"
