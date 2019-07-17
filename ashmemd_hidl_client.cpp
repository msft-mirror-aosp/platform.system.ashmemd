/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <android-base/logging.h>
#include <android/system/ashmem/1.0/IAshmem.h>

using android::sp;
using android::hardware::hidl_handle;
using android::system::ashmem::V1_0::IAshmem;

namespace android {
namespace ashmemd {

extern "C" int openAshmemdFd() {
    static sp<IAshmem> ashmemService = IAshmem::getService();
    if (!ashmemService) {
        LOG(ERROR) << "Failed to get IAshmem service.";
        return -1;
    }
    int fd = -1;
    ashmemService->open([&fd](hidl_handle handle) {
        if (handle == nullptr || handle->numFds < 1 || handle->data[0] < 0) {
            LOG(ERROR) << "IAshmem.open() failed";
        } else {
            fd = dup(handle->data[0]);
        }
    });
    return fd;
}

} // namespace ashmemd
} // namespace android
