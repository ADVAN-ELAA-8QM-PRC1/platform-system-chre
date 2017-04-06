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

#include "chre/core/event_loop.h"

#include "chre/core/event.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/context.h"
#include "chre/platform/log.h"
#include "chre_api/chre/version.h"

namespace chre {

EventLoop::EventLoop()
    : mTimerPool(*this) {}

bool EventLoop::findNanoappInstanceIdByAppId(uint64_t appId,
                                             uint32_t *instanceId) {
  CHRE_ASSERT(instanceId != nullptr);

  // TODO: would be nice to have a ConditionalLockGuard where we just pass this
  // bool to the constructor and it automatically handles the unlock for us
  bool needLock = (getCurrentEventLoop() != this);
  if (needLock) {
    mNanoappsLock.lock();
  }

  bool found = false;
  for (const UniquePtr<Nanoapp>& app : mNanoapps) {
    if (app->getAppId() == appId) {
      *instanceId = app->getInstanceId();
      found = true;
      break;
    }
  }

  if (needLock) {
    mNanoappsLock.unlock();
  }

  return found;
}

void EventLoop::forEachNanoapp(NanoappCallbackFunction *callback, void *data) {
  bool needLock = (getCurrentEventLoop() != this);
  if (needLock) {
    mNanoappsLock.lock();
  }

  for (const UniquePtr<Nanoapp>& nanoapp : mNanoapps) {
    callback(nanoapp.get(), data);
  }

  if (needLock) {
    mNanoappsLock.unlock();
  }
}

void EventLoop::invokeMessageFreeFunction(
    uint64_t appId, chreMessageFreeFunction *freeFunction, void *message,
    size_t messageSize) {
  Nanoapp *nanoapp = lookupAppByAppId(appId);
  if (nanoapp == nullptr) {
    LOGE("Couldn't find app 0x%016" PRIx64 " for message free callback", appId);
  } else {
    auto prevCurrentApp = mCurrentApp;
    mCurrentApp = nanoapp;
    freeFunction(message, messageSize);
    mCurrentApp = prevCurrentApp;
  }
}

void EventLoop::run() {
  LOGI("EventLoop start");

  bool havePendingEvents = false;
  while (mRunning) {
    // Events are delivered in two stages: first they arrive in the inbound
    // event queue mEvents (potentially posted from another thread), then within
    // this context these events are distributed to smaller event queues
    // associated with each Nanoapp that should receive the event. Once the
    // event is delivered to all interested Nanoapps, its free callback is
    // invoked.
    if (!havePendingEvents || !mEvents.empty()) {
      // mEvents.pop() will be a blocking call if mEvents.empty()
      distributeEvent(mEvents.pop());
    }

    havePendingEvents = deliverEvents();
  }

  // Deliver any events sitting in Nanoapps' own queues (we could drop them to
  // exit faster, but this is less code and should complete quickly under normal
  // conditions), then purge the main queue of events pending distribution. All
  // nanoapps should be prevented from sending events or messages at this point
  // via currentNanoappIsStopping() returning true.
  flushNanoappEventQueues();
  while (!mEvents.empty()) {
    freeEvent(mEvents.pop());
  }

  // Unload all running nanoapps
  while (!mNanoapps.empty()) {
    unloadNanoappAtIndex(mNanoapps.size() - 1);
  }

  LOGI("Exiting EventLoop");
}

bool EventLoop::startNanoapp(UniquePtr<Nanoapp>& nanoapp) {
  CHRE_ASSERT(!nanoapp.isNull());
  bool success = false;
  auto *eventLoopManager = EventLoopManagerSingleton::get();
  uint32_t existingInstanceId;

  if (nanoapp.isNull()) {
    // no-op, invalid argument
  } else if (eventLoopManager->findNanoappInstanceIdByAppId(nanoapp->getAppId(),
                                                            &existingInstanceId,
                                                            nullptr)) {
    LOGE("App with ID 0x%016" PRIx64 " already exists as instance ID 0x%"
         PRIx32, nanoapp->getAppId(), existingInstanceId);
  } else if (!mNanoapps.prepareForPush()) {
    LOGE("Failed to allocate space for new nanoapp");
  } else {
    nanoapp->setInstanceId(eventLoopManager->getNextInstanceId());
    LOGD("Instance ID %" PRIu32 " assigned to app ID 0x%016" PRIx64,
         nanoapp->getInstanceId(), nanoapp->getAppId());

    Nanoapp *newNanoapp = nanoapp.get();
    {
      LockGuard<Mutex> lock(mNanoappsLock);
      mNanoapps.push_back(std::move(nanoapp));
      // After this point, nanoapp is null as we've transferred ownership into
      // mNanoapps.back() - use newNanoapp to reference it
    }

    mCurrentApp = newNanoapp;
    success = newNanoapp->start();
    mCurrentApp = nullptr;
    if (!success) {
      // TODO: to be fully safe, need to purge/flush any events and messages
      // sent by the nanoapp here (but don't call nanoappEnd). For now, we just
      // destroy the Nanoapp instance.
      LOGE("Nanoapp %" PRIu32 " failed to start", newNanoapp->getInstanceId());

      // Note that this lock protects against concurrent read and modification
      // of mNanoapps, but we are assured that no new nanoapps were added since
      // we pushed the new nanoapp
      LockGuard<Mutex> lock(mNanoappsLock);
      mNanoapps.pop_back();
    }
  }

  return success;
}

bool EventLoop::unloadNanoapp(uint32_t instanceId,
                              bool allowSystemNanoappUnload) {
  bool unloaded = false;

  for (size_t i = 0; i < mNanoapps.size(); i++) {
    if (instanceId == mNanoapps[i]->getInstanceId()) {
      if (!allowSystemNanoappUnload && mNanoapps[i]->isSystemNanoapp()) {
        LOGE("Refusing to unload system nanoapp");
      } else {
        // Make sure all messages sent by this nanoapp at least have their
        // associated free callback processing pending in the event queue (i.e.
        // there are no messages pending delivery to the host)
        EventLoopManagerSingleton::get()->getHostCommsManager()
            .flushMessagesSentByNanoapp(mNanoapps[i]->getAppId());

        // Distribute all inbound events we have at this time - here we're
        // interested in handling any message free callbacks generated by
        // flushMessagesSentByNanoapp()
        flushInboundEventQueue();

        // Mark that this nanoapp is stopping early, so it can't send events or
        // messages during the nanoapp event queue flush
        mStoppingNanoapp = mNanoapps[i].get();

        // Process any pending events, with the intent of ensuring that we free
        // all events generated by this nanoapp
        flushNanoappEventQueues();

        // Finally, we are at a point where there should not be any pending
        // events or messages sent by the app that could potentially reference
        // the nanoapp's memory, so we are safe to unload it
        unloadNanoappAtIndex(i);
        mStoppingNanoapp = nullptr;

        // TODO: right now we assume that the nanoapp will clean up all of its
        // resource allocations in its nanoappEnd callback (memory, sensor
        // subscriptions, etc.), otherwise we're leaking resources. We should
        // perform resource cleanup automatically here to avoid these types of
        // potential leaks.

        LOGD("Unloaded nanoapp with instanceId %" PRIu32, instanceId);
        unloaded = true;
      }
      break;
    }
  }

  return unloaded;
}

bool EventLoop::postEvent(uint16_t eventType, void *eventData,
    chreEventCompleteFunction *freeCallback, uint32_t senderInstanceId,
    uint32_t targetInstanceId) {
  bool success = false;

  if (mRunning) {
    Event *event = mEventPool.allocate(eventType, eventData, freeCallback,
        senderInstanceId, targetInstanceId);
    if (event != nullptr) {
      success = mEvents.push(event);
    } else {
      LOGE("Failed to allocate event");
    }
  }

  return success;
}

void EventLoop::stop() {
  postEvent(0, nullptr, nullptr, kSystemInstanceId, kSystemInstanceId);
  // Stop accepting new events and tell the main loop to finish
  mRunning = false;
}

Nanoapp *EventLoop::getCurrentNanoapp() const {
  CHRE_ASSERT(getCurrentEventLoop() == this);
  return mCurrentApp;
}

size_t EventLoop::getNanoappCount() const {
  CHRE_ASSERT(getCurrentEventLoop() == this);
  return mNanoapps.size();
}

TimerPool& EventLoop::getTimerPool() {
  return mTimerPool;
}

Nanoapp *EventLoop::findNanoappByInstanceId(uint32_t instanceId) {
  bool needLock = (getCurrentEventLoop() != this);
  if (needLock) {
    mNanoappsLock.lock();
  }

  Nanoapp *nanoapp = lookupAppByInstanceId(instanceId);

  if (needLock) {
    mNanoappsLock.unlock();
  }

  return nanoapp;
}

bool EventLoop::currentNanoappIsStopping() {
  return (mCurrentApp == mStoppingNanoapp || !mRunning);
}

bool EventLoop::deliverEvents() {
  bool havePendingEvents = false;

  // Do one loop of round-robin. We might want to have some kind of priority or
  // time sharing in the future, but this should be good enough for now.
  for (const UniquePtr<Nanoapp>& app : mNanoapps) {
    if (app->hasPendingEvent()) {
      havePendingEvents |= deliverNextEvent(app);
    }
  }

  return havePendingEvents;
}

bool EventLoop::deliverNextEvent(const UniquePtr<Nanoapp>& app) {
  // TODO: cleaner way to set/clear this? RAII-style?
  mCurrentApp = app.get();
  Event *event = app->processNextEvent();
  mCurrentApp = nullptr;

  if (event->isUnreferenced()) {
    freeEvent(event);
  }

  return app->hasPendingEvent();
}

void EventLoop::distributeEvent(Event *event) {
  // TODO: this is *not* thread-safe; if we have multiple EventLoops, then there
  // is no safety mechanism that ensures an event is not freed twice, or that
  // its free callback is invoked in the proper EventLoop, etc.
  for (const UniquePtr<Nanoapp>& app : mNanoapps) {
    if ((event->targetInstanceId == chre::kBroadcastInstanceId
            && app->isRegisteredForBroadcastEvent(event->eventType))
        || event->targetInstanceId == app->getInstanceId()) {
      app->postEvent(event);
    }
  }

  if (event->isUnreferenced()) {
    // Events sent to the system instance ID are processed via the free callback
    // and are not expected to be delivered to any nanoapp, so no need to log a
    // warning in that case
    if (event->senderInstanceId != kSystemInstanceId) {
      LOGW("Dropping event 0x%" PRIx16, event->eventType);
    }
    freeEvent(event);
  }
}

void EventLoop::flushInboundEventQueue() {
  while (!mEvents.empty()) {
    distributeEvent(mEvents.pop());
  }
}

void EventLoop::flushNanoappEventQueues() {
  while (deliverEvents());
}

void EventLoop::freeEvent(Event *event) {
  if (event->freeCallback != nullptr) {
    // TODO: find a better way to set the context to the creator of the event
    mCurrentApp = lookupAppByInstanceId(event->senderInstanceId);
    event->freeCallback(event->eventType, event->eventData);
    mCurrentApp = nullptr;
  }

  mEventPool.deallocate(event);
}

Nanoapp *EventLoop::lookupAppByAppId(uint64_t appId) {
  for (const UniquePtr<Nanoapp>& app : mNanoapps) {
    if (app->getAppId() == appId) {
      return app.get();
    }
  }

  return nullptr;
}

Nanoapp *EventLoop::lookupAppByInstanceId(uint32_t instanceId) {
  // The system instance ID always has nullptr as its Nanoapp pointer, so can
  // skip iterating through the nanoapp list for that case
  if (instanceId != kSystemInstanceId) {
    for (const UniquePtr<Nanoapp>& app : mNanoapps) {
      if (app->getInstanceId() == instanceId) {
        return app.get();
      }
    }
  }

  return nullptr;
}

void EventLoop::unloadNanoappAtIndex(size_t index) {
  const UniquePtr<Nanoapp>& nanoapp = mNanoapps[index];

  // Let the app know it's going away
  mCurrentApp = nanoapp.get();
  nanoapp->end();
  mCurrentApp = nullptr;

  // Destroy the Nanoapp instance
  {
    LockGuard<Mutex> lock(mNanoappsLock);
    mNanoapps.erase(index);
  }
}

}  // namespace chre
