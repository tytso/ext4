#
# Makefile for the linux kernel.
#

# Common support
obj-y := cpu_common.o gpio.o clock.o wdog.o snoop.o io.o time.o

obj-$(CONFIG_ARCH_MX2) += iomux-mx1-mx2.o dma-mx1-mx2.o

ifeq ($(CONFIG_MXC_TZIC),y)
obj-y += tzic.o
else
obj-y += irq.o
endif

obj-$(CONFIG_ARCH_MX51) += usb_common.o utmixc.o dvfs_core.o spba.o sdma/

# CPU FREQ support
obj-$(CONFIG_CPU_FREQ_IMX)    += cpufreq.o

# USB support
obj-$(CONFIG_ISP1504_MXC) += isp1504xc.o

# obj-$(CONFIG_USB_EHCI_FSL_UTMI) += utmixc.o
ifneq ($(strip $(CONFIG_USB_EHCI_FSL_UTMI) $(CONFIG_USB_GADGET_FSL_UTMI)),)
obj-y += utmixc.o
endif

ifneq ($(CONFIG_USB_EHCI_ARC_H1)$(CONFIG_USB_EHCI_ARC_H2),)
ifeq ($(CONFIG_ARCH_MX51),y)
obj-y += isp1504xc.o
endif
endif

ifeq ($(CONFIG_USB),y)
obj-y += usb_common.o
endif

