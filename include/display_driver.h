#ifndef DISPLAY_DRIVER_H_
#define DISPLAY_DRIVER_H_

#include "microwave_firmware.h"

// ============================================================================
// --- 3. СЕКЦІЯ ДИСПЛЕЯ ---
// ============================================================================
#define SEG_D (1 << PB0)
#define SEG_G (1 << PB1)
#define SEG_C (1 << PB2)
#define SEG_B (1 << PC1)
#define SEG_A (1 << PC2)
#define SEG_F (1 << PC4)
#define SEG_E (1 << PC5)
#define ALL_SEGMENTS_B (SEG_D | SEG_G | SEG_C)
#define ALL_SEGMENTS_C (SEG_A | SEG_B | SEG_F | SEG_E)
#define DIGIT1_CATHODE (1 << PD6)
#define DIGIT2_CATHODE (1 << PD7)
#define DIGIT3_CATHODE (1 << PB4)
#define DIGIT4_CATHODE (1 << PB3)
#define COLON_ANODE (1 << PC3)

// Символи
#define CHAR_SPACE 10
#define CHAR_L 11
#define CHAR_E 12
#define CHAR_D 13
#define CHAR_B 14
#define CHAR_F 15
#define CHAR_A 16
#define CHAR_C 17 
#define CHAR_H 18
#define CHAR_R 19
#define CHAR_O 20 
#define CHAR_N 21
#define CHAR_P 22
#define CHAR_DASH 23
#define CHAR_T 24

// --- 🔴 ВИДАЛЕНО ДУБЛЮЮЧЕ ВИЗНАЧЕННЯ ---
// typedef enum { ... } ColonDisplayMode;
// (Це визначення вже є у microwave_firmware.h)
// -------------------------------------

// Глобальні змінні, що використовуються ТІЛЬКИ цим модулем
// (оголошені extern тут, визначені у display_driver.c)
extern uint8_t g_display_buffer[4];
extern volatile uint8_t g_colon_visible;
extern volatile uint16_t g_colon_timer;
extern volatile ColonDisplayMode g_colon_mode;


// Прототипи функцій
void setup_display_pins();
void set_display(uint8_t vis1, uint8_t vis2, uint8_t vis3, uint8_t vis4);
void display_time_suppressed(uint16_t total_seconds);
void display_clock(uint8_t h, uint8_t m);
void update_display();
void set_colon_mode(ColonDisplayMode mode);
void update_colon_state();
void run_display_multiplex();
void disable_all_digits();

#endif // DISPLAY_DRIVER_H_