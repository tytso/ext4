#
# Makefile for the PMIC core drivers.
#
obj-$(CONFIG_MXC_PMIC_MC13892) += pmic_mc13892_mod.o
pmic_mc13892_mod-objs := pmic_external.o pmic_event.o pmic_common.o mc13892.o

ifneq ($(CONFIG_MXC_PMIC_SPI),)
pmic_mc13892_mod-objs += pmic_core_spi.o
endif

ifneq ($(CONFIG_MXC_PMIC_I2C),)
pmic_mc13892_mod-objs += pmic_core_i2c.o
endif

