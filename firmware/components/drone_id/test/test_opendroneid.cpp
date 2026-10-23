// SPDX-License-Identifier: Apache-2.0
#include <string.h>
#include "drone_id_test.h"
#include "opendroneid.h"

TEST_CASE("init functions handle NULL pointers safely", "[opendroneid]") {
  odid_initBasicIDData(NULL);
  odid_initLocationData(NULL);
  odid_initAuthData(NULL);
  odid_initSelfIDData(NULL);
  odid_initSystemData(NULL);
  odid_initOperatorIDData(NULL);
  odid_initMessagePackData(NULL);
  odid_initUasData(NULL);
  TEST_ASSERT_TRUE(true);
}

TEST_CASE("encode and decode BasicID message", "[opendroneid]") {
  ODID_BasicID_data data;
  odid_initBasicIDData(&data);
  data.IDType = ODID_IDTYPE_SERIAL_NUMBER;
  data.UAType = ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;
  strncpy(data.UASID, "1234567890ABCDEF", sizeof(data.UASID));

  ODID_BasicID_encoded enc;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeBasicIDMessage(&enc, &data));

  ODID_BasicID_data dec;
  odid_initBasicIDData(&dec);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeBasicIDMessage(&dec, &enc));

  TEST_ASSERT_EQUAL_INT(data.IDType, dec.IDType);
  TEST_ASSERT_EQUAL_INT(data.UAType, dec.UAType);
  TEST_ASSERT_EQUAL_STRING(data.UASID, dec.UASID);
}

TEST_CASE("encode and decode Location message with boundaries",
          "[opendroneid]") {
  ODID_Location_data data;
  odid_initLocationData(&data);
  data.Status = ODID_STATUS_AIRBORNE;
  data.Direction = 270.0f;
  data.SpeedHorizontal = 15.5f;
  data.SpeedVertical = -2.5f;
  data.Latitude = 37.7749;
  data.Longitude = -122.4194;
  data.AltitudeBaro = 120.0f;
  data.AltitudeGeo = 125.0f;
  data.HeightType = ODID_HEIGHT_REF_OVER_TAKEOFF;
  data.Height = 50.0f;
  data.HorizAccuracy = ODID_HOR_ACC_1_METER;
  data.VertAccuracy = ODID_VER_ACC_1_METER;
  data.BaroAccuracy = ODID_VER_ACC_1_METER;
  data.SpeedAccuracy = ODID_SPEED_ACC_0_3_METERS_PER_SECOND;
  data.TSAccuracy = ODID_TIME_ACC_0_1_SECOND;
  data.TimeStamp = 1234.5f;

  ODID_Location_encoded enc;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeLocationMessage(&enc, &data));

  ODID_Location_data dec;
  odid_initLocationData(&dec);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeLocationMessage(&dec, &enc));

  TEST_ASSERT_EQUAL_INT(data.Status, dec.Status);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, data.Direction, dec.Direction);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, data.SpeedHorizontal, dec.SpeedHorizontal);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, data.SpeedVertical, dec.SpeedVertical);
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, data.Latitude, dec.Latitude);
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, data.Longitude, dec.Longitude);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, data.AltitudeBaro, dec.AltitudeBaro);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, data.AltitudeGeo, dec.AltitudeGeo);
  TEST_ASSERT_FLOAT_WITHIN(0.2f, data.TimeStamp, dec.TimeStamp);
}

TEST_CASE("encode and decode Auth message across pages", "[opendroneid]") {
  // Page 0
  ODID_Auth_data data0;
  odid_initAuthData(&data0);
  data0.DataPage = 0;
  data0.AuthType = ODID_AUTH_UAS_ID_SIGNATURE;
  data0.LastPageIndex = 1;
  data0.Length = 30;
  data0.Timestamp = 1600000;
  memcpy(data0.AuthData, "12345678901234567", ODID_AUTH_PAGE_ZERO_DATA_SIZE);

  ODID_Auth_encoded enc0;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeAuthMessage(&enc0, &data0));

  ODID_Auth_data dec0;
  odid_initAuthData(&dec0);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeAuthMessage(&dec0, &enc0));
  TEST_ASSERT_EQUAL_INT(data0.DataPage, dec0.DataPage);
  TEST_ASSERT_EQUAL_INT(data0.AuthType, dec0.AuthType);
  TEST_ASSERT_EQUAL_INT(data0.LastPageIndex, dec0.LastPageIndex);
  TEST_ASSERT_EQUAL_UINT32(data0.Timestamp, dec0.Timestamp);

  // Page 1
  ODID_Auth_data data1;
  odid_initAuthData(&data1);
  data1.DataPage = 1;
  data1.AuthType = ODID_AUTH_UAS_ID_SIGNATURE;
  memcpy(data1.AuthData, "abcdefghijklmnopqrstuvw",
         ODID_AUTH_PAGE_NONZERO_DATA_SIZE);

  ODID_Auth_encoded enc1;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeAuthMessage(&enc1, &data1));

  ODID_Auth_data dec1;
  odid_initAuthData(&dec1);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeAuthMessage(&dec1, &enc1));
  TEST_ASSERT_EQUAL_INT(1, dec1.DataPage);
}

TEST_CASE("encode and decode SelfID message", "[opendroneid]") {
  ODID_SelfID_data data;
  odid_initSelfIDData(&data);
  data.DescType = ODID_DESC_TYPE_TEXT;
  strncpy(data.Desc, "Recreational Drone", sizeof(data.Desc));

  ODID_SelfID_encoded enc;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeSelfIDMessage(&enc, &data));

  ODID_SelfID_data dec;
  odid_initSelfIDData(&dec);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeSelfIDMessage(&dec, &enc));
  TEST_ASSERT_EQUAL_INT(data.DescType, dec.DescType);
  TEST_ASSERT_EQUAL_STRING(data.Desc, dec.Desc);
}

TEST_CASE("encode and decode System message", "[opendroneid]") {
  ODID_System_data data;
  odid_initSystemData(&data);
  data.OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
  data.ClassificationType = ODID_CLASSIFICATION_TYPE_EU;
  data.OperatorLatitude = 48.8566;
  data.OperatorLongitude = 2.3522;
  data.AreaCount = 1;
  data.AreaRadius = 500;
  data.AreaCeiling = 150.0f;
  data.AreaFloor = 0.0f;
  data.CategoryEU = ODID_CATEGORY_EU_OPEN;
  data.ClassEU = ODID_CLASS_EU_CLASS_1;
  data.OperatorAltitudeGeo = 35.0f;
  data.Timestamp = 1600000;

  ODID_System_encoded enc;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeSystemMessage(&enc, &data));

  ODID_System_data dec;
  odid_initSystemData(&dec);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeSystemMessage(&dec, &enc));
  TEST_ASSERT_EQUAL_INT(data.OperatorLocationType, dec.OperatorLocationType);
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, data.OperatorLatitude, dec.OperatorLatitude);
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, data.OperatorLongitude,
                            dec.OperatorLongitude);
  TEST_ASSERT_EQUAL_UINT16(data.AreaRadius, dec.AreaRadius);
}

TEST_CASE("encode and decode OperatorID message", "[opendroneid]") {
  ODID_OperatorID_data data;
  odid_initOperatorIDData(&data);
  data.OperatorIdType = ODID_OPERATOR_ID;
  strncpy(data.OperatorId, "FIN123456789012a", sizeof(data.OperatorId));

  ODID_OperatorID_encoded enc;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeOperatorIDMessage(&enc, &data));

  ODID_OperatorID_data dec;
  odid_initOperatorIDData(&dec);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeOperatorIDMessage(&dec, &enc));
  TEST_ASSERT_EQUAL_INT(data.OperatorIdType, dec.OperatorIdType);
  TEST_ASSERT_EQUAL_STRING(data.OperatorId, dec.OperatorId);
}

TEST_CASE("encode and decode MessagePack with multiple messages",
          "[opendroneid]") {
  ODID_MessagePack_data packData;
  odid_initMessagePackData(&packData);

  // Message 0: BasicID
  ODID_BasicID_data bData;
  odid_initBasicIDData(&bData);
  bData.IDType = ODID_IDTYPE_SERIAL_NUMBER;
  strncpy(bData.UASID, "SN12345", sizeof(bData.UASID));
  encodeBasicIDMessage((ODID_BasicID_encoded*) &packData.Messages[0], &bData);

  // Message 1: Location
  ODID_Location_data lData;
  odid_initLocationData(&lData);
  lData.Status = ODID_STATUS_AIRBORNE;
  lData.Latitude = 10.0;
  lData.Longitude = 20.0;
  encodeLocationMessage((ODID_Location_encoded*) &packData.Messages[1], &lData);

  packData.MsgPackSize = 2;

  ODID_MessagePack_encoded encPack;
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, encodeMessagePack(&encPack, &packData));

  ODID_UAS_Data uasData;
  odid_initUasData(&uasData);
  TEST_ASSERT_EQUAL_INT(ODID_SUCCESS, decodeMessagePack(&uasData, &encPack));

  TEST_ASSERT_EQUAL_INT(1, uasData.BasicIDValid[0]);
  TEST_ASSERT_EQUAL_STRING("SN12345", uasData.BasicID[0].UASID);
  TEST_ASSERT_EQUAL_INT(1, uasData.LocationValid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-5, 10.0, uasData.Location.Latitude);
}

TEST_CASE("NULL safety across print and decode functions", "[opendroneid]") {
  printByteArray(NULL, 10, 1);
  printBasicID_data(NULL);
  printLocation_data(NULL);
  printAuth_data(NULL);
  printSelfID_data(NULL);
  printSystem_data(NULL);
  printOperatorID_data(NULL);

  ODID_UAS_Data uasData;
  odid_initUasData(&uasData);
  TEST_ASSERT_EQUAL_INT(ODID_MESSAGETYPE_INVALID,
                        decodeOpenDroneID(NULL, NULL));
  TEST_ASSERT_EQUAL_INT(ODID_MESSAGETYPE_INVALID,
                        decodeOpenDroneID(&uasData, NULL));
  TEST_ASSERT_EQUAL_INT(ODID_MESSAGETYPE_INVALID,
                        decodeOpenDroneID(NULL, (uint8_t*) &uasData));
}
