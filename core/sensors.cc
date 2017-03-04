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

#include <algorithm>

#include "chre/core/sensors.h"
#include "chre/platform/assert.h"
#include "chre/platform/fatal_error.h"

namespace chre {

const char *getSensorTypeName(SensorType sensorType) {
  switch (sensorType) {
    case SensorType::Unknown:
      return "Unknown";
    case SensorType::Accelerometer:
      return "Accelerometer";
    case SensorType::InstantMotion:
      return "Instant Motion";
    case SensorType::StationaryDetect:
      return "Stationary Detect";
    case SensorType::Gyroscope:
      return "Gyroscope";
    case SensorType::GeomagneticField:
      return "Geomagnetic Field";
    case SensorType::Pressure:
      return "Pressure";
    case SensorType::Light:
      return "Light";
    case SensorType::Proximity:
      return "Proximity";
    default:
      CHRE_ASSERT(false);
      return "";
  }
}

uint16_t getSampleEventTypeForSensorType(SensorType sensorType) {
  if (sensorType == SensorType::Unknown) {
    FATAL_ERROR("Tried to obtain the sensor sample event index for an unknown "
                "sensor type");
  }

  // The enum values of SensorType map to the defined values in the CHRE API.
  uint8_t sensorTypeValue = static_cast<uint8_t>(sensorType);
  return CHRE_EVENT_SENSOR_DATA_EVENT_BASE + sensorTypeValue;
}

SensorType getSensorTypeFromUnsignedInt(uint8_t sensorType) {
  switch (sensorType) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
      return SensorType::Accelerometer;
    case CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT:
      return SensorType::InstantMotion;
    case CHRE_SENSOR_TYPE_STATIONARY_DETECT:
      return SensorType::StationaryDetect;
    case CHRE_SENSOR_TYPE_GYROSCOPE:
      return SensorType::Gyroscope;
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
      return SensorType::GeomagneticField;
    case CHRE_SENSOR_TYPE_PRESSURE:
      return SensorType::Pressure;
    case CHRE_SENSOR_TYPE_LIGHT:
      return SensorType::Light;
    case CHRE_SENSOR_TYPE_PROXIMITY:
      return SensorType::Proximity;
    default:
      return SensorType::Unknown;
  }
}

SensorRequest::SensorRequest()
    : SensorRequest(SensorMode::Off,
                    Nanoseconds(0) /* interval */,
                    Nanoseconds(0) /* latency */) {}

SensorRequest::SensorRequest(SensorMode mode,
                             Nanoseconds interval,
                             Nanoseconds latency)
    : mInterval(interval), mLatency(latency), mMode(mode) {}

bool SensorRequest::isEquivalentTo(const SensorRequest& request) const {
  return (mMode == request.mMode
      && mInterval == request.mInterval
      && mLatency == request.mLatency);
}

SensorRequest SensorRequest::generateIntersectionOf(
    const SensorRequest& request) const {
  SensorMode maximalSensorMode = SensorMode::Off;
  Nanoseconds minimalInterval = std::min(mInterval, request.mInterval);
  Nanoseconds minimalLatency = std::min(mLatency, request.mLatency);

  // Compute the highest priority mode. Active continuous is the highest
  // priority and passive one-shot is the lowest.
  if (mMode == SensorMode::ActiveContinuous
      || request.mMode == SensorMode::ActiveContinuous) {
    maximalSensorMode = SensorMode::ActiveContinuous;
  } else if (mMode == SensorMode::ActiveOneShot
      || request.mMode == SensorMode::ActiveOneShot) {
    maximalSensorMode = SensorMode::ActiveOneShot;
  } else if (mMode == SensorMode::PassiveContinuous
      || request.mMode == SensorMode::PassiveContinuous) {
    maximalSensorMode = SensorMode::PassiveContinuous;
  } else if (mMode == SensorMode::PassiveOneShot
      || request.mMode == SensorMode::PassiveOneShot) {
    maximalSensorMode = SensorMode::PassiveOneShot;
  }

  return SensorRequest(maximalSensorMode, minimalInterval, minimalLatency);
}

Nanoseconds SensorRequest::getInterval() const {
  return mInterval;
}

Nanoseconds SensorRequest::getLatency() const {
  return mLatency;
}

SensorMode SensorRequest::getMode() const {
  return mMode;
}

}  // namespace chre
