// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// This header may be included by other board headers as "boards/custom_catchcam.h"

// pico_cmake_set PICO_PLATFORM=rp2040

#ifndef _BOARDS_CUSTOM_CATCHCAM_H
#define _BOARDS_CUSTOM_CATCHCAM_H

// For board detection
#define CUSTOM_CATCHCAM

// TODO: add spi, gpios etc.

// --- LEDs ---
// Leds have inverted logic
#define LEDS_GNSS_FIX_GREEN_LED_PIN 12
#define LEDS_GNSS_FIX_RED_LED_PIN   13

#define LEDS_CAM_DET_LED_PIN        14

#define LEDS_SYS_GREEN_LED_PIN      18
#define LEDS_SYS_RED_LED_PIN        19

// --- GPIO ---
// Pins have inverted logic
#define GNSS_RESET_PIN 20
#define AUDIO_MUTE_PIN 26

// --- UART NMEA/PMTK ---
#define UART_NMEA_PMTK_ID     uart0
#define UART_NMEA_PMTK_TX_PIN 16
#define UART_NMEA_PMTK_RX_PIN 17

// --- UART ---
#define UART_GNSS_RTCM_ID     uart1
#define UART_GNSS_RTCM_TX_PIN 24
#define UART_GNSS_RTCM_RX_PIN 25

// --- I2S ---
#define I2S_DATA_PIN      27
#define I2S_CLK_PIN_BASE  28
#define I2S_WS_PIN        29
#if I2S_WS_PIN != (I2S_CLK_PIN_BASE + 1)
    #error "I2S_WS_PIN must be (I2S_CLK_PIN_BASE + 1)"
#endif

// --- FLASH ---

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 4
#endif

// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (16 * 1024 * 1024)
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 1
#endif

#endif
