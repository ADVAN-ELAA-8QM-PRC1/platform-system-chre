/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <cinttypes>

#include "qurt_timer.h"
#include "chre/platform/system_timer.h"

#include "chre/core/init.h"
#include "chre/platform/log.h"
#include "chre/platform/system_time.h"
#include "chre/util/time.h"

void logCallback(void *data) {
  LOGI("timer callback invoked");
}

/**
 * The main entry point to the SLPI CHRE runtime.
 *
 * This method is invoked automatically via FastRPC and must be externed for
 * C-linkage.
 */
extern "C" int chre_init() {
  chre::init();

  LOGI("SLPI CHRE initialized at time %" PRIu64,
       chre::SystemTime::getMonotonicTime().toRawNanoseconds());

  chre::SystemTimer delayedLogTimer;
  if (!delayedLogTimer.init()) {
    LOGE("Failed to initialize timer");
  } else if (
      !delayedLogTimer.set(logCallback, nullptr, chre::Milliseconds(500))) {
    LOGE("Failed to set timer");
  } else {
    LOGI("sleeping");
    qurt_timer_sleep(5000000);
    LOGI("done sleeping");
  }

  return 0;
}
