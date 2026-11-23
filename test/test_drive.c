#include <stdlib.h>

#include "unity.h"
#include "drive.h"

void setUp(void)
{
    drive_reset();
}

void tearDown(void)
{
    if (Unity.CurrentTestFailed || getenv("DRIVE_LOG")) {
        drive_print(Unity.CurrentTestName);
    }
}

static void test_speeding_straight_at_camera(void)
{
    camera(0, 0, 50);
    start(0, -1000);
    drive_to(0, 500, 80);

    TEST_ASSERT_EQUAL_INT(1, announcements());
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(400, first_announcement_m());
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(450, first_announcement_m());
    TEST_ASSERT_GREATER_THAN_INT(0, speeding_warnings());
    TEST_ASSERT_EQUAL_INT(warnings(), speeding_warnings());
    TEST_ASSERT_EQUAL_INT(0, warnings_after_passing());
}

static void test_under_limit_one_beep(void)
{
    camera(0, 0, 90);
    start(0, -1000);
    drive_to(0, 500, 80);

    TEST_ASSERT_EQUAL_INT(1, announcements());
    TEST_ASSERT_GREATER_THAN_INT(0, warnings());
    TEST_ASSERT_EQUAL_INT(0, speeding_warnings());
}

static void test_slow_driving_announces_at_350_m(void)
{
    camera(0, 0, 50);
    start(0, -1000);
    drive_to(0, 500, 30);

    TEST_ASSERT_EQUAL_INT(1, announcements());
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(330, first_announcement_m());
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(350, first_announcement_m());
}

static void test_fast_driving_announces_earlier(void)
{
    camera(0, 0, 100);
    start(0, -2000);
    drive_to(0, 500, 130);

    TEST_ASSERT_EQUAL_INT(1, announcements());
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(680, first_announcement_m());
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(725, first_announcement_m());
}

static void test_parallel_road_200_m_away_announces(void)
{
    camera(0, 0, 50);
    start(200, -1000);
    drive_to(200, 500, 80);

    TEST_ASSERT_EQUAL_INT(1, announcements());
}

static void test_turning_away_stops_beeping(void)
{
    camera(0, 0, 50);
    start(0, -1000);
    drive_to(0, -300, 80);
    drive_to(1000, -300, 80);

    TEST_ASSERT_EQUAL_INT(1, announcements());
    TEST_ASSERT_EQUAL_INT(0, warnings_after_passing());
}

static void test_tunnel_announces_again(void)
{
    camera(0, 0, 50);
    start(0, -1000);
    drive_to(0, -300, 80);
    gps_off();
    drive_to(0, -150, 80);
    gps_on();
    drive_to(0, 500, 80);

    TEST_ASSERT_EQUAL_INT(2, announcements());
}

static void test_standing_at_radius_edge_announces_once(void)
{
    camera(0, 0, 50);
    gps_noise(10);
    start(0, -1000);
    drive_to(0, -345, 50);
    stop_for(60);
    drive_to(0, 500, 50);

    TEST_ASSERT_EQUAL_INT(1, announcements());
}

static void test_two_cameras_in_a_row(void)
{
    camera(0, 0, 50);
    camera(0, 1000, 70);
    start(0, -1000);
    drive_to(0, 2000, 80);

    TEST_ASSERT_EQUAL_INT(2, announcements());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_speeding_straight_at_camera);
    RUN_TEST(test_under_limit_one_beep);
    RUN_TEST(test_slow_driving_announces_at_350_m);
    RUN_TEST(test_fast_driving_announces_earlier);
    RUN_TEST(test_parallel_road_200_m_away_announces);
    RUN_TEST(test_turning_away_stops_beeping);
    RUN_TEST(test_tunnel_announces_again);
    RUN_TEST(test_standing_at_radius_edge_announces_once);
    RUN_TEST(test_two_cameras_in_a_row);
    return UNITY_END();
}
