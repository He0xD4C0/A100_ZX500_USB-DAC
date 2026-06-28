ifndef CONFIG_SPL_BUILD
obj-y += fsl_avb.o
obj-y += fsl_bootctl.o
obj-y += fsl_avb_sysdeps_uboot.o
obj-y += fsl_public_key.o
endif

obj-y += fsl_avbkey.o
obj-y += utils.o
obj-y += fsl_avb_ab_flow.o
obj-$(CONFIG_AVB_ATX) += fsl_atx_attributes.o

ifneq ($(ENABLE_AVB_PRODUCTION_KEY),)
ccflags-y += -DAVB_PRODUCTION_KEY
endif
