/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CameraModule"
//#define LOG_NDEBUG 0

#include "CameraModule.h"

namespace android {

void CameraModule::deriveCameraCharacteristicsKeys(
        uint32_t deviceVersion, CameraMetadata &chars) {
    // HAL1 devices should not reach here
    if (deviceVersion < CAMERA_DEVICE_API_VERSION_2_0) {
        ALOGV("%s: Cannot derive keys for HAL version < 2.0");
        return;
    }

    // Keys added in HAL3.3
    if (deviceVersion < CAMERA_DEVICE_API_VERSION_3_3) {
        Vector<uint8_t> controlModes;
        uint8_t data = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
        chars.update(ANDROID_CONTROL_AE_LOCK_AVAILABLE, &data, /*count*/1);
        data = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
        chars.update(ANDROID_CONTROL_AWB_LOCK_AVAILABLE, &data, /*count*/1);
        controlModes.push(ANDROID_CONTROL_MODE_OFF);
        controlModes.push(ANDROID_CONTROL_MODE_AUTO);
        camera_metadata_entry entry = chars.find(ANDROID_CONTROL_AVAILABLE_SCENE_MODES);
        if (entry.count > 1 || entry.data.u8[0] != ANDROID_CONTROL_SCENE_MODE_DISABLED) {
            controlModes.push(ANDROID_CONTROL_MODE_USE_SCENE_MODE);
        }
        chars.update(ANDROID_CONTROL_AVAILABLE_MODES, controlModes);
    }
    return;
}

CameraModule::CameraModule(camera_module_t *module) {
    if (module == NULL) {
        ALOGE("%s: camera hardware module must not be null", __FUNCTION__);
        assert(0);
    }

    mModule = module;
    mCameraInfoMap.setCapacity(getNumberOfCameras());
}

int CameraModule::getCameraInfo(int cameraId, struct camera_info *info) {
    Mutex::Autolock lock(mCameraInfoLock);
    if (cameraId < 0) {
        ALOGE("%s: Invalid camera ID %d", __FUNCTION__, cameraId);
        return -EINVAL;
    }

    // Only override static_camera_characteristics for API2 devices
    int apiVersion = mModule->common.module_api_version;
    if (apiVersion < CAMERA_MODULE_API_VERSION_2_0) {
        return mModule->get_camera_info(cameraId, info);
    }

    ssize_t index = mCameraInfoMap.indexOfKey(cameraId);
    if (index == NAME_NOT_FOUND) {
        // Get camera info from raw module and cache it
        camera_info rawInfo, cameraInfo;
        int ret = mModule->get_camera_info(cameraId, &rawInfo);
        if (ret != 0) {
            return ret;
        }
        CameraMetadata m;
        m = rawInfo.static_camera_characteristics;
        deriveCameraCharacteristicsKeys(rawInfo.device_version, m);
        mCameraCharacteristicsMap.add(cameraId, m);
        cameraInfo = rawInfo;
        cameraInfo.static_camera_characteristics =
                mCameraCharacteristicsMap.valueFor(cameraId).getAndLock();
        mCameraInfoMap.add(cameraId, cameraInfo);
        index = mCameraInfoMap.indexOfKey(cameraId);
    }

    assert(index != NAME_NOT_FOUND);
    // return the cached camera info
    *info = mCameraInfoMap[index];
    return 0;
}

int CameraModule::open(const char* id, struct hw_device_t** device) {
    return filterOpenErrorCode(mModule->common.methods->open(&mModule->common, id, device));
}

int CameraModule::openLegacy(
        const char* id, uint32_t halVersion, struct hw_device_t** device) {
    return mModule->open_legacy(&mModule->common, id, halVersion, device);
}

int CameraModule::getNumberOfCameras() {
    return mModule->get_number_of_cameras();
}

int CameraModule::setCallbacks(const camera_module_callbacks_t *callbacks) {
    return mModule->set_callbacks(callbacks);
}

bool CameraModule::isVendorTagDefined() {
    return mModule->get_vendor_tag_ops != NULL;
}

void CameraModule::getVendorTagOps(vendor_tag_ops_t* ops) {
    if (mModule->get_vendor_tag_ops) {
        mModule->get_vendor_tag_ops(ops);
    }
}

int CameraModule::setTorchMode(const char* camera_id, bool enable) {
    return mModule->set_torch_mode(camera_id, enable);
}

status_t CameraModule::filterOpenErrorCode(status_t err) {
    switch(err) {
        case NO_ERROR:
        case -EBUSY:
        case -EINVAL:
        case -EUSERS:
            return err;
        default:
            break;
    }
    return -ENODEV;
}

uint16_t CameraModule::getModuleApiVersion() {
    return mModule->common.module_api_version;
}

const char* CameraModule::getModuleName() {
    return mModule->common.name;
}

uint16_t CameraModule::getHalApiVersion() {
    return mModule->common.hal_api_version;
}

const char* CameraModule::getModuleAuthor() {
    return mModule->common.author;
}

void* CameraModule::getDso() {
    return mModule->common.dso;
}

}; // namespace android
