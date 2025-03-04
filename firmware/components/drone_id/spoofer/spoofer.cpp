#include "spoofer.h"

#include "esp_timer.h"

void Spoofer::init() {
  // time things
  memset(&clock_tm, 0, sizeof(struct tm));
  clock_tm.tm_hour = 10;
  clock_tm.tm_mday = 16;
  clock_tm.tm_mon = 11;
  clock_tm.tm_year = 122;
  tv.tv_sec = time_2 = mktime(&clock_tm);
  settimeofday(&tv, &utc);

  // utm things
  memset(&utm_parameters, 0, sizeof(utm_parameters));
  strcpy(utm_parameters.UAS_operator, getID().c_str());
  utm_parameters.region = 1;
  utm_parameters.EU_category = 1;
  utm_parameters.EU_class = 5;
  squitter.init(&utm_parameters);
  memset(&utm_data, 0, sizeof(utm_data));
}

void Spoofer::updateLocation(float latitude, float longitude) {
  // define location plus some noise
  double lat_d = utm_data.latitude_d = utm_data.base_latitude =
      latitude + (float) (rand() % 10 - 5) / 10000.0;

  double long_d = utm_data.longitude_d = utm_data.base_longitude =
      longitude + (float) (rand() % 10 - 5) / 10000.0;

  utm_data.base_valid = 1;
  utm_data.base_alt_m = (float) (rand() % 1000) / 10.0;

  utm_utils.calc_m_per_deg(lat_d, &m_deg_lat, &m_deg_long);
}

void Spoofer::update() {
  // FAA says minimum rate is 1 Hz, we do 2 Hz here
  if ((esp_timer_get_time() / 1000 - last_update) < 200) {
    return;
  }

  // update time calculations
  double time_elapsed_secs =
      double(esp_timer_get_time() / 1000 - last_update) / 1000.0;
  last_update = esp_timer_get_time() / 1000;

  // random number of satellites
  utm_data.satellites = rand() % 8 + 8;

  // random acceleration to change speed
  // bias towards flying near the center
  speed_m_x +=
      float(rand() % int(2 * max_accel) - max_accel) / 1000.0 - 0.05 * x;
  speed_m_y +=
      float(rand() % int(2 * max_accel) - max_accel) / 1000.0 - 0.05 * y;
  speed_m_x = (speed_m_x < -max_speed)
                  ? -max_speed
                  : (speed_m_x > max_speed ? max_speed : speed_m_x);
  speed_m_y = (speed_m_y < -max_speed)
                  ? -max_speed
                  : (speed_m_y > max_speed ? max_speed : speed_m_y);

  // update the actual speed in knots
  // double absolute_speed = sqrt(pow(speed_m_x, 2) + pow(speed_m_y, 2));
  double absolute_speed =
      std::sqrt(std::pow(speed_m_x, 2) + std::pow(speed_m_y, 2));
  utm_data.speed_kn = speed_ms2kn * absolute_speed;

  // compute the heading based on speed
  double heading_rads = atan2(speed_m_y, speed_m_x);
  int heading_degs = int(heading_rads * angle_rad2deg);
  utm_data.heading = heading_degs % 360;

  // calculate the new x, y
  x += speed_m_x * time_elapsed_secs;
  y += speed_m_y * time_elapsed_secs;

  // calculate new height
  float climbrate =
      float(rand() % int(2 * max_climbrate) - max_climbrate) / 1000.0;
  z = (z < -max_height) ? -max_height : (z > max_height ? max_height : z);
  utm_data.alt_msl_m = utm_data.base_alt_m + z;
  utm_data.alt_agl_m = z;

  // update the lat long degrees
  utm_data.latitude_d = utm_data.base_latitude + (y / m_deg_lat);
  utm_data.longitude_d = utm_data.base_longitude + (x / m_deg_long);

  // transmit things
  squitter.transmit(&utm_data);
}

std::string Spoofer::getID() {
  std::string characters = std::string(
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  std::string ID = "";
  for (int i = 0; i < 16; i++) {
    ID += characters[(rand() % characters.length())];
  }
  return ID;
}
