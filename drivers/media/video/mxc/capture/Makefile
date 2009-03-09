ifeq ($(CONFIG_VIDEO_MXC_IPU_CAMERA),y)
	obj-$(CONFIG_VIDEO_MXC_CAMERA) += mxc_v4l2_capture.o
	obj-$(CONFIG_MXC_IPU_PRP_VF_ADC) += ipu_prp_vf_adc.o
	obj-$(CONFIG_MXC_IPU_PRP_VF_SDC) += ipu_prp_vf_sdc.o ipu_prp_vf_sdc_bg.o
	obj-$(CONFIG_MXC_IPU_PRP_ENC) += ipu_prp_enc.o ipu_still.o
endif

ov3640_camera-objs := ov3640.o sensor_clock.o
obj-$(CONFIG_MXC_CAMERA_OV3640) += ov3640_camera.o

