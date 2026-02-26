#include "test_runner.h"
#include "../src/url_builder.h"

#include <cmath>
#include <cstdio>

// ---- FmtDbl ----------------------------------------------------------------

TEST(FmtDbl_basic) {
  REQUIRE_EQ(FmtDbl(0.0), "0.0000");
  REQUIRE_EQ(FmtDbl(1.0), "1.0000");
  REQUIRE_EQ(FmtDbl(-90.0), "-90.0000");
  REQUIRE_EQ(FmtDbl(180.0), "180.0000");
}

TEST(FmtDbl_decimal_separator_is_period) {
  // Must always use '.' even if system locale uses ','
  std::string s = FmtDbl(3.14159);
  REQUIRE(s.find('.') != std::string::npos);
  REQUIRE(s.find(',') == std::string::npos);
}

TEST(FmtDbl_four_decimal_places) {
  REQUIRE_EQ(FmtDbl(1.23456789), "1.2346");  // rounds
  REQUIRE_EQ(FmtDbl(-0.0001), "-0.0001");
  REQUIRE_EQ(FmtDbl(0.00001), "0.0000");  // rounds to zero
}

// ---- ClampBbox — lat -------------------------------------------------------

TEST(ClampBbox_lat_normal) {
  BBox b = ClampBbox(30.0, 60.0, -10.0, 10.0);
  REQUIRE_NEAR(b.lat_min, 30.0, 1e-9);
  REQUIRE_NEAR(b.lat_max, 60.0, 1e-9);
}

TEST(ClampBbox_lat_clamps_below_minus90) {
  BBox b = ClampBbox(-95.0, 45.0, 0.0, 10.0);
  REQUIRE_NEAR(b.lat_min, -90.0, 1e-9);
}

TEST(ClampBbox_lat_clamps_above_90) {
  BBox b = ClampBbox(0.0, 95.0, 0.0, 10.0);
  REQUIRE_NEAR(b.lat_max, 90.0, 1e-9);
}

// ---- ClampBbox — lon normal ------------------------------------------------

TEST(ClampBbox_lon_normal) {
  BBox b = ClampBbox(0.0, 90.0, -10.0, 10.0);
  REQUIRE_NEAR(b.lon_min, -10.0, 1e-9);
  REQUIRE_NEAR(b.lon_max, 10.0, 1e-9);
}

// ---- ClampBbox — antimeridian wrap -----------------------------------------

TEST(ClampBbox_lon_wrap_left) {
  BBox b = ClampBbox(0.0, 90.0, -200.0, -140.0);
  REQUIRE_NEAR(b.lon_min, 160.0, 1e-9);
  REQUIRE_NEAR(b.lon_max, 180.0, 1e-9);
}

TEST(ClampBbox_lon_wrap_right_overflow) {
  BBox b = ClampBbox(0.0, 90.0, 150.0, 220.0);
  REQUIRE_NEAR(b.lon_min, 150.0, 1e-9);
  REQUIRE_NEAR(b.lon_max, 180.0, 1e-9);
}

TEST(ClampBbox_lon_full_world) {
  BBox b = ClampBbox(0.0, 90.0, -260.0, 100.0);  // span=360
  REQUIRE_NEAR(b.lon_min, -180.0, 1e-9);
  REQUIRE_NEAR(b.lon_max, 180.0, 1e-9);
}

TEST(ClampBbox_lon_full_world_exact360) {
  BBox b = ClampBbox(-90.0, 90.0, -180.0, 180.0);
  REQUIRE_NEAR(b.lon_min, -180.0, 1e-9);
  REQUIRE_NEAR(b.lon_max, 180.0, 1e-9);
}

TEST(ClampBbox_lon_very_negative) {
  BBox b = ClampBbox(0.0, 90.0, -540.0, -480.0);
  REQUIRE_NEAR(b.lon_min, -180.0, 1e-9);
  REQUIRE_NEAR(b.lon_max, -120.0, 1e-9);
}

int main(int argc, char **argv) { return run_tests(argc, argv); }
