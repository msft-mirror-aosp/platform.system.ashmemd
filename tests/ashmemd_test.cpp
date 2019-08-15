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

#include <android-base/unique_fd.h>
#include <android/ashmemd/IAshmemDeviceService.h>
#include <android/hidl/manager/1.2/IServiceManager.h>
#include <android/system/ashmem/1.0/IAshmem.h>
#include <binder/IServiceManager.h>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <hidl/ServiceManagement.h>
#include <linux/ashmem.h>
#include <sys/mman.h>

using android::IBinder;
using android::IServiceManager;
using android::String16;
using android::ashmemd::IAshmemDeviceService;
using android::base::unique_fd;
using android::hardware::hidl_handle;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;
using android::os::ParcelFileDescriptor;
using android::system::ashmem::V1_0::IAshmem;

namespace android {
namespace ashmemd {

enum class Interface { AIDL, HIDL };

class AshmemdTest : public ::testing::TestWithParam<Interface> {
  public:
    void SetUp() override {
        sp<IServiceManager> sm = android::defaultServiceManager();
        sp<IBinder> binder = sm->getService(String16("ashmem_device_service"));
        ASSERT_NE(binder, nullptr);

        ashmemAidlService = android::interface_cast<IAshmemDeviceService>(binder);
        ASSERT_NE(ashmemAidlService, nullptr);

        ashmemHidlService = IAshmem::getService();
        ASSERT_NE(ashmemHidlService, nullptr) << "failed to get ashmem hidl service";
    }

    unique_fd openFd(Interface interface) {
        switch (interface) {
            case Interface::AIDL: {
                ParcelFileDescriptor fd;
                ashmemAidlService->open(&fd);
                return unique_fd(dup(fd.get()));
            }
            case Interface::HIDL: {
                unique_fd fd;
                ashmemHidlService->open([&fd](hidl_handle handle) {
                    ASSERT_NE(handle, nullptr)
                        << "failed to open /dev/ashmem from IAshmem.";
                    fd = unique_fd(dup(handle->data[0]));
                });
                return fd;
            }
            default:
                return unique_fd(-1);
        }
    }

    sp<IAshmemDeviceService> ashmemAidlService;
    sp<IAshmem> ashmemHidlService;
};

TEST_P(AshmemdTest, OpenFd) {
    unique_fd fd = openFd(GetParam());
    ASSERT_GE(fd.get(), 0);
}

TEST_P(AshmemdTest, OpenMultipleFds) {
    unique_fd fd1 = openFd(GetParam());
    unique_fd fd2 = openFd(GetParam());
    ASSERT_NE(fd1.get(), fd2.get());
}

TEST_P(AshmemdTest, MmapFd) {
    unique_fd fd = openFd(GetParam());
    size_t testSize = 2097152;

    ASSERT_EQ(ioctl(fd.get(), ASHMEM_SET_NAME, "AshmemdTest"), 0);
    ASSERT_EQ(ioctl(fd.get(), ASHMEM_SET_SIZE, testSize), 0);

    void* data = mmap(NULL, testSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
    ASSERT_NE(data, MAP_FAILED) << "Failed to mmap() ashmem fd";
    ASSERT_EQ(munmap(data, testSize), 0) << "Failed to munmap() ashmem fd";
}

INSTANTIATE_TEST_SUITE_P(AshmemdTestSuite, AshmemdTest,
                         ::testing::Values(Interface::AIDL, Interface::HIDL));

TEST(IAshmemTest, OnlyOneInstance) {
    using ::android::hidl::manager::V1_2::IServiceManager;
    sp<IServiceManager> manager = android::hardware::defaultServiceManager1_2();
    ASSERT_NE(manager, nullptr) << "Unable to open defaultServiceManager";

    manager->listManifestByInterface(
        IAshmem::descriptor, [](const hidl_vec<hidl_string>& names) {
            int instances = 0;
            for (const auto& name : names) {
                ASSERT_EQ(name, std::string("default"));
                instances += 1;
            }
            ASSERT_EQ(instances, 1);
        });
}

TEST(LibAshmemdClientTest, OpenFd) {
    void* handle = dlopen("libashmemd_client.so", RTLD_NOW);
    ASSERT_NE(handle, nullptr) << "Failed to dlopen() libashmemd_client.so: " << dlerror();

    auto function = (int (*)())dlsym(handle, "openAshmemdFd");
    ASSERT_NE(function, nullptr) << "Failed to dlsym() openAshmemdFd() function: " << dlerror();

    int fd = function();
    ASSERT_GE(fd, 0) << "Failed to open /dev/ashmem";
}

}  // namespace ashmemd
}  // namespace android
