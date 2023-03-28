ifeq (, $(WORKING_DIR))
	WORKING_DIR=${shell pwd}
endif

ifeq (, ${BLD_INVOLFLT})
	BLD_INVOLFLT=bld_involflt
endif

BLD_DIR=${WORKING_DIR}/${BLD_INVOLFLT}/

.PHONY: all clean

all:
	@rm -rf ${BLD_INVOLFLT}  # deletes from dkms build dir
	@rm -rf ${BLD_DIR} # deletes from InVolFlt/linux dir
	@mkdir -p ${BLD_DIR}
	@cp ${WORKING_DIR}/*.[ch] ${BLD_DIR}/
	@cp ${WORKING_DIR}/../../../../thirdparty/md5_drv/common/*.[ch] ${BLD_DIR}/
	@rm ${BLD_DIR}/involflt.h
	@cp ${WORKING_DIR}/../common/*.[ch] ${WORKING_DIR}/../../lib/linux/*.[ch] ${WORKING_DIR}/../../lib/common/*.[ch] Makefile ${BLD_DIR}/
	@ln -s ${BLD_DIR} ${BLD_INVOLFLT}
	@cd ${BLD_DIR}; sed -i "/SCST_DIR=/ s/\.\./..\/../" Makefile; sed -i "s|\.\./common/version.h|../../common/version.h|" ioctl.c; $(MAKE) debug=$(debug) err=$(err) fabric=$(fabric) KDIR=$(KDIR) WORKING_DIR=${WORKING_DIR} BLD_DIR=${BLD_DIR} # pass WORKING_DIR and BLD_DIR to Makefile

clean:
	@rm -rf ${BLD_INVOLFLT} # deletes from dkms build dir
	@rm -rf ${BLD_DIR} # deletes from InVolFlt/linux dir
