# included by all X_MOUDLE Makefiles
X_MAKEFILES := $(filter-out %.sh,$(filter-out %.d,$(filter-out %.mak,$(MAKEFILE_LIST))))

X_MODULE_DIR := $(patsubst %/Makefile,%, \
	$(word $(words $(X_MAKEFILES)), $(X_MAKEFILES)))

X_MODULE := $(subst /,_,$(X_MODULE_DIR))

$(X_MODULE)_OUTPUT := $(X_ARCH)/$(X_MODULE_DIR)/$(X_CONFIGURATION)
