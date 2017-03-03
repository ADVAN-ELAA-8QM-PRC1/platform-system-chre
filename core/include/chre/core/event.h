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

#ifndef CHRE_CORE_EVENT_H_
#define CHRE_CORE_EVENT_H_

#include "chre_api/chre/event.h"
#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/util/non_copyable.h"

#include <cstdint>

namespace chre {

// TODO: put these in a more common place
constexpr uint32_t kSystemInstanceId = 0;
constexpr uint32_t kBroadcastInstanceId = UINT32_MAX;

// Can be used in the context of a specific Nanoapp's instance ID
constexpr uint32_t kInvalidInstanceId = kBroadcastInstanceId;

class Event : public NonCopyable {
 public:
  Event(uint16_t eventType,
        void *eventData,
        chreEventCompleteFunction *freeCallback,
        uint32_t senderInstanceId = kSystemInstanceId,
        uint32_t targetInstanceId = kBroadcastInstanceId)
      : eventType(eventType),
        eventData(eventData),
        freeCallback(freeCallback),
        senderInstanceId(senderInstanceId),
        targetInstanceId(targetInstanceId) {}

  void incrementRefCount() {
    mRefCount++;
    ASSERT(mRefCount != 0);
  }

  void decrementRefCount() {
    ASSERT(mRefCount > 0);
    mRefCount--;
  }

  bool isUnreferenced() {
    return (mRefCount == 0);
  }

  const uint16_t eventType;
  void * const eventData;
  const chreEventCompleteFunction *freeCallback;
  const uint32_t senderInstanceId;
  const uint32_t targetInstanceId;

 private:
  size_t mRefCount = 0;
};

}

#endif  // CHRE_CORE_EVENT_H_
