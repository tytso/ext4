#
# Makefile for the Linux kernel ubuntu supplied third-party device drivers.
#

obj-$(CONFIG_BLK_DEV_DRBD)	+= drbd/
obj-$(CONFIG_SCSI_ISCSITARGET)	+= iscsitarget/
obj-$(CONFIG_SQUASHFS)		+= squashfs/
obj-$(CONFIG_AUFS)		+= aufs/
obj-$(CONFIG_ATL2)		+= atl2/
obj-$(CONFIG_NET_ET131X)	+= et131x/
obj-$(CONFIG_DM_RAID45)		+= dm-raid4-5/
obj-$(CONFIG_DM_LOOP)		+= dm-loop/
obj-$(CONFIG_NDISWRAPPER)	+= ndiswrapper/
obj-$(CONFIG_TLSF)		+= compcache/
obj-$(CONFIG_HECI)		+= heci/
obj-$(CONFIG_QC_USB)		+= qc-usb/
obj-$(CONFIG_FS_UNIONFS)	+= unionfs/
obj-$(CONFIG_LIRC_DEV)          += lirc/
obj-$(CONFIG_GFS_FS)		+= gfs/
obj-$(CONFIG_TLSUP)		+= tlsup/
obj-$(CONFIG_E1000E_NEXT)	+= e1000e-next/
obj-$(CONFIG_RT2860)		+= ../drivers/staging/rt2860/
obj-$(CONFIG_RT2870)		+= ../drivers/staging/rt2870/
obj-$(CONFIG_RTL8187SE)		+= ../drivers/staging/rtl8187se/
obj-$(CONFIG_USB_ATMEL)		+= ../drivers/staging/at76_usb/
obj-$(CONFIG_PRISM2_USB)	+= ../drivers/staging/wlan-ng/

obj-m				+= misc/ rfkill/

# This is a stupid trick to get kbuild to create ubuntu/built-in.o
obj- += foo.o
