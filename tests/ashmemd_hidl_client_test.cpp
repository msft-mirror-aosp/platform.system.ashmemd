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

#include <dlfcn.h>
#include <gtest/gtest.h>

namespace android {
namespace ashmemd {

TEST(AshmemdHidlClientTest, OpenFd) {
    const char *clientLib = "libashmemd_hidl_client.so";

    void *handle = dlopen(clientLib, RTLD_NOW);
    ASSERT_NE(handle, nullptr) << "Failed to dlopen() " << clientLib << ": " << dlerror();

    auto function = (int (*)())dlsym(handle, "openAshmemdFd");
    ASSERT_NE(function, nullptr) << "Failed to dlsym() openAshmemdFd() function: " << dlerror();

    int fd = function();
    ASSERT_GE(fd, 0) << "Failed to open /dev/ashmem";
}

}  // namespace ashmemd
}  // namespace android
