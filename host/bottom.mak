# The global makefile, bottom half (also see top.mak)

include $(X_ARCH).mak

# list of all the object files needed by this module (don't use lazy evalution)
$(X_MODULE)_OBJS := $(subst ..,$(DOTDOT),$(addsuffix $(X_OBJEXT),$(addprefix $($(X_MODULE)_OUTPUT)/,$(basename $(SRCS)))))

# the final binary for the module (don't use lazy evaluation)
$(X_MODULE)_BINARY := $(addprefix $($(X_MODULE)_OUTPUT)/, $(BINARY))$(BINARY_EXT)

include $(X_ARCH)-rules.mak

all:: $($(X_MODULE)_BINARY)

$(X_MODULE) $(X_MODULE_DIR): $($(X_MODULE)_BINARY)

X_ALREADY_INCLUDED=1
