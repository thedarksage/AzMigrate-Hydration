NAME_IN_SPEC=$(shell grep Name:   ../build/scripts/FX/templates/inmage.spec | awk '{print $$2}')
VER_IN_SPEC=$(shell grep Version: ../build/scripts/FX/templates/inmage.spec | awk '{print $$2}')
REL_IN_SPEC=$(shell grep Release: ../build/scripts/FX/templates/inmage.spec | awk '{print $$2}')
RPM_ARCH_LEVEL=$(shell rpm -q --queryformat "%{ARCH}" rpm)
BUILT_RPM_PACKAGE="${NAME_IN_SPEC}-${VER_IN_SPEC}-${REL_IN_SPEC}"
BUILT_RPM_FILE="${BUILT_RPM_PACKAGE}.${RPM_ARCH_LEVEL}.rpm"
