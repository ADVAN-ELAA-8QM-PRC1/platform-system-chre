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

extern "C" {

#include "fixed_point.h"
#include "qmi_client.h"
#include "sns_smgr_api_v01.h"
#include "timetick.h"

}  // extern "C"

#include "chre_api/chre/sensor.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/log.h"
#include "chre/platform/platform_sensor.h"
#include "chre/platform/slpi/platform_sensor_util.h"

namespace {

//! The QMI client handle.
qmi_client_type gPlatformSensorQmiClientHandle = nullptr;

//! A sensor report indication for deserializing sensor sample indications
//! into. This global instance is used to avoid thrashy use of the heap by
//! allocating and freeing this on the heap for every new sensor sample. This
//! relies on the assumption that the QMI callback is not reentrant.
sns_smgr_buffering_ind_msg_v01 gSmgrBufferingIndMsg;

}  // anonymous namespace

namespace chre {

//! The timeout for QMI messages in milliseconds.
constexpr uint32_t kQmiTimeoutMs = 1000;

constexpr float kMicroTeslaPerGauss = 100.0f;

/**
 * Converts a sensorId, dataType and calType as provided by SMGR to a
 * SensorType as used by platform-independent CHRE code. This is useful in
 * sensor discovery.
 *
 * @param sensorId The sensorID as provided by the SMGR request for sensor info.
 * @param dataType The dataType for the sesnor as provided by the SMGR request
 *                 for sensor info.
 * @param calType The calibration type (CAL_SEL) as defined in the SMGR API.
 * @return Returns the platform-independent sensor type or Unknown if no
 *         match is found.
 */
SensorType getSensorTypeFromSensorId(uint8_t sensorId, uint8_t dataType,
                                     uint8_t calType) {
  // Here be dragons. These constants below are defined in
  // sns_smgr_common_v01.h. Refer to the section labelled "Define sensor
  // identifier" for more details. This function relies on the ordering of
  // constants provided by their API. Do not change these values without care.
  // You have been warned!
  if (dataType == SNS_SMGR_DATA_TYPE_PRIMARY_V01) {
    if (sensorId >= SNS_SMGR_ID_ACCEL_V01
        && sensorId < SNS_SMGR_ID_GYRO_V01) {
      if (calType == SNS_SMGR_CAL_SEL_FULL_CAL_V01) {
        return SensorType::Accelerometer;
      } else if (calType == SNS_SMGR_CAL_SEL_FACTORY_CAL_V01) {
        return SensorType::UncalibratedAccelerometer;
      }
    } else if (sensorId >= SNS_SMGR_ID_GYRO_V01
        && sensorId < SNS_SMGR_ID_MAG_V01) {
      if (calType == SNS_SMGR_CAL_SEL_FULL_CAL_V01) {
        return SensorType::Gyroscope;
      } else if (calType == SNS_SMGR_CAL_SEL_FACTORY_CAL_V01) {
        return SensorType::UncalibratedGyroscope;
      }
    } else if (sensorId >= SNS_SMGR_ID_MAG_V01
        && sensorId < SNS_SMGR_ID_PRESSURE_V01) {
      if (calType == SNS_SMGR_CAL_SEL_FULL_CAL_V01) {
        return SensorType::GeomagneticField;
      } else if (calType == SNS_SMGR_CAL_SEL_FACTORY_CAL_V01) {
        return SensorType::UncalibratedGeomagneticField;
      }
    } else if (sensorId >= SNS_SMGR_ID_PRESSURE_V01
        && sensorId < SNS_SMGR_ID_PROX_LIGHT_V01) {
      return SensorType::Pressure;
    } else if (sensorId >= SNS_SMGR_ID_PROX_LIGHT_V01
        && sensorId < SNS_SMGR_ID_HUMIDITY_V01) {
      return SensorType::Proximity;
    }
  } else if (dataType == SNS_SMGR_DATA_TYPE_SECONDARY_V01) {
    if (sensorId >= SNS_SMGR_ID_ACCEL_V01
        && sensorId < SNS_SMGR_ID_GYRO_V01) {
      return SensorType::AccelerometerTemperature;
    } else if (sensorId >= SNS_SMGR_ID_GYRO_V01
        && sensorId < SNS_SMGR_ID_MAG_V01) {
      return SensorType::GyroscopeTemperature;
    } else if ((sensorId >= SNS_SMGR_ID_PROX_LIGHT_V01
        && sensorId < SNS_SMGR_ID_HUMIDITY_V01)
        || (sensorId >= SNS_SMGR_ID_ULTRA_VIOLET_V01
            && sensorId < SNS_SMGR_ID_OBJECT_TEMP_V01)) {
      return SensorType::Light;
    }
  }

  return SensorType::Unknown;
}

/**
 * Converts a reportId as provided by SMGR to a SensorType.
 *
 * @param reportId The reportID as provided by the SMGR buffering index.
 * @return Returns the sensorType that corresponds to the reportId.
 */
SensorType getSensorTypeFromReportId(uint8_t reportId) {
  SensorType sensorType;
  if (reportId < static_cast<uint8_t>(SensorType::SENSOR_TYPE_COUNT)) {
    sensorType = static_cast<SensorType>(reportId);
  } else {
    sensorType = SensorType::Unknown;
  }
  return sensorType;
}

/**
 * Converts a PlatformSensor to a unique report ID through SensorType. This is
 * useful in making sensor request.
 *
 * @param sensorId The sensorID as provided by the SMGR request for sensor info.
 * @param dataType The dataType for the sesnor as provided by the SMGR request
 *                 for sensor info.
 * @param calType The calibration type (CAL_SEL) as defined in the SMGR API.
 * @return Returns a unique report ID that is based on SensorType.
 */
uint8_t getReportId(uint8_t sensorId, uint8_t dataType, uint8_t calType) {
  SensorType sensorType = getSensorTypeFromSensorId(
      sensorId, dataType, calType);

  CHRE_ASSERT_LOG(sensorType != SensorType::Unknown,
                  "sensorId %" PRIu8 ", dataType %" PRIu8 ", calType %" PRIu8,
                  sensorId, dataType, calType);
  return static_cast<uint8_t>(sensorType);
}

/**
 * Checks whether the corresponding sensor is a sencondary temperature sensor.
 *
 * @param reportId The reportID as provided by the SMGR buffering index.
 * @return true if the sensor is a secondary temperature sensor.
 */
bool isSecondaryTemperature(uint8_t reportId) {
  SensorType sensorType = getSensorTypeFromReportId(reportId);
  return (sensorType == SensorType::AccelerometerTemperature
          || sensorType == SensorType::GyroscopeTemperature);
}

/**
 * Verifies whether the buffering index's report ID matches the expected
 * indices length.
 *
 * @return true if it's a valid pair of indices length and report ID.
 */
bool isValidIndicesLength() {
  return ((gSmgrBufferingIndMsg.Indices_len == 1
           && !isSecondaryTemperature(gSmgrBufferingIndMsg.ReportId))
          || (gSmgrBufferingIndMsg.Indices_len == 2
              && isSecondaryTemperature(gSmgrBufferingIndMsg.ReportId)));
}

/**
 * Adds a Platform sensor to the sensor list.
 *
 * @param sensorId The sensorID as provided by the SMGR request for sensor info.
 * @param dataType The dataType for the sesnor as provided by the SMGR request
 *                 for sensor info.
 * @param calType The calibration type (CAL_SEL) as defined in the SMGR API.
 * @param sensor The sensor list.
 */
void addPlatformSensor(uint8_t sensorId, uint8_t dataType, uint8_t calType,
                       DynamicVector<PlatformSensor> *sensors) {
  PlatformSensor platformSensor;
  platformSensor.sensorId = sensorId;
  platformSensor.dataType = dataType;
  platformSensor.calType = calType;
  if (!sensors->push_back(std::move(platformSensor))) {
    FATAL_ERROR("Failed to allocate new sensor: out of memory");
  }
}

/**
 * Converts SMGR ticks to nanoseconds as a uint64_t.
 *
 * @param ticks The number of ticks.
 * @return The number of nanoseconds represented by the ticks value.
 */
uint64_t getNanosecondsFromSmgrTicks(uint32_t ticks) {
  return (ticks * Seconds(1).toRawNanoseconds())
      / TIMETICK_NOMINAL_FREQ_HZ;
}

void smgrSensorDataEventFree(uint16_t eventType, void *eventData) {
  // Events are allocated using the simple memoryAlloc/memoryFree platform
  // functions.
  // TODO: Consider using a MemoryPool.
  memoryFree(eventData);
}

/**
 * Populate the header
 */
void populateSensorDataHeader(
    SensorType sensorType, chreSensorDataHeader *header,
    const sns_smgr_buffering_sample_index_s_v01& sensorIndex) {
  uint64_t baseTimestamp = getNanosecondsFromSmgrTicks(
      sensorIndex.FirstSampleTimestamp);
  memset(header->reserved, 0, sizeof(header->reserved));
  header->baseTimestamp = baseTimestamp;
  header->sensorHandle = getSensorHandleFromSensorType(sensorType);
  header->readingCount = sensorIndex.SampleCount;
}

/**
 * Populate three-axis event data.
 */
void populateThreeAxisEvent(
    SensorType sensorType, chreSensorThreeAxisData *data,
    const sns_smgr_buffering_sample_index_s_v01& sensorIndex) {
  populateSensorDataHeader(sensorType, &data->header, sensorIndex);

  for (size_t i = 0; i < sensorIndex.SampleCount; i++) {
    const sns_smgr_buffering_sample_s_v01& sensorData =
        gSmgrBufferingIndMsg.Samples[i + sensorIndex.FirstSampleIdx];

    // TimeStampOffset has max value of < 2 sec so it will not overflow here.
    data->readings[i].timestampDelta =
        getNanosecondsFromSmgrTicks(sensorData.TimeStampOffset);

    // Convert from SMGR's NED coordinate to Android coordinate.
    data->readings[i].x = FX_FIXTOFLT_Q16(sensorData.Data[1]);
    data->readings[i].y = FX_FIXTOFLT_Q16(sensorData.Data[0]);
    data->readings[i].z = -FX_FIXTOFLT_Q16(sensorData.Data[2]);

    // Convert from Gauss to micro Tesla
    if (sensorType == SensorType::GeomagneticField
        || sensorType == SensorType::UncalibratedGeomagneticField) {
      data->readings[i].x *= kMicroTeslaPerGauss;
      data->readings[i].y *= kMicroTeslaPerGauss;
      data->readings[i].z *= kMicroTeslaPerGauss;
    }
  }
}

/**
 * Populate float event data.
 */
void populateFloatEvent(
    SensorType sensorType, chreSensorFloatData *data,
    const sns_smgr_buffering_sample_index_s_v01& sensorIndex) {
  populateSensorDataHeader(sensorType, &data->header, sensorIndex);

  for (size_t i = 0; i < sensorIndex.SampleCount; i++) {
    const sns_smgr_buffering_sample_s_v01& sensorData =
        gSmgrBufferingIndMsg.Samples[i + sensorIndex.FirstSampleIdx];

    // TimeStampOffset has max value of < 2 sec so it will not overflow.
    data->readings[i].timestampDelta =
        getNanosecondsFromSmgrTicks(sensorData.TimeStampOffset);
    data->readings[i].value = FX_FIXTOFLT_Q16(sensorData.Data[0]);
  }
}

/**
 * Allocate event memory according to SensorType and populate event readings.
 */
void *allocateAndPopulateEvent(
    SensorType sensorType,
    const sns_smgr_buffering_sample_index_s_v01& sensorIndex) {
  SensorSampleType sampleType = getSensorSampleTypeFromSensorType(sensorType);
  size_t memorySize = sizeof(chreSensorDataHeader);
  switch (sampleType) {
    case SensorSampleType::ThreeAxis: {
      memorySize += sensorIndex.SampleCount *
          sizeof(chreSensorThreeAxisData::chreSensorThreeAxisSampleData);
      auto *event =
          static_cast<chreSensorThreeAxisData *>(memoryAlloc(memorySize));
      if (event != nullptr) {
        populateThreeAxisEvent(sensorType, event, sensorIndex);
      }
      return event;
    }

    case SensorSampleType::Float: {
      memorySize += sensorIndex.SampleCount *
          sizeof(chreSensorFloatData::chreSensorFloatSampleData);
      auto *event =
          static_cast<chreSensorFloatData *>(memoryAlloc(memorySize));
      if (event != nullptr) {
        populateFloatEvent(sensorType, event, sensorIndex);
      }
      return event;
    }

    default:
      LOGW("Unhandled sensor data %" PRIu8, sensorType);
      return nullptr;
  }
}

/**
 * Handles sensor data provided by the SMGR framework. This function does not
 * return but logs errors and warnings.
 *
 * @param userHandle The userHandle is used by the QMI decode function.
 * @param buffer The buffer to decode sensor data from.
 * @param bufferLength The size of the buffer to decode.
 */
void handleSensorDataIndication(void *userHandle, void *buffer,
                                unsigned int bufferLength) {
  int status = qmi_client_message_decode(
      userHandle, QMI_IDL_INDICATION, SNS_SMGR_BUFFERING_IND_V01, buffer,
      bufferLength, &gSmgrBufferingIndMsg,
      sizeof(sns_smgr_buffering_ind_msg_v01));
  if (status != QMI_NO_ERR) {
    LOGE("Error parsing sensor data indication %d", status);
    return;
  }

  // We only requested one sensor per request except for a secondary
  // secondary temperature sensor.
  bool validReport = isValidIndicesLength();
  CHRE_ASSERT_LOG(validReport,
                  "Got buffering indication from %" PRIu32
                  " sensors with report ID %" PRIu8,
                  gSmgrBufferingIndMsg.Indices_len,
                  gSmgrBufferingIndMsg.ReportId);
  if (validReport) {
    // Identify the index for the desired sensor. It is always 0 except possibly
    // for a secondary temperature sensor.
    uint32_t index = 0;
    if (isSecondaryTemperature(gSmgrBufferingIndMsg.ReportId)) {
      index = (gSmgrBufferingIndMsg.Indices[0].DataType
               == SNS_SMGR_DATA_TYPE_SECONDARY_V01) ? 0 : 1;
    }
    const sns_smgr_buffering_sample_index_s_v01& sensorIndex =
        gSmgrBufferingIndMsg.Indices[index];

    // Use ReportID to identify sensors as gSmgrBufferingIndMsg.Samples[i].Flags
    // are not populated.
    SensorType sensorType = getSensorTypeFromReportId(
        gSmgrBufferingIndMsg.ReportId);
    if (sensorType == SensorType::Unknown) {
      LOGW("Received sensor sample for unknown sensor %" PRIu8 " %" PRIu8,
           sensorIndex.SensorId, sensorIndex.DataType);
    } else {
      void *eventData = allocateAndPopulateEvent(sensorType, sensorIndex);
      if (eventData == nullptr) {
        LOGW("Dropping event due to allocation failure");
      } else {
        EventLoopManagerSingleton::get()->postEvent(
            getSampleEventTypeForSensorType(sensorType), eventData,
            smgrSensorDataEventFree);
      }
    }
  }
}

/**
 * This callback is invoked by the QMI framework when an asynchronous message is
 * delivered. Unhandled messages are logged. The signature is defined by the QMI
 * library.
 *
 * @param userHandle The userHandle is used by the QMI library.
 * @param messageId The type of the message to decode.
 * @param buffer The buffer to decode.
 * @param bufferLength The length of the buffer to decode.
 * @param callbackData Data that is provided as a context to this callback. This
 *                     is not used in this context.
 */
void platformSensorQmiIndicationCallback(void *userHandle,
                                         unsigned int messageId,
                                         void *buffer,
                                         unsigned int bufferLength,
                                         void *callbackData) {
  switch (messageId) {
    case SNS_SMGR_BUFFERING_IND_V01:
      handleSensorDataIndication(userHandle, buffer, bufferLength);
      break;
    default:
      LOGW("Received unhandled sensor QMI indication message: %u", messageId);
      break;
  };
}

void PlatformSensor::init() {
  qmi_idl_service_object_type sensorServiceObject =
      SNS_SMGR_SVC_get_service_object_v01();
  if (sensorServiceObject == nullptr) {
    FATAL_ERROR("Failed to obtain the SNS SMGR service instance");
  }

  qmi_client_os_params sensorContextOsParams;
  qmi_client_error_type status = qmi_client_init_instance(sensorServiceObject,
      QMI_CLIENT_INSTANCE_ANY, &platformSensorQmiIndicationCallback, nullptr,
      &sensorContextOsParams, kQmiTimeoutMs, &gPlatformSensorQmiClientHandle);
  if (status != QMI_NO_ERR) {
    FATAL_ERROR("Failed to initialize the sensors QMI client: %d", status);
  }
}

void PlatformSensor::deinit() {
  qmi_client_release(&gPlatformSensorQmiClientHandle);
  gPlatformSensorQmiClientHandle = nullptr;
}

/**
 * Requests the sensors for a given sensor ID and appends them to the provided
 * list of sensors. If an error occurs, false is returned.
 *
 * @param sensorId The sensor ID to request sensor info for.
 * @param sensors The list of sensors to append newly found sensors to.
 * @return Returns false if an error occurs.
 */
bool getSensorsForSensorId(uint8_t sensorId,
                           DynamicVector<PlatformSensor> *sensors) {
  sns_smgr_single_sensor_info_req_msg_v01 sensorInfoRequest;
  sns_smgr_single_sensor_info_resp_msg_v01 sensorInfoResponse;

  sensorInfoRequest.SensorID = sensorId;

  qmi_client_error_type status = qmi_client_send_msg_sync(
      gPlatformSensorQmiClientHandle, SNS_SMGR_SINGLE_SENSOR_INFO_REQ_V01,
      &sensorInfoRequest, sizeof(sns_smgr_single_sensor_info_req_msg_v01),
      &sensorInfoResponse, sizeof(sns_smgr_single_sensor_info_resp_msg_v01),
      kQmiTimeoutMs);

  bool success = false;
  if (status != QMI_NO_ERR) {
    LOGE("Error requesting single sensor info: %d", status);
  } else if (sensorInfoResponse.Resp.sns_result_t != SNS_RESULT_SUCCESS_V01) {
    LOGE("Single sensor info request failed with error: %d",
         sensorInfoResponse.Resp.sns_err_t);
  } else {
    const sns_smgr_sensor_info_s_v01& sensorInfoList =
        sensorInfoResponse.SensorInfo;
    for (uint32_t i = 0; i < sensorInfoList.data_type_info_len; i++) {
      const sns_smgr_sensor_datatype_info_s_v01 *sensorInfo =
          &sensorInfoList.data_type_info[i];
      LOGD("SensorID %" PRIu8 ", DataType %" PRIu8 ", MaxRate %" PRIu16
           "Hz, SensorName %s",
           sensorInfo->SensorID, sensorInfo->DataType,
           sensorInfo->MaxSampleRate, sensorInfo->SensorName);

      SensorType sensorType = getSensorTypeFromSensorId(
          sensorInfo->SensorID, sensorInfo->DataType,
          SNS_SMGR_CAL_SEL_FULL_CAL_V01);
      if (sensorType != SensorType::Unknown) {
        addPlatformSensor(sensorInfo->SensorID, sensorInfo->DataType,
                          SNS_SMGR_CAL_SEL_FULL_CAL_V01, sensors);

        // Add an uncalibrated version if defined.
        SensorType uncalibratedType = getSensorTypeFromSensorId(
            sensorInfo->SensorID, sensorInfo->DataType,
            SNS_SMGR_CAL_SEL_FACTORY_CAL_V01);
        if (sensorType != uncalibratedType) {
          addPlatformSensor(sensorInfo->SensorID, sensorInfo->DataType,
                            SNS_SMGR_CAL_SEL_FACTORY_CAL_V01, sensors);
        }
      }
    }

    success = true;
  }

  return success;
}

bool PlatformSensor::getSensors(DynamicVector<PlatformSensor> *sensors) {
  CHRE_ASSERT(sensors);

  sns_smgr_all_sensor_info_req_msg_v01 sensorListRequest;
  sns_smgr_all_sensor_info_resp_msg_v01 sensorListResponse;

  qmi_client_error_type status = qmi_client_send_msg_sync(
      gPlatformSensorQmiClientHandle, SNS_SMGR_ALL_SENSOR_INFO_REQ_V01,
      &sensorListRequest, sizeof(sns_smgr_all_sensor_info_req_msg_v01),
      &sensorListResponse, sizeof(sns_smgr_all_sensor_info_resp_msg_v01),
      kQmiTimeoutMs);

  bool success = false;
  if (status != QMI_NO_ERR) {
    LOGE("Error requesting sensor list: %d", status);
  } else if (sensorListResponse.Resp.sns_result_t != SNS_RESULT_SUCCESS_V01) {
    LOGE("Sensor list lequest failed with error: %d",
         sensorListResponse.Resp.sns_err_t);
  } else {
    success = true;
    for (uint32_t i = 0; i < sensorListResponse.SensorInfo_len; i++) {
      uint8_t sensorId = sensorListResponse.SensorInfo[i].SensorID;
      if (!getSensorsForSensorId(sensorId, sensors)) {
        success = false;
        break;
      }
    }
  }

  return success;
}

/**
 * Converts a SensorMode into an SMGR request action. When the net request for
 * a sensor is considered to be active an add operation is required for the
 * SMGR request. When the sensor becomes inactive the request is deleted.
 *
 * @param mode The sensor mode.
 * @return Returns the SMGR request action given the sensor mode.
 */
uint8_t getSmgrRequestActionForMode(SensorMode mode) {
  if (sensorModeIsActive(mode)) {
    return SNS_SMGR_BUFFERING_ACTION_ADD_V01;
  } else {
    return SNS_SMGR_BUFFERING_ACTION_DELETE_V01;
  }
}

/**
 * Populates a sns_smgr_buffering_req_msg_v01 struct to request sensor data.
 *
 * @param request The new request to set this sensor to.
 * @param sensorId The sensorID as provided by the SMGR request for sensor info.
 * @param dataType The dataType for the sesnor as provided by the SMGR request
 *                 for sensor info.
 * @param calType The calibration type (CAL_SEL) as defined in the SMGR API.
 * @param sensorDataRequest The pointer to the data request to be populated.
 */
void populateSensorRequest(
    const SensorRequest& request, uint8_t sensorId, uint8_t dataType,
    uint8_t calType, sns_smgr_buffering_req_msg_v01 *sensorRequest) {
  // Zero the fields in the request. All mandatory and unused fields are
  // specified to be set to false or zero so this is safe.
  memset(sensorRequest, 0, sizeof(*sensorRequest));

  // Build the request for one sensor at the requested rate. An add action for a
  // ReportID that is already in use causes a replacement of the last request.
  sensorRequest->ReportId = getReportId(sensorId, dataType, calType);
  sensorRequest->Action = getSmgrRequestActionForMode(request.getMode());
  Nanoseconds batchingInterval =
      (request.getLatency() > request.getInterval()) ?
      request.getLatency() : request.getInterval();
  sensorRequest->ReportRate = intervalToSmgrQ16ReportRate(batchingInterval);
  sensorRequest->Item_len = 1; // One sensor per request if possible.
  sensorRequest->Item[0].SensorId = sensorId;
  sensorRequest->Item[0].DataType = dataType;
  sensorRequest->Item[0].Decimation = SNS_SMGR_DECIMATION_RECENT_SAMPLE_V01;
  sensorRequest->Item[0].Calibration = calType;
  sensorRequest->Item[0].SamplingRate =
      intervalToSmgrSamplingRate(request.getInterval());

  // Add a dummy primary sensor to accompany a secondary temperature sensor.
  // This is requred by the SMGR. The primary sensor is requested with the same
  // (low) rate and the same latency, whose response data will be ignored.
  if (isSecondaryTemperature(sensorRequest->ReportId)) {
    sensorRequest->Item_len = 2;
    sensorRequest->Item[1].SensorId = sensorId;
    sensorRequest->Item[1].DataType = SNS_SMGR_DATA_TYPE_PRIMARY_V01;
    sensorRequest->Item[1].Decimation = SNS_SMGR_DECIMATION_RECENT_SAMPLE_V01;
    sensorRequest->Item[1].Calibration = SNS_SMGR_CAL_SEL_FULL_CAL_V01;
    sensorRequest->Item[1].SamplingRate = sensorRequest->Item[0].SamplingRate;
  }
}

bool PlatformSensor::setRequest(const SensorRequest& request) {
  // Allocate request and response for the sensor request.
  auto *sensorRequest = memoryAlloc<sns_smgr_buffering_req_msg_v01>();
  auto *sensorResponse = memoryAlloc<sns_smgr_buffering_resp_msg_v01>();

  bool success = false;
  if (sensorRequest == nullptr || sensorResponse == nullptr) {
    LOGE("Failed to allocated sensor request/response: out of memory");
  } else {
    populateSensorRequest(request, this->sensorId, this->dataType,
                          this->calType, sensorRequest);

    qmi_client_error_type status = qmi_client_send_msg_sync(
        gPlatformSensorQmiClientHandle, SNS_SMGR_BUFFERING_REQ_V01,
        sensorRequest, sizeof(*sensorRequest),
        sensorResponse, sizeof(*sensorResponse),
        kQmiTimeoutMs);

    if (status != QMI_NO_ERR) {
      LOGE("Error requesting sensor data: %d", status);
    } else if (sensorResponse->Resp.sns_result_t != SNS_RESULT_SUCCESS_V01
        || (sensorResponse->AckNak != SNS_SMGR_RESPONSE_ACK_SUCCESS_V01
            && sensorResponse->AckNak != SNS_SMGR_RESPONSE_ACK_MODIFIED_V01)) {
      LOGE("Sensor data request failed with error: %d, AckNak: %d",
           sensorResponse->Resp.sns_err_t, sensorResponse->AckNak);
    } else {
      success = true;
    }
  }

  memoryFree(sensorRequest);
  memoryFree(sensorResponse);
  return success;
}

SensorType PlatformSensor::getSensorType() const {
  return getSensorTypeFromSensorId(this->sensorId, this->dataType,
                                   this->calType);
}

}  // namespace chre
