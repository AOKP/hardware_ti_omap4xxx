ifeq ($(TARGET_BOARD_PLATFORM),omap4)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

-include $(TOP)/vendor/widevine/proprietary/cryptoPlugin/decrypt-core.mk

LOCAL_SRC_FILES := \
        crypto.cpp

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/vendor/widevine/proprietary/cryptoPlugin \

LOCAL_STATIC_LIBRARIES := \
        liboemcrypto                    \
        libtee_client_api_driver        \

LOCAL_SHARED_LIBRARIES := \
        libstagefright_foundation       \
        liblog                          \

LOCAL_MODULE := libdrmdecrypt

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
