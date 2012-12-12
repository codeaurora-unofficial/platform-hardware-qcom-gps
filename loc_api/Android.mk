ifneq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE),)

LOCAL_PATH := $(call my-dir)

GPS_DIR_LIST :=

# add RPC dirs if RPC is available
ifneq ($(TARGET_NO_RPC),true)

GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api-rpc-50001/
GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api-rpc/
GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api/

endif #TARGET_NO_RPC

ifeq ($(BOARD_USES_QCOM_HARDWARE), true)
#add QMI libraries for QMI targets
QMI_BOARD_PLATFORM_LIST := msm8960
QMI_BOARD_PLATFORM_LIST += msm8974
endif

ifeq ($(call is-board-platform-in-list,$(QMI_BOARD_PLATFORM_LIST)),true)
GPS_DIR_LIST += $(LOCAL_PATH)/loc_api_v02/
endif #is-board-platform-in-list

GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api_50001/

#call the subfolders
include $(addsuffix Android.mk, $(GPS_DIR_LIST))

endif#BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE
