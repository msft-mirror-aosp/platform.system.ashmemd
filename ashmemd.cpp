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
#include <android-base/unique_fd.h>
#include <binder/BinderService.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/Status.h>
#include <cutils/native_handle.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/Status.h>
#include <utils/String16.h>

#include <android/ashmemd/BnAshmemDeviceService.h>
#include <android/system/ashmem/1.0/IAshmem.h>

using android::sp;
using android::String16;
using android::base::unique_fd;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::Return;
using android::hardware::Void;
using android::system::ashmem::V1_0::IAshmem;

namespace android {
namespace ashmemd {

inline unique_fd openDevAshmem() {
    int fd = TEMP_FAILURE_RETRY(::open("/dev/ashmem", O_RDWR | O_CLOEXEC));
    return unique_fd(fd);
}

class AshmemAidlService : public BnAshmemDeviceService {
public:
  binder::Status open(os::ParcelFileDescriptor *ashmemFd) override {
      ashmemFd->reset(openDevAshmem());
      return binder::Status::ok();
  }
};

class AshmemHidlService : public IAshmem {
public:
  Return<void> open(open_cb _hidl_cb) override {
      unique_fd fd = openDevAshmem();
      if (fd == -1) {
          _hidl_cb(nullptr);
      } else {
          native_handle_t *native_handle =
              native_handle_create(1 /* num_fds */, 0 /* num_ints */);
          native_handle->data[0] = fd.get();
          _hidl_cb(native_handle);
          // unique_fd, fd, will close when it falls out of scope, no need to
          // call native_handle_close.
          native_handle_delete(native_handle);
      }
      return Void();
  }
};

}  // namespace ashmemd
}  // namespace android

using android::ashmemd::AshmemAidlService;
using android::ashmemd::AshmemHidlService;

int main() {
    configureRpcThreadpool(1, true /* callerWillJoin */);

    sp<AshmemAidlService> ashmemAidlService = new AshmemAidlService();
    android::defaultServiceManager()->addService(
        String16("ashmem_device_service"), ashmemAidlService,
        true /* allowIsolated */);

    sp<AshmemHidlService> ashmemHidlService = new AshmemHidlService();
    auto status = ashmemHidlService->registerAsService();
    if (status != android::OK) {
        LOG(FATAL) << "Unable to register ashmem hidl service: " << status;
    }

    // Create non-HW binder threadpool for Aidl Service
    sp<android::ProcessState> ps{android::ProcessState::self()};
    ps->startThreadPool();

    joinRpcThreadpool();
    std::abort(); // unreachable
}
