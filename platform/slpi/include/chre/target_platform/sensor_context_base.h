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

#ifndef CHRE_PLATFORM_SLPI_SENSOR_CONTEXT_BASE_H_
#define CHRE_PLATFORM_SLPI_SENSOR_CONTEXT_BASE_H_

namespace chre {

/**
 * Storage for the SLPI implementation of the PlatformSensor class.
 */
class PlatformSensorBase {
 public:
  //! The handle to uniquely identify this sensor.
  uint8_t sensorId;

  //! The type of data that this sensor uses. SMGR overloads sensor IDs and
  //! allows them to behave as two sensors. The data type differentiates which
  //! sensor this object refers to.
  uint8_t dataType;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SLPI_SENSOR_CONTEXT_BASE_H_
