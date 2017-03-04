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

#ifndef CHRE_CORE_EVENT_LOOP_MANAGER_H_
#define CHRE_CORE_EVENT_LOOP_MANAGER_H_

#include "chre/core/event_loop.h"
#include "chre/util/dynamic_vector.h"
#include "chre/util/non_copyable.h"
#include "chre/util/singleton.h"
#include "chre/util/unique_ptr.h"

namespace chre {

/**
 * A class that keeps track of all event loops in the system. This class
 * represents the top-level object in CHRE. It will own all resources that are
 * shared by all event loops.
 */
class EventLoopManager : public NonCopyable {
 public:
  /**
   * Constructs an event loop and returns a pointer to it. The event loop is not
   * started by this method.
   *
   * @return A pointer to an event loop. If creation fails, nullptr is returned.
   */
  EventLoop *createEventLoop();

 private:
  //! The list of event loops managed by this event loop manager. The EventLoops
  //! are stored in UniquePtr because they are large objects. They do not
  //! provide an implementation of the move constructor so it is best left to
  //! allocate each event loop and manage the pointers to those event loops.
  DynamicVector<UniquePtr<EventLoop>> mEventLoops;
};

//! Provide an alias to the EventLoopManager singleton.
typedef Singleton<EventLoopManager> EventLoopManagerSingleton;

}  // namespace chre

#endif  // CHRE_CORE_EVENT_LOOP_MANAGER_H_
