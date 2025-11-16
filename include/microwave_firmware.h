/*
 * ПОВНА ПРОШИВКА МІКРОХВИЛЬОВКИ (v_final_2.9.38 - Мелодія 1x3 та 3x3)
 *
 * --- ОПИС ФУНКЦІОНАЛУ v2.9.38 ---
 * 1. (v2.9.38) ДОДАНО: Секвенсер для мелодії 1x3 (3 біпи) для 1-го етапу.
 * 2. (v2.9.38) ЗМІНА: update_cook_timer тепер запускає 1x3 (Етап 1) або 3x3 (Етап 2).
 * 3. (v2.9.38) ВИПРАВЛЕНО: Двокрапка тепер ВИМКНЕНА під час "End1", "End2" (для консистенції).
 * 4. (v2.9.35) ОПТИМІЗАЦІЯ: Вся 1ms логіка (кнопки, дисплей) винесена з ISR в loop().
 * 5. (v2.9.32) ЗМІНА: Timer1 працює на 2000Hz (500µs) для 1kHz тону.
 * 6. (v2.9.2) ВИДАЛЕНО: Повністю видалено STATE_SLEEPING.
 * 7. (v2.9.0) ВИПРАВЛЕНО: Інвертована логіка дверей (5В=Закрито, 0В=Відкрито).
 */

#ifndef MICROWAVE_FIRMWARE_H_
#define MICROWAVE_FIRMWARE_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <string.h> 
#include <stdbool.h> 

// ============================================================================
// --- 🔴 ГОЛОВНА КОНФІГУРАЦІЯ ---
// ============================================================================
#define ZVS_MODE 0

#define ENABLE_KEYPAD 1 

#if (ZVS_MODE != 0)
// 🔽🔽🔽 КОНСТАНТИ ДЛЯ КВАЛІФІКАЦІЇ ZVS (v2.9.8 - Спрощено) 🔽🔽🔽
#define ZVS_QUALIFICATION_COUNT 3
#define ZVS_QUAL_TIMEOUT_MS 200 
// (v2.9.8) Видалено MIN/MAX_INTERVAL_MS
#endif


// ============================================================================
// --- 🟡 ТИПИ ДАНИХ (ENUMS & STRUCTS) ---
// ============================================================================

typedef enum {
    COLON_OFF = 0, COLON_ON = 1, COLON_BLINK_SLOW = 2, COLON_BLINK_FAST = 3, COLON_BLINK_SUPERFAST=4
} ColonDisplayMode;

typedef enum {
    STATE_IDLE, /* STATE_SLEEPING, */ STATE_SET_CLOCK_MODE, STATE_SET_CLOCK_TIME, // (v2.9.2) Видалено STATE_SLEEPING
    STATE_CLOCK_SAVED, STATE_SET_TIME, STATE_SET_POWER, STATE_TWO_STAGE_1,
    STATE_TWO_STAGE_2, STATE_SET_AUTO_COOK, STATE_SET_AUTO_DEFROST,
    STATE_SET_WEIGHT, STATE_QUICK_START_PREP, STATE_COOKING, STATE_PAUSED,
    STATE_FLIP_PAUSE, STATE_FINISHED, STATE_POST_COOK, 
    STATE_LOCKED,
    STATE_STAGE2_TRANSITION,
    #if (ZVS_MODE != 0)
    STATE_ZVS_QUALIFICATION // (v2.9.0) Стан очікування ZVS
    #endif
} AppState_t;

typedef enum { PROGRAM_NONE, PROGRAM_COOK, PROGRAM_DEFROST } AutoProgramType;

typedef struct { uint16_t weight_g; uint16_t time_sec; uint8_t power_level; bool add_beep; } AutoProgramEntry;
typedef struct { uint16_t weight_g; uint8_t num_flips; uint8_t flip_percentages[5]; } FlipSchedule;

typedef struct { 
    uint8_t num_flips_total; 
    uint16_t flip_times_sec[5]; 
    uint8_t next_flip_index; 
} DefrostFlipInfo_t; // (v2.3.1)


// ============================================================================
// --- 🔴 ВКЛЮЧЕННЯ МОДУЛІВ (v2.8.0) ---
// ============================================================================
#include "display_driver.h"
#include "keypad_driver.h"
#include "timers_isr.h"
#include "cooking_logic.h"
#include "auto_programs.h"


// ============================================================================
// --- 2. АПАРАТНІ ВИЗНАЧЕННЯ ---
// ============================================================================
#define ZVS_DDR DDRD
#define ZVS_PIN PIND
#define ZVS_BIT (1 << PD2)

#define CDD_DDR DDRD
#define CDD_PORT PORTD 
#define CDD_PIN PIND
#define CDD_BIT (1 << PD0) 

#define MAGNETRON_DDR DDRD
#define MAGNETRON_PORT PORTD
#define MAGNETRON_BIT (1 << PD4) 

#define FAN_DDR DDRD
#define FAN_PORT PORTD
#define FAN_BIT (1 << PD1) 

#define BEEPER_DDR DDRD
#define BEEPER_PORT PORTD
#define BEEPER_BIT (1 << PD5) 

// ============================================================================
// --- 4. ГЛОБАЛЬНІ ЗМІННІ (ОГОЛОШЕННЯ) ---
// ============================================================================

extern volatile AppState_t g_state;
extern volatile uint32_t g_millis_counter; 
extern volatile uint16_t g_timer_ms; 

// (v2.9.2) 16-бітний "знімок" часу
extern volatile uint16_t g_millis_16bit_snapshot;

extern volatile bool g_1sec_tick_flag;
extern volatile bool g_start_cooking_flag; 

// (v2.9.35) Прапор 1ms
extern volatile bool g_1ms_tick_flag;

extern volatile uint16_t g_beep_ms_counter;
extern volatile uint16_t g_beep_flip_sequence_timer;
extern volatile uint16_t g_clock_save_burst_timer;
extern volatile uint16_t g_flip_beep_timeout_ms;
extern volatile uint16_t g_key_3sec_hold_timer_ms;
extern volatile uint16_t g_key_continuous_hold_ms;
extern volatile uint16_t g_last_key_hold_duration;
extern volatile char g_last_key_for_hold;
extern volatile bool g_key_hold_3sec_flag;
extern volatile bool g_ignore_next_key_release; 
extern volatile uint16_t g_quick_start_delay_ms;
extern volatile uint32_t g_post_cook_timer_ms;
extern volatile uint8_t g_post_cook_sec_counter; 
extern volatile uint16_t g_clock_save_blink_ms;
extern volatile bool g_door_open_during_pause;
extern volatile bool g_magnetron_request;
extern volatile bool g_zvs_present;
extern volatile uint8_t g_zvs_pulse_counter;
extern volatile uint8_t g_zvs_watchdog_counter;
extern volatile AutoProgramType g_active_auto_program_type;
extern volatile uint16_t g_door_overlay_timer_ms;
extern volatile uint32_t g_magnetron_last_off_timestamp_ms;
extern volatile uint8_t g_pwm_cycle_duration;
extern volatile uint8_t g_pwm_cycle_counter_seconds;

#if (ZVS_MODE != 0)
// (v2.9.8) Спрощено
extern volatile uint8_t g_zvs_qualification_counter; 
// extern volatile uint16_t g_zvs_timestamps[ZVS_QUALIFICATION_COUNT]; // Видалено
extern volatile uint16_t g_zvs_qual_timeout_ms;
// extern volatile bool g_zvs_error_flag; // Видалено
#endif

// 🔽🔽🔽 (v2.9.38) ЗМІННІ ДЛЯ МЕЛОДІЇ 🔽🔽🔽
extern volatile uint16_t g_final_beep_sequencer_ms; // Для 3x3
extern volatile uint8_t g_final_beep_counter;
extern volatile uint16_t g_stage1_beep_sequencer_ms; // Для 1x3
extern volatile uint8_t g_stage1_beep_counter;
// 🔼🔼🔼 (v2.9.38) КІНЕЦЬ ЗМІН 🔼🔼🔼


extern volatile uint16_t g_cook_time_total_sec, g_cook_original_total_time;
extern volatile uint8_t g_cook_power_level;
extern uint16_t g_stage1_time_sec; 
extern volatile uint16_t g_stage2_time_sec;
extern volatile uint8_t g_stage2_power;
extern volatile bool g_was_two_stage_cook; 
extern volatile uint8_t g_auto_program;
extern volatile uint16_t g_auto_weight_grams;
extern volatile uint8_t g_input_min_tens, g_input_min_units, g_input_sec_tens;
extern volatile uint8_t g_input_hour, g_input_min, g_input_sec;
extern bool g_is_defrost_mode; 
extern volatile bool g_magnetron_is_on;
extern volatile uint8_t g_pwm_on_time_seconds;
extern volatile uint8_t g_clock_hour, g_clock_min, g_clock_sec;
extern volatile bool g_clock_24hr_mode;
extern volatile DefrostFlipInfo_t g_defrost_flip_info;

extern volatile uint8_t g_clock_save_beep_phaser; 

extern const uint16_t power_levels_watt[];
#define ADAPTIVE_PWM_THRESHOLD_SEC 30
#define MIN_SAFE_ON_TIME_SEC 5U
#define MIN_SAFE_OFF_TIME_SEC 2U 
#define MIN_SAFE_CYCLE_SEC (MIN_SAFE_ON_TIME_SEC + MIN_SAFE_OFF_TIME_SEC) 
#define MAGNETRON_COAST_TIME_SEC 10 

// ============================================================================
// --- 🟨 ПРОТОТИПИ ФУНКЦІЙ (ТІЛЬКИ ГОЛОВНИЙ МОДУЛЬ) ---
// ============================================================================

void reset_to_idle();
void handle_state_machine(char key, bool allow_beep);
void handle_time_input_odometer(char key);
void handle_clock_input(char key);

#endif // MICROWAVE_FIRMWARE_H_