#ifndef KEYPAD_DRIVER_H_
#define KEYPAD_DRIVER_H_

#include "microwave_firmware.h"

// ============================================================================
// --- 5. СЕКЦІЯ КНОПОК (ADC) ---
// ============================================================================
#define KEYPAD_ADC_CHANNEL 0
#define ADC_NOISE_THRESHOLD 1015
#define ADC_TOLERANCE 20
#define DEBOUNCE_TIME 1 

// Визначення клавіш
#define KEY_10_MIN '1'
#define KEY_1_MIN '4'
#define KEY_10_SEC '7'
#define KEY_AUTO_COOK '2'
#define KEY_AUTO_DEFROST '5'
#define KEY_QUICK_DEFROST '8'
#define KEY_MICRO '3' 
#define KEY_CLOCK '6'
#define KEY_MORE '9'
#define KEY_LESS 'A'
#define KEY_STOP_RESET 'B'
#define KEY_START_QUICKSTART 'C'

// Прототипи функцій
void keypad_init();
char get_key_press();
void handle_key_hold_increment(char key, uint16_t hold_duration, uint16_t* last_trigger_ms);
void keypad_timer_tick(void); // Функція, що викликається з ISR таймера

#endif // KEYPAD_DRIVER_H_