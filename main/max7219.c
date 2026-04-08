#include "max7219.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h" // for esp_random()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAX7219";

/* Background effect task handle (only one effect at a time) */
static TaskHandle_t s_effect_task = NULL;

/* ---- MAX7219 register addresses ---- */
#define REG_NOOP 0x00
#define REG_DIGIT0 0x01
#define REG_DECODE_MODE 0x09
#define REG_INTENSITY 0x0A
#define REG_SCAN_LIMIT 0x0B
#define REG_SHUTDOWN 0x0C
#define REG_TEST 0x0F

/* ---- Low-level SPI bit-bang ---- */

static void spi_send_byte(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    gpio_set_level(MAX7219_DIN_PIN, (byte >> i) & 1);
    gpio_set_level(MAX7219_CLK_PIN, 1);
    gpio_set_level(MAX7219_CLK_PIN, 0);
  }
}

static void max7219_write(uint8_t reg, uint8_t data) {
  gpio_set_level(MAX7219_CS_PIN, 0);
  spi_send_byte(reg);
  spi_send_byte(data);
  gpio_set_level(MAX7219_CS_PIN, 1);
}

/* ---- Public API ---- */

void max7219_display(const uint8_t fb[8]) {
  for (int row = 0; row < 8; row++) {
    max7219_write(REG_DIGIT0 + row, fb[row]);
  }
}

void max7219_clear(void) {
  uint8_t blank[8] = {0};
  max7219_display(blank);
}

void max7219_set_brightness(uint8_t level) {
  if (level > 15) level = 15;
  max7219_write(REG_INTENSITY, level);
}

/* ---- Effect helpers ---- */

static void show_fb(const uint8_t fb[8], int delay_ms) {
  max7219_display(fb);
  vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/* --- Effect 1: Expanding / contracting rings from center --- */
static void effect_rings(void) {
  static const uint8_t rings[5][8] = {
      {0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00}, /* 2×2 center dot */
      {0x00, 0x00, 0x3C, 0x24, 0x24, 0x3C, 0x00, 0x00}, /* 4×4 ring */
      {0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00}, /* 6×6 ring */
      {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}, /* 8×8 ring */
      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* full fill */
  };
  while (1) {
    for (int i = 0; i < 5; i++)
      show_fb(rings[i], 80);
    for (int i = 3; i >= 0; i--)
      show_fb(rings[i], 80);
  }
}

/* --- Effect 2: Rain — random droplets falling per column --- */
static void effect_rain(void) {
  uint32_t rng = 0xDEADBEEF;
  uint8_t pos[8] = {0, 3, 6, 1, 5, 2, 7, 4}; /* staggered start rows */
  uint8_t active[8] = {1, 0, 1, 1, 0, 1, 0, 1};
  uint8_t speed[8] = {1, 1, 1, 2, 1, 1, 2, 1};
  while (1) {
    uint8_t fb[8] = {0};
    for (int c = 0; c < 8; c++) {
      if (!active[c]) {
        rng = rng * 1664525u + 1013904223u;
        if ((rng >> 27) == 0) { /* ~3 % chance per tick to spawn */
          active[c] = 1;
          pos[c] = 0;
          speed[c] = 1 + ((rng >> 24) & 1);
        }
        continue;
      }
      int head = pos[c];
      fb[head] |= (0x80u >> c);                   /* head pixel  */
      if (head > 0) fb[head - 1] |= (0x80u >> c); /* tail pixel  */
      pos[c] = (uint8_t)(head + speed[c]);
      if (pos[c] >= 8) {
        pos[c] = 0;
        rng = rng * 1664525u + 1013904223u;
        active[c] = ((rng >> 25) & 3) != 0; /* 75 % chance to stay active */
        speed[c] = 1 + ((rng >> 23) & 1);
      }
    }
    show_fb(fb, 60);
  }
}

/* --- Effect 3: Checkerboard — hold → rapid flash transition --- */
static void effect_checkerboard(void) {
  static const uint8_t chkA[8] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
  static const uint8_t chkB[8] = {0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA};
  while (1) {
    show_fb(chkA, 600);
    for (int i = 0; i < 5; i++) {
      show_fb(chkB, 60);
      show_fb(chkA, 60);
    }
    show_fb(chkB, 600);
    for (int i = 0; i < 5; i++) {
      show_fb(chkA, 60);
      show_fb(chkB, 60);
    }
  }
}

/* --- Effect 4: Wipe — fill/erase in 4 directions cycling --- */
static void effect_wipe(void) {
  while (1) {
    uint8_t fb[8] = {0};
    /* left → right fill / erase */
    for (int c = 0; c < 8; c++) {
      for (int r = 0; r < 8; r++)
        fb[r] |= (0x80u >> c);
      show_fb(fb, 50);
    }
    for (int c = 0; c < 8; c++) {
      for (int r = 0; r < 8; r++)
        fb[r] &= ~(0x80u >> c);
      show_fb(fb, 50);
    }
    /* top → bottom fill / erase */
    for (int r = 0; r < 8; r++) {
      fb[r] = 0xFF;
      show_fb(fb, 50);
    }
    for (int r = 0; r < 8; r++) {
      fb[r] = 0x00;
      show_fb(fb, 50);
    }
    /* right → left fill / erase */
    for (int c = 7; c >= 0; c--) {
      for (int r = 0; r < 8; r++)
        fb[r] |= (0x80u >> c);
      show_fb(fb, 50);
    }
    for (int c = 7; c >= 0; c--) {
      for (int r = 0; r < 8; r++)
        fb[r] &= ~(0x80u >> c);
      show_fb(fb, 50);
    }
    /* bottom → top fill / erase */
    for (int r = 7; r >= 0; r--) {
      fb[r] = 0xFF;
      show_fb(fb, 50);
    }
    for (int r = 7; r >= 0; r--) {
      fb[r] = 0x00;
      show_fb(fb, 50);
    }
  }
}

/* --- Effect 5: Spiraling inward fill --- */
static void effect_spiral(void) {
  /* Pre-computed spiral order for an 8x8 grid (row, col) */
  static const uint8_t spiral[64][2] = {
      {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}, {6, 7}, {7, 7}, {7, 6}, {7, 5}, {7, 4}, {7, 3}, {7, 2}, {7, 1}, {7, 0}, {6, 0}, {5, 0}, {4, 0}, {3, 0}, {2, 0}, {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4},
      {1, 5}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6}, {6, 6}, {6, 5}, {6, 4}, {6, 3}, {6, 2}, {6, 1}, {5, 1}, {4, 1}, {3, 1}, {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {3, 5}, {4, 5}, {5, 5}, {5, 4}, {5, 3}, {5, 2}, {4, 2}, {3, 2}, {3, 3}, {3, 4}, {4, 4}, {4, 3},
  };
  uint8_t fb[8] = {0};
  while (1) {
    for (int i = 0; i < 64; i++) {
      fb[spiral[i][0]] |= (0x80 >> spiral[i][1]);
      show_fb(fb, 25);
    }
    for (int i = 63; i >= 0; i--) {
      fb[spiral[i][0]] &= ~(0x80 >> spiral[i][1]);
      show_fb(fb, 25);
    }
  }
}

/* --- Effect 6: Fireworks — rocket launches, explodes in expanding rings, fades --- */
static void effect_fireworks(void) {
  uint32_t rng = 0xCAFEBABE;
  while (1) {
    /* Pick a random burst centre, keeping all rings visible on-screen */
    rng = rng * 1664525u + 1013904223u;
    int bc = 2 + ((rng >> 16) & 3); /* col 2-5 */
    rng = rng * 1664525u + 1013904223u;
    int br = 1 + ((rng >> 18) & 3); /* row 1-4 */

    /* Phase 1: rocket rising from the bottom row */
    for (int r = 7; r >= br; r--) {
      uint8_t fb[8] = {0};
      fb[r] |= (0x80u >> bc);
      if (r < 7) fb[r + 1] |= (0x80u >> bc); /* short trailing pixel */
      show_fb(fb, 40);
    }

    /* Phase 2: bright flash at burst point */
    {
      uint8_t fb[8] = {0};
      fb[br] |= (0x80u >> bc);
      max7219_write(REG_INTENSITY, 15);
      show_fb(fb, 40);
    }

    /* Phase 3: four expanding rings (radius 1-4) */
    for (int rad = 1; rad <= 4; rad++) {
      uint8_t fb[8] = {0};
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
          int dr = r - br, dc = c - bc;
          int d2 = dr * dr + dc * dc;
          /* ring band: pixels whose distance^2 is within ±rad of rad^2 */
          if (d2 >= rad * rad - rad && d2 <= rad * rad + rad) fb[r] |= (0x80u >> c);
        }
      }
      show_fb(fb, 70);
    }

    /* Phase 4: hold largest ring then dim it out */
    {
      uint8_t fb[8] = {0};
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
          int dr = r - br, dc = c - bc;
          int d2 = dr * dr + dc * dc;
          if (d2 >= 12 && d2 <= 20) fb[r] |= (0x80u >> c);
        }
      }
      static const uint8_t fade_steps[] = {10, 6, 3, 1};
      for (int i = 0; i < 4; i++) {
        max7219_write(REG_INTENSITY, fade_steps[i]);
        show_fb(fb, 55);
      }
    }
    max7219_write(REG_INTENSITY, 8); /* restore brightness */
    max7219_clear();

    /* Random pause between launches (200-455 ms) */
    rng = rng * 1664525u + 1013904223u;
    vTaskDelay(pdMS_TO_TICKS(200 + ((rng >> 16) & 255)));
  }
}

/* --- Effect 7: Heartbeat — heart bitmap with brightness pulse --- */
static void effect_heartbeat(void) {
  /* Heart shape on 8×8 */
  static const uint8_t heart[8] = {
      0x00,
      0x66,
      0xFF,
      0xFF,
      0x7E,
      0x3C,
      0x18,
      0x00,
  };
  static const uint8_t ramp[] = {1, 2, 4, 7, 11, 15, 11, 7, 4, 2};
  static const uint8_t blank[8] = {0};
  while (1) {
    /* lub — first beat */
    for (int i = 0; i < (int)sizeof(ramp); i++) {
      max7219_write(REG_INTENSITY, ramp[i]);
      show_fb(heart, 28);
    }
    /* dub — second beat (faster, slightly quieter) */
    max7219_write(REG_INTENSITY, 2);
    show_fb(heart, 80);
    for (int i = 0; i < (int)sizeof(ramp); i++) {
      max7219_write(REG_INTENSITY, ramp[i] > 10 ? ramp[i] : ramp[i] / 2 + 1);
      show_fb(heart, 22);
    }
    /* rest */
    max7219_write(REG_INTENSITY, 0);
    show_fb(blank, 550);
    max7219_write(REG_INTENSITY, 8); /* restore medium brightness */
  }
}

/* --- Effect 8: Rotating line — 8-frame 45° steps (radar sweep) --- */
static void effect_cross_rotate(void) {
  /* 8 frames: a 2-pixel-wide line rotated from vertical to 157.5° in 22.5° steps */
  static const uint8_t frames[8][8] = {
      /* 0°   vertical */ {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
      /* 22.5° */ {0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06},
      /* 45°  NW→SE diagonal */ {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01},
      /* 67.5° */ {0x00, 0x00, 0xC0, 0xF0, 0x1F, 0x03, 0x00, 0x00},
      /* 90°  horizontal */ {0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00},
      /* 112.5° */ {0x00, 0x00, 0x03, 0x0F, 0xF8, 0xC0, 0x00, 0x00},
      /* 135° NE→SW diagonal */ {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80},
      /* 157.5° */ {0x06, 0x06, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60},
  };
  while (1) {
    for (int i = 0; i < 8; i++)
      show_fb(frames[i], 100);
  }
}

/* --- Effect 9: Sparkle — twinkling stars with pulsing density --- */
static void effect_sparkle(void) {
  uint32_t rng = 0xDEADBEEF;
  int step = 0;
  while (1) {
    uint8_t fb[8] = {0};
    /* density oscillates 4..20 pixels over a 64-step cycle */
    int phase = step & 63;
    int n = 4 + (phase < 32 ? phase / 2 : (63 - phase) / 2);
    for (int i = 0; i < n; i++) {
      rng = rng * 1664525u + 1013904223u;
      fb[(rng >> 24) & 7] |= (0x80u >> ((rng >> 16) & 7));
    }
    show_fb(fb, 50);
    step++;
  }
}

/* --- Effect 10: Vertical scanner (KITT-style) with 2-row echo trail --- */
static void effect_scanner(void) {
  while (1) {
    for (int r = 0; r < 8; r++) {
      uint8_t fb[8] = {0};
      fb[r] = 0xFF;
      if (r > 0) fb[r - 1] = 0xFF; /* trailing echo */
      show_fb(fb, 55);
    }
    for (int r = 6; r >= 1; r--) {
      uint8_t fb[8] = {0};
      fb[r] = 0xFF;
      if (r < 7) fb[r + 1] = 0xFF; /* trailing echo */
      show_fb(fb, 55);
    }
  }
}

/* --- Effect 12: Wave — sinusoidal bars scrolling continuously --- */
static void effect_wave(void) {
  /* heights[n]: number of bottom rows lit at phase position n */
  static const uint8_t heights[16] = {4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5};
  int offset = 0;
  while (1) {
    uint8_t fb[8] = {0};
    for (int c = 0; c < 8; c++) {
      int h = heights[(c + offset) & 15];
      for (int r = 8 - h; r < 8; r++)
        fb[r] |= (0x80u >> c);
    }
    show_fb(fb, 60);
    offset = (offset + 1) & 15;
  }
}

/* --- Effect 13: Snake — continuous boustrophedon loop --- */
static void effect_snake(void) {
  uint8_t path_r[64], path_c[64];
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      int idx = r * 8 + c;
      path_r[idx] = (uint8_t)r;
      path_c[idx] = (uint8_t)((r % 2 == 0) ? c : 7 - c);
    }
  }
  const int snake_len = 6;
  int head = 0;
  while (1) {
    uint8_t fb[8] = {0};
    for (int s = 0; s < snake_len; s++) {
      int pos = (head - s + 64) % 64;
      fb[path_r[pos]] |= (0x80u >> path_c[pos]);
    }
    show_fb(fb, 45);
    head = (head + 1) % 64;
  }
}

/* --- Effect 14: Starburst radiating from center, repeating --- */
static void effect_starburst(void) {
  static const uint8_t bursts[7][8] = {
      {0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00}, /* center dot */
      {0x18, 0x18, 0x18, 0xFF, 0xFF, 0x18, 0x18, 0x18}, /* + cross */
      {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81}, /* × diagonals */
      {0x99, 0x5A, 0x3C, 0xFF, 0xFF, 0x3C, 0x5A, 0x99}, /* combined */
      {0xBD, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0xBD}, /* nearly full */
      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, /* full flash */
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* blank */
  };
  while (1) {
    for (int i = 0; i < 7; i++)
      show_fb(bursts[i], 75);
    for (int i = 5; i >= 0; i--)
      show_fb(bursts[i], 75);
    vTaskDelay(pdMS_TO_TICKS(180)); /* brief pause before next burst */
  }
}

/* --- Effect 15: Random letters A-Z / a-z ---
 * Each character is rendered as an 8x8 bitmap using a 5x7 font scaled to fit.
 * 52 glyphs total (A-Z then a-z). Each glyph is 8 bytes (one per row). */

/* 5-wide glyphs packed into 8 columns: bits 7..3 = glyph, bits 2..0 = unused (zero-padded right). */
static const uint8_t font_letters[26][8] = {
    /* A */ {0x30, 0x78, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0x00},
    /* B */ {0xF8, 0xCC, 0xCC, 0xF8, 0xCC, 0xCC, 0xF8, 0x00},
    /* C */ {0x78, 0xCC, 0xC0, 0xC0, 0xC0, 0xCC, 0x78, 0x00},
    /* D */ {0xF0, 0xD8, 0xCC, 0xCC, 0xCC, 0xD8, 0xF0, 0x00},
    /* E */ {0xFC, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xFC, 0x00},
    /* F */ {0xFC, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xC0, 0x00},
    /* G */ {0x78, 0xCC, 0xC0, 0xDC, 0xCC, 0xCC, 0x78, 0x00},
    /* H */ {0xCC, 0xCC, 0xCC, 0xFC, 0xCC, 0xCC, 0xCC, 0x00},
    /* I */ {0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},
    /* J */ {0x1C, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00},
    /* K */ {0xCC, 0xD8, 0xF0, 0xE0, 0xF0, 0xD8, 0xCC, 0x00},
    /* L */ {0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFC, 0x00},
    /* M */ {0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00},
    /* N */ {0xCC, 0xEC, 0xFC, 0xDC, 0xCC, 0xCC, 0xCC, 0x00},
    /* O */ {0x78, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x78, 0x00},
    /* P */ {0xF8, 0xCC, 0xCC, 0xF8, 0xC0, 0xC0, 0xC0, 0x00},
    /* Q */ {0x78, 0xCC, 0xCC, 0xCC, 0xDC, 0x78, 0x1C, 0x00},
    /* R */ {0xF8, 0xCC, 0xCC, 0xF8, 0xD8, 0xCC, 0xCC, 0x00},
    /* S */ {0x78, 0xCC, 0xC0, 0x78, 0x0C, 0xCC, 0x78, 0x00},
    /* T */ {0xFC, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00},
    /* U */ {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x78, 0x00},
    /* V */ {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x78, 0x30, 0x00},
    /* W */ {0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00},
    /* X */ {0xCC, 0xCC, 0x78, 0x30, 0x78, 0xCC, 0xCC, 0x00},
    /* Y */ {0xCC, 0xCC, 0xCC, 0x78, 0x30, 0x30, 0x30, 0x00},
    /* Z */ {0xFC, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xFC, 0x00},
};

static void effect_random_letters(void) {
  uint32_t rng = 0xABCD1234;
  while (1) {
    /* code */
    rng = rng * 1664525u + 1013904223u;
    uint8_t idx = (rng >> 16) % 26;
    /* Show letter */
    show_fb(font_letters[idx], 400);
    /* Brief blank between letters */
    max7219_clear();
    vTaskDelay(pdMS_TO_TICKS(80));
  }
}

typedef void (*effect_func_t)(void);
static const effect_func_t s_effect_list[] = {
    effect_rings,          // 1
    effect_rain,           // 2
    effect_checkerboard,   // 3
    effect_wipe,           // 4
    effect_spiral,         // 5
    effect_fireworks,      // 6
    effect_heartbeat,      // 7
    effect_cross_rotate,   // 8
    effect_sparkle,        // 9
    effect_scanner,        // 10
    effect_random_letters, // 11
    effect_wave,           // 12
    effect_snake,          // 13
    effect_starburst       // 14
};

static void max7219_effect_task(void *arg) {
  int n = (int)(intptr_t)arg;
  if (n < 0) n = -n;
  int max_effect = sizeof(s_effect_list) / sizeof(s_effect_list[0]);
  n = n % max_effect;
  effect_func_t effect = s_effect_list[n];
  effect();

  /* clear task handle then delete self */
  s_effect_task = NULL;
  vTaskDelete(NULL);
}

/*
 * Public: run a single effect by number.
 * Note: effects are blocking and include their own delays; call from a task.
 */
void max7219_show_effect(int n) {
  if (n < 0) n = -n;
  max7219_set_brightness(15); /* ensure medium brightness for effects */
  /* If an effect is already running, stop it */
  if (s_effect_task) {
    vTaskDelete(s_effect_task);
    s_effect_task = NULL;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  /* Spawn background task to run the effect */
  BaseType_t ok = xTaskCreate(max7219_effect_task, "max7219_effect", 4096, (void *)(intptr_t)n, tskIDLE_PRIORITY + 2, &s_effect_task);
  if (ok != pdPASS) {
    ESP_LOGW(TAG, "Failed to create effect task (n=%d)", n);
    s_effect_task = NULL;
  }
}

void max7219_init(void) {
  ESP_LOGI(TAG, "Initializing MAX7219 (CS=%d, CLK=%d, DIN=%d)", MAX7219_CS_PIN, MAX7219_CLK_PIN, MAX7219_DIN_PIN);

  /* Configure GPIO pins */
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << MAX7219_CS_PIN) | (1ULL << MAX7219_CLK_PIN) | (1ULL << MAX7219_DIN_PIN),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);

  gpio_set_level(MAX7219_CS_PIN, 1);
  gpio_set_level(MAX7219_CLK_PIN, 0);

  /* MAX7219 initialization sequence */
  max7219_write(REG_TEST, 0x00);        /* Normal operation (not test) */
  max7219_write(REG_SCAN_LIMIT, 0x07);  /* Display digits 0-7 */
  max7219_write(REG_DECODE_MODE, 0x00); /* No BCD decode — raw segments */
  max7219_write(REG_INTENSITY, 0x08);   /* Medium brightness */
  max7219_write(REG_SHUTDOWN, 0x01);    /* Normal operation (not shutdown) */

  max7219_clear();
  // start a random effect by default
  max7219_show_effect(esp_random());

  ESP_LOGI(TAG, "MAX7219 initialized, effects running");
}
