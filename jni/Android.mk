# Copyright (C) 2009 Motisan Radu
#
# radu.motisan@gmail.com
#
# All rights reserved.
# 
LOCAL_PATH:= $(call my-dir)

$(info PREPARE TO BUILD)

# aux, which will be built statically
include $(CLEAR_VARS)
LOCAL_MODULE    := hci
LOCAL_SRC_FILES := hci.c
include $(BUILD_STATIC_LIBRARY)

# aux, which will be built statically
include $(CLEAR_VARS)
LOCAL_MODULE    := btutil
LOCAL_SRC_FILES := btutil.c
include $(BUILD_STATIC_LIBRARY)

# second lib, which will depend on and include the first one
include $(CLEAR_VARS)
LOCAL_MODULE    := BTL
LOCAL_SRC_FILES := BTL.c
LOCAL_STATIC_LIBRARIES := hci btutil
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog
include $(BUILD_SHARED_LIBRARY)  
