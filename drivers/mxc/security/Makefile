# Makefile for the Linux MXC Security API
ifeq ($( SCC_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
EXTRA_CFLAGS += -DMXC -DLINUX_KERNEL

obj-$(CONFIG_MXC_SECURITY_SCC2) += scc2_driver.o
obj-$(CONFIG_MXC_SAHARA) += sahara2/

