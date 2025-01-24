#include <stdbool.h>

#include <hardware/gpio.h>

void leds_init(void)
{
    gpio_init(LEDS_SYS_RED_LED_PIN);
    gpio_set_dir(LEDS_SYS_RED_LED_PIN, GPIO_OUT);
    gpio_put(LEDS_SYS_RED_LED_PIN, false);

    gpio_init(LEDS_SYS_GREEN_LED_PIN);
    gpio_set_dir(LEDS_SYS_GREEN_LED_PIN, GPIO_OUT);
    gpio_put(LEDS_SYS_GREEN_LED_PIN, true);

    gpio_init(LEDS_GNSS_FIX_RED_LED_PIN);
    gpio_set_dir(LEDS_GNSS_FIX_RED_LED_PIN, GPIO_OUT);
    gpio_put(LEDS_GNSS_FIX_RED_LED_PIN, false);

    gpio_init(LEDS_GNSS_FIX_GREEN_LED_PIN);
    gpio_set_dir(LEDS_GNSS_FIX_GREEN_LED_PIN, GPIO_OUT);
    gpio_put(LEDS_GNSS_FIX_GREEN_LED_PIN, true);

    gpio_init(LEDS_CAM_DET_LED_PIN);
    gpio_set_dir(LEDS_CAM_DET_LED_PIN, GPIO_OUT);
    gpio_put(LEDS_CAM_DET_LED_PIN, true);
}

void leds_set_system_red_led(bool on)
{
    gpio_put(LEDS_SYS_RED_LED_PIN, !on);
}

void leds_set_system_green_led(bool on)
{
    gpio_put(LEDS_SYS_GREEN_LED_PIN, !on);
}

void leds_set_camera_detected_led(bool on)
{
    gpio_put(LEDS_CAM_DET_LED_PIN, !on);
}

void leds_set_gnss_fix_leds_state(bool fix)
{
    gpio_put(LEDS_GNSS_FIX_RED_LED_PIN, fix);
    gpio_put(LEDS_GNSS_FIX_GREEN_LED_PIN, !fix);
}
