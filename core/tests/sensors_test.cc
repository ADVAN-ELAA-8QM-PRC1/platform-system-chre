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

#include "gtest/gtest.h"

#include "chre/core/sensors.h"

using chre::Nanoseconds;
using chre::SensorMode;
using chre::SensorRequest;

TEST(SensorRequest, DefaultMinimalPriority) {
  SensorRequest request;
  EXPECT_EQ(request.getInterval(), Nanoseconds(0));
  EXPECT_EQ(request.getLatency(), Nanoseconds(0));
  EXPECT_EQ(request.getMode(), SensorMode::Off);
}

TEST(SensorRequest, ActiveContinuousIsHigherPriorityThanActiveOneShot) {
  SensorRequest activeContinuous(SensorMode::ActiveContinuous,
                                 Nanoseconds(0), Nanoseconds(0));
  SensorRequest activeOneShot(SensorMode::ActiveOneShot,
                              Nanoseconds(0), Nanoseconds(0));
  SensorRequest mergedRequest = activeContinuous
      .generateIntersectionOf(activeOneShot);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, ActiveOneShotIsHigherPriorityThanPassiveContinuous) {
  SensorRequest activeOneShot(SensorMode::ActiveOneShot,
                              Nanoseconds(0), Nanoseconds(0));
  SensorRequest passiveContinuous(SensorMode::PassiveContinuous,
                                  Nanoseconds(0), Nanoseconds(0));
  SensorRequest mergedRequest = activeOneShot
      .generateIntersectionOf(passiveContinuous);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveOneShot);

}

TEST(SensorRequest, PassiveContinuousIsHigherPriorityThanPassiveOneShot) {
  SensorRequest passiveContinuous(SensorMode::PassiveContinuous,
                                  Nanoseconds(0), Nanoseconds(0));
  SensorRequest passiveOneShot(SensorMode::PassiveOneShot,
                               Nanoseconds(0), Nanoseconds(0));
  SensorRequest mergedRequest = passiveContinuous
      .generateIntersectionOf(passiveOneShot);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::PassiveContinuous);
}

TEST(SensorRequest, PassiveOneShotIsHigherPriorityThanOff) {
  SensorRequest passiveOneShot(SensorMode::PassiveOneShot,
                               Nanoseconds(0), Nanoseconds(0));
  SensorRequest off(SensorMode::Off, Nanoseconds(0), Nanoseconds(0));
  SensorRequest mergedRequest = passiveOneShot
      .generateIntersectionOf(off);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::PassiveOneShot);
}

TEST(SensorRequest, LowerLatencyIsHigherPriorityThanHigherLatency) {
  SensorRequest lowLatencyRequest(SensorMode::ActiveContinuous,
                                  Nanoseconds(10), Nanoseconds(10));
  SensorRequest highLatencyRequest(SensorMode::ActiveOneShot,
                                   Nanoseconds(10), Nanoseconds(100));
  SensorRequest mergedRequest = lowLatencyRequest
      .generateIntersectionOf(highLatencyRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, HigherFrequencyIsHigherPriorityThanLowerFrequency) {
  SensorRequest lowFreqRequest(SensorMode::ActiveOneShot,
                               Nanoseconds(100), Nanoseconds(10));
  SensorRequest highFreqRequest(SensorMode::ActiveContinuous,
                                Nanoseconds(10), Nanoseconds(10));
  SensorRequest mergedRequest = lowFreqRequest
      .generateIntersectionOf(highFreqRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, OnlyDefaultFrequency) {
  SensorRequest defaultFreqRequest(SensorMode::ActiveContinuous,
                                   Nanoseconds(CHRE_SENSOR_INTERVAL_DEFAULT),
                                   Nanoseconds(0));
  SensorRequest mergedRequest = defaultFreqRequest
      .generateIntersectionOf(defaultFreqRequest);
  EXPECT_EQ(mergedRequest.getInterval(),
            Nanoseconds(CHRE_SENSOR_INTERVAL_DEFAULT));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, NonDefaultAndDefaultFrequency) {
  SensorRequest defaultFreqRequest(SensorMode::ActiveContinuous,
                                   Nanoseconds(CHRE_SENSOR_INTERVAL_DEFAULT),
                                   Nanoseconds(0));
  SensorRequest nonDefaultFreqRequest(SensorMode::ActiveContinuous,
                                      Nanoseconds(20000000), Nanoseconds(0));
  SensorRequest mergedRequest = defaultFreqRequest
      .generateIntersectionOf(nonDefaultFreqRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(20000000));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(0));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, OnlyAsapLatency) {
  SensorRequest asapLatencyRequest(SensorMode::ActiveContinuous,
                                   Nanoseconds(10),
                                   Nanoseconds(CHRE_SENSOR_LATENCY_ASAP));
  SensorRequest mergedRequest = asapLatencyRequest
      .generateIntersectionOf(asapLatencyRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getLatency(),
            Nanoseconds(CHRE_SENSOR_LATENCY_ASAP));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, NonAsapAndAsapLatency) {
  SensorRequest asapLatencyRequest(SensorMode::ActiveContinuous,
                                   Nanoseconds(10),
                                   Nanoseconds(CHRE_SENSOR_LATENCY_ASAP));
  SensorRequest nonAsapLatencyRequest(SensorMode::ActiveContinuous,
                                      Nanoseconds(10),
                                      Nanoseconds(2000));
  SensorRequest mergedRequest = asapLatencyRequest
      .generateIntersectionOf(nonAsapLatencyRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getLatency(),
            Nanoseconds(CHRE_SENSOR_LATENCY_ASAP));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, OnlyDefaultLatency) {
  SensorRequest defaultLatencyRequest(SensorMode::ActiveContinuous,
                                      Nanoseconds(10),
                                      Nanoseconds(CHRE_SENSOR_LATENCY_DEFAULT));
  SensorRequest mergedRequest = defaultLatencyRequest
      .generateIntersectionOf(defaultLatencyRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getLatency(),
            Nanoseconds(CHRE_SENSOR_LATENCY_DEFAULT));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}

TEST(SensorRequest, NonDefaultAndDefaultLatency) {
  SensorRequest defaultLatencyRequest(SensorMode::ActiveContinuous,
                                      Nanoseconds(10),
                                      Nanoseconds(CHRE_SENSOR_LATENCY_DEFAULT));
  SensorRequest nonDefaultLatencyRequest(SensorMode::ActiveContinuous,
                                         Nanoseconds(10),
                                         Nanoseconds(2000));
  SensorRequest mergedRequest = defaultLatencyRequest
      .generateIntersectionOf(nonDefaultLatencyRequest);
  EXPECT_EQ(mergedRequest.getInterval(), Nanoseconds(10));
  EXPECT_EQ(mergedRequest.getLatency(), Nanoseconds(2000));
  EXPECT_EQ(mergedRequest.getMode(), SensorMode::ActiveContinuous);
}
