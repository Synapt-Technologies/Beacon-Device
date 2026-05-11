// SPDX-FileCopyrightText: 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

// @file board_config.h
// @brief Helper to load menuconfig settings into Hub75Config

#pragma once

#include "hub75.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <cstdio>

static const char *const TAG_CONFIG = "board_config";

// Load configuration from menuconfig (CONFIG_* defines)
static inline Hub75Config getMenuConfigSettings() {
  Hub75Config config = {};

  // Panel dimensions
  config.panel_width = CONFIG_HUB75_PANEL_WIDTH;
  config.panel_height = CONFIG_HUB75_PANEL_HEIGHT;

  // Scan wiring (scan rate is determined automatically from panel_height)
  config.scan_wiring = Hub75ScanWiring::STANDARD_TWO_SCAN;
  // Shift driver
  config.shift_driver = Hub75ShiftDriver::GENERIC;

  // Huidu HD-WF2
  config.pins.r1 = 2;
  config.pins.g1 = 6;
  config.pins.b1 = 10;
  config.pins.r2 = 3;
  config.pins.g2 = 7;
  config.pins.b2 = 11;
  config.pins.a = 39;
  config.pins.b = 38;
  config.pins.c = 37;
  config.pins.d = 36;
  config.pins.e = 21;
  config.pins.lat = 33;
  config.pins.oe = 35;
  config.pins.clk = 34;


  // Multi-panel layout
  config.layout_rows = 1;
  config.layout_cols = 1;
  config.layout = Hub75PanelLayout::HORIZONTAL;

  config.rotation = Hub75Rotation::ROTATE_0;

  config.output_clock_speed = Hub75ClockSpeed::HZ_10M;

  // Performance settings
  config.min_refresh_rate = CONFIG_HUB75_MIN_REFRESH_RATE;
  config.brightness = CONFIG_HUB75_BRIGHTNESS;

  // Timing settings
  config.latch_blanking = CONFIG_HUB75_LATCH_BLANKING;

  config.double_buffer = false;

  config.clk_phase_inverted = false;

  // Note: Gamma correction is handled at compile-time via Kconfig (HUB75_GAMMA_MODE)
  // and applied in color_lut.cpp during LUT generation. No runtime config needed.

  return config;
}

// Helper: Print pin configuration (for debugging)
static inline void printPinConfig(const Hub75Pins &pins) {
  printf("HUB75 Pin Configuration:\n");
  printf("  Data (Upper): R1=%d, G1=%d, B1=%d\n", pins.r1, pins.g1, pins.b1);
  printf("  Data (Lower): R2=%d, G2=%d, B2=%d\n", pins.r2, pins.g2, pins.b2);
  printf("  Address: A=%d, B=%d, C=%d, D=%d, E=%d\n", pins.a, pins.b, pins.c, pins.d, pins.e);
  printf("  Control: LAT=%d, OE=%d, CLK=%d\n", pins.lat, pins.oe, pins.clk);
}