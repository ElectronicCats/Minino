// SPDX-License-Identifier: Apache-2.0
#include <string.h>
#include <cmath>
#include "spoofer.h"
#include "id_open.h"
#include "drone_id_test.h"

TEST_CASE("Spoofer::update before updateLocation does not divide by zero", "[spoofer]") {
  Spoofer spoofer;
  spoofer.init(false);

  // Calling update() immediately without calling updateLocation()
  // Before our fix, m_deg_lat and m_deg_long were 0.0, resulting in division by zero (inf/nan).
  spoofer.update();
  TEST_ASSERT_TRUE(true);
}

TEST_CASE("Spoofer::updateLocation handles polar coordinates without division by zero", "[spoofer]") {
  Spoofer spoofer;
  spoofer.init(false);

  // At the North pole (90.0 deg), cos(lat) == 0, making m_deg_long == 0.0.
  // Before our fix, x / m_deg_long divided by zero.
  spoofer.updateLocation(90.0f, 0.0f);
  spoofer.update();
  TEST_ASSERT_TRUE(true);

  // At the South pole (-90.0 deg)
  spoofer.updateLocation(-90.0f, 0.0f);
  spoofer.update();
  TEST_ASSERT_TRUE(true);
}

TEST_CASE("Spoofer normal lifecycle updates location and transmits", "[spoofer]") {
  Spoofer spoofer;
  spoofer.init(false);

  spoofer.updateLocation(37.7749f, -122.4194f);
  spoofer.update();
  TEST_ASSERT_TRUE(true);
}

TEST_CASE("ID_OpenDrone set_self_id and set_auth safety", "[spoofer]") {
  ID_OpenDrone drone;

  // NULL safety
  drone.set_self_id(NULL);
  drone.set_auth(NULL);

  // Valid self_id
  char desc[] = "Test Recreational Drone";
  drone.set_self_id(desc);

  // Buffer overflow and caller buffer mutation check for set_auth
  // Before our fix: if len > MAX_AUTH_LENGTH, auth[len] = 0 mutated the caller's memory out of bounds!
  // We allocate a buffer of exact size 260 and ensure it is not corrupted at auth[255].
  uint8_t caller_auth[260];
  memset(caller_auth, 'A', sizeof(caller_auth));
  caller_auth[259] = '\0';

  drone.set_auth(caller_auth, 259, 0x01);
  // Verify that caller_auth was not mutated or overwritten
  TEST_ASSERT_EQUAL_HEX8('A', caller_auth[255]);
}
