/*
 * ПОВНА ПРОШИВКА МІКРОХВИЛЬОВКИ (v_pre_final_2.3.1_types_fixed)
 * Модель: LG MS-2044J (на базі ATmega8, 16МГц)
 *
 * --- ОПИС ФУНКЦІОНАЛУ v2.3.1 (Fix Types) ---
 * 1.  ВИПРАВЛЕНО БАГ КОМПІЛЯЦІЇ:
 * - Усунено "conflicting types" для 'g_defrost_flip_info'
 * шляхом створення 'DefrostFlipInfo_t' у .h файлі
 * та його використання у .c файлі.
 *
 * --- (Решта функціоналу без змін з v2.3.0) ---
 */

// ============================================================================
// --- 🔴 ВКЛЮЧЕННЯ МОДУЛІВ ---
// ============================================================================
#include "microwave_firmware.h" // < Містить всі спільні визначення та extern
#include "display_driver.h"     // < Містить прототипи для дисплея
#include "keypad_driver.h"      // < Містить прототипи для кнопок

// ============================================================================
// --- 🥩 ТАБЛИЦІ (PROGMEM) ---
// ============================================================================
const FlipSchedule def_meat_flips[] PROGMEM = { {300, 1, {50,0,0,0,0}}, {500, 2, {33,67,0,0,0}}, {750, 2, {33,67,0,0,0}}, {1000, 3, {25,50,75,0,0}}, {1500, 3, {25,50,75,0,0}}, {2000, 4, {20,40,60,80,0}}, {4000, 5, {20,40,60,80,99}} };
const FlipSchedule def_poultry_flips[] PROGMEM = { {300, 1, {50,0,0,0,0}}, {500, 2, {33,67,0,0,0}}, {750, 2, {33,67,0,0,0}}, {1000, 3, {25,50,75,0,0}}, {1500, 3, {25,50,75,0,0}}, {2000, 4, {20,40,60,80,0}}, {4000, 5, {20,40,60,80,99}} };
const FlipSchedule def_fish_flips[] PROGMEM = { {300, 1, {50,0,0,0,0}}, {400, 1, {50,0,0,0,0}}, {600, 2, {40,80,0,0,0}}, {750, 2, {40,80,0,0,0}}, {1000, 2, {33,67,0,0,0}} };
const AutoProgramEntry PROGMEM ac1_potato[] = { {100,120,0,false}, {200,210,0,false}, {400,360,0,false}, {600,510,0,false}, {800,660,0,false}, {1000,780,0,false} };
const AutoProgramEntry PROGMEM ac2_fresh_veg[] = { {100,90,0,false}, {200,180,0,false}, {400,300,0,false}, {600,420,0,false}, {800,540,0,false} };
const AutoProgramEntry PROGMEM ac3_frozen_veg[] = { {100,120,0,false}, {200,240,0,false}, {400,420,0,false}, {600,570,0,false}, {800,720,0,false} };
const AutoProgramEntry PROGMEM def1_meat[] = { {100,120,3,true}, {500,600,3,true}, {1000,1260,3,true}, {2000,2760,3,true}, {3000,4500,3,true}, {4000,5999,3,true} };
const AutoProgramEntry PROGMEM def2_poultry[] = { {100,120,3,true}, {500,570,3,true}, {1000,1200,3,true}, {2000,2640,3,true}, {3000,4320,3,true}, {4000,5999,3,true} };
const AutoProgramEntry PROGMEM def3_fish[] = { {100,90,3,true}, {500,420,3,true}, {1000,900,3,true}, {2000,1920,3,true}, {3000,3000,3,true}, {4000,4200,3,true} };
const AutoProgramEntry PROGMEM def4_bread[] = { {100,40,4,false}, {200,70,4,false}, {300,100,4,false}, {400,130,4,false}, {500,150,4,false} };

// ============================================================================
// --- 4. ГЛОБАЛЬНІ ЗМІННІ (ВИЗНАЧЕННЯ) ---
// ============================================================================
// (Оголошені 'extern' у .h, визначені тут)

volatile AppState_t g_state = STATE_IDLE;
volatile uint32_t g_millis_counter = 0; 
volatile uint16_t g_timer_ms = 0; 
volatile uint16_t g_beep_ms_counter = 0;
volatile uint16_t g_beep_flip_sequence_timer = 0;
volatile uint16_t g_clock_save_burst_timer = 0;
volatile uint16_t g_flip_beep_timeout_ms = 0;
volatile uint16_t g_key_3sec_hold_timer_ms = 0;
volatile uint16_t g_key_continuous_hold_ms = 0;
volatile uint16_t g_last_key_hold_duration = 0;
volatile char g_last_key_for_hold = 0;
volatile bool g_key_hold_3sec_flag = false;
volatile bool g_ignore_next_key_release = false; 
volatile uint16_t g_quick_start_delay_ms = 0;
volatile uint32_t g_post_cook_timer_ms = 0;
volatile uint8_t g_post_cook_sec_counter = 0; 
volatile uint16_t g_clock_save_blink_ms = 0;
volatile bool g_door_open_during_pause = false;
volatile bool g_magnetron_request = false;
volatile bool g_zvs_present = false;
volatile uint8_t g_zvs_pulse_counter = 0;
volatile uint8_t g_zvs_watchdog_counter = 0;
uint16_t g_cook_time_total_sec = 0, g_cook_original_total_time = 0;
uint8_t g_cook_power_level = 0;
uint16_t g_stage1_time_sec = 0; uint8_t g_stage1_power = 0;
uint16_t g_stage2_time_sec = 0; uint8_t g_stage2_power = 0;
bool g_was_two_stage_cook = false; 
uint8_t g_auto_program = 0; uint16_t g_auto_weight_grams = 0;
uint8_t g_input_min_tens=0, g_input_min_units=0, g_input_sec_tens=0;
uint8_t g_input_hour=0, g_input_min=0, g_input_sec=0;
bool g_is_defrost_mode = false;
volatile AutoProgramType g_active_auto_program_type = PROGRAM_NONE;
volatile uint16_t g_door_overlay_timer_ms = 0;
const uint16_t power_levels_watt[] = {700, 560, 420, 280, 140};
volatile uint8_t g_pwm_cycle_duration = 10;
volatile uint8_t g_pwm_cycle_counter_seconds = 0;
uint8_t g_pwm_on_time_seconds = 0;
volatile uint32_t g_magnetron_last_off_timestamp_ms = 0;
bool g_magnetron_is_on = false;
uint8_t g_clock_hour = 0, g_clock_min = 0, g_clock_sec = 0;
bool g_clock_24hr_mode = true;

// --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.1) ---
// Використовуємо іменований тип 'DefrostFlipInfo_t' з .h файлу
volatile DefrostFlipInfo_t g_defrost_flip_info;
// --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---


// ============================================================================
// --- 6. АПАРАТНІ ФУНКЦІЇ ---
// ============================================================================
void set_magnetron(bool on) {
    if (on) {
        uint32_t elapsed = g_millis_counter - g_magnetron_last_off_timestamp_ms;
        if (g_magnetron_last_off_timestamp_ms != 0 && elapsed < ((uint32_t)MIN_SAFE_OFF_TIME_SEC * 1000UL)) {
            return; 
        }
        g_magnetron_is_on = true;
        #if (ZVS_MODE == 0)
            MAGNETRON_PORT |= MAGNETRON_BIT;
        #else
            g_magnetron_request = true;
        #endif
    } else {
        if (g_magnetron_is_on) {
            g_magnetron_last_off_timestamp_ms = g_millis_counter;
        }
        g_magnetron_is_on = false;
        #if (ZVS_MODE == 0)
            MAGNETRON_PORT &= ~MAGNETRON_BIT;
        #else
            g_magnetron_request = false; 
            MAGNETRON_PORT &= ~MAGNETRON_BIT;
        #endif
    }
}

void set_fan(bool on) { if (on) FAN_PORT |= FAN_BIT; else FAN_PORT &= ~FAN_BIT; }
void do_short_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 300; }
void do_long_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 800; }
void do_flip_beep() { if (g_beep_flip_sequence_timer == 0) g_beep_flip_sequence_timer = 1; }

void setup_hardware() {
    DDRD &= ~ZVS_BIT; 
    CDD_DDR &= ~CDD_BIT; 
    CDD_PORT |= CDD_BIT; 
    MAGNETRON_DDR |= MAGNETRON_BIT; MAGNETRON_PORT &= ~MAGNETRON_BIT;
    FAN_DDR |= FAN_BIT; FAN_PORT &= ~FAN_BIT;
    BEEPER_DDR |= BEEPER_BIT; BEEPER_PORT &= ~BEEPER_BIT;
}
#if (ZVS_MODE == 2)
void enter_sleep_mode() { 
    reset_to_idle(); g_state=STATE_SLEEPING; set_colon_mode(COLON_OFF); 
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable();
}
void wake_up_from_sleep() { 
    sleep_disable(); g_state=STATE_IDLE; set_colon_mode(COLON_BLINK_SLOW);
}
#endif

// ============================================================================
// --- 6. ФУНКЦІЇ ОНОВЛЕННЯ СТАНУ ---
// ============================================================================
void calculate_pwm_on_time() {
    if (g_cook_power_level == 0) { g_pwm_on_time_seconds = g_pwm_cycle_duration; return; }
    uint16_t watts = power_levels_watt[g_cook_power_level];
    uint16_t on_time = (uint16_t)(((uint32_t)watts * g_pwm_cycle_duration) / 700);
    if (on_time < MIN_SAFE_ON_TIME_SEC) on_time = MIN_SAFE_ON_TIME_SEC;
    if (on_time > (g_pwm_cycle_duration - MIN_SAFE_OFF_TIME_SEC)) on_time = g_pwm_cycle_duration - MIN_SAFE_OFF_TIME_SEC;
    g_pwm_on_time_seconds = (uint8_t)on_time;
}

void recalculate_adaptive_pwm() {
    if (g_cook_original_total_time < MAGNETRON_COAST_TIME_SEC && g_cook_power_level != 0) { 
        g_pwm_cycle_duration = g_cook_original_total_time; 
    } 
    else if (g_cook_original_total_time < ADAPTIVE_PWM_THRESHOLD_SEC) { g_pwm_cycle_duration = g_cook_original_total_time; } 
    else { g_pwm_cycle_duration = 30; }
    
    calculate_pwm_on_time();
    
    // --- 🔴 ВИПРАВЛЕННЯ БАГУ ШІМ (v2.2.2) ---
    g_pwm_cycle_counter_seconds = 0;
}

bool start_cooking_cycle() {
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.2.4) ---
    // (Видалено _delay_ms() для запобігання зависанню при виклику з ISR)
    if (CDD_PIN & CDD_BIT) {
        do_short_beep();
        reset_to_idle();
        return false;
    }
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---

    set_fan(true); g_cook_original_total_time = g_cook_time_total_sec; recalculate_adaptive_pwm(); 
    
    set_magnetron(true);
    
    g_state = STATE_COOKING; set_colon_mode(COLON_ON); return true;
}
void resume_cooking() {
    if (!(CDD_PIN & CDD_BIT)) { 
        g_state=STATE_COOKING; set_colon_mode(COLON_ON); set_fan(true); 
        
        if (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC && g_cook_power_level != 0) {
            set_magnetron(false);
        } else { 
            if(g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds) set_magnetron(true); else set_magnetron(false); 
        }
    }
}
void update_clock() { g_clock_sec++; if(g_clock_sec>=60) { g_clock_sec=0; g_clock_min++; if(g_clock_min>=60) { g_clock_min=0; g_clock_hour++; if(g_clock_hour>=24) g_clock_hour=0; } } }

void calculate_flip_schedule(uint8_t program_num, uint16_t weight) {
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    const FlipSchedule* flip_table = NULL; uint8_t table_len = 0;
    if (program_num == 1) { flip_table = def_meat_flips; table_len = sizeof(def_meat_flips) / sizeof(FlipSchedule); } 
    else if (program_num == 2) { flip_table = def_poultry_flips; table_len = sizeof(def_poultry_flips) / sizeof(FlipSchedule); } 
    else if (program_num == 3) { flip_table = def_fish_flips; table_len = sizeof(def_fish_flips) / sizeof(FlipSchedule); } 
    else return;
    FlipSchedule sched;
    for (uint8_t i = 0; i < table_len; i++) { memcpy_P(&sched, &flip_table[i], sizeof(FlipSchedule)); if (weight <= sched.weight_g) break; }
    g_defrost_flip_info.num_flips_total = sched.num_flips;
    uint16_t total_time = g_cook_time_total_sec; 
    for (uint8_t i = 0; i < sched.num_flips; i++) { 
        if (sched.flip_percentages[i] > 0 && sched.flip_percentages[i] < 100) {
            g_defrost_flip_info.flip_times_sec[i] = total_time - (((uint32_t)total_time * sched.flip_percentages[i]) / 100); 
        }
    }
}
void initiate_flip_pause() { set_magnetron(false); set_fan(false); g_state = STATE_FLIP_PAUSE; g_flip_beep_timeout_ms = 5000; do_flip_beep(); }

void resume_after_flip() { 
    g_defrost_flip_info.next_flip_index++; 
    g_state = STATE_COOKING; 
    set_fan(true); 
    if (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC && g_cook_power_level != 0) {
        set_magnetron(false);
    } else {
        set_magnetron(true);
    }
}
void check_flip_required() {
    if (g_active_auto_program_type != PROGRAM_DEFROST || g_defrost_flip_info.next_flip_index >= g_defrost_flip_info.num_flips_total) return;
    uint16_t next_flip_time = g_defrost_flip_info.flip_times_sec[g_defrost_flip_info.next_flip_index];
    if (next_flip_time > 0 && g_cook_time_total_sec == next_flip_time) initiate_flip_pause();
}
void update_cook_timer() {
    if (g_state == STATE_COOKING && g_cook_time_total_sec > 0) {
        if (g_cook_time_total_sec <= 3 && g_stage2_time_sec == 0) do_long_beep();
        
        g_cook_time_total_sec--; 
        check_flip_required();
        
        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.2.3) ---
        bool should_be_on = (g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds);
        if (should_be_on && 
            (g_cook_original_total_time >= ADAPTIVE_PWM_THRESHOLD_SEC) && 
            (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) && 
            (g_cook_power_level != 0))
        {
            set_magnetron(false);
        } 
        else if (should_be_on) 
        {
            set_magnetron(true);
        } 
        else 
        {
            set_magnetron(false);
        }
        // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
        
        g_pwm_cycle_counter_seconds++; 
        if (g_pwm_cycle_counter_seconds >= g_pwm_cycle_duration) g_pwm_cycle_counter_seconds = 0;
        
        if (g_cook_time_total_sec == 0) {
            if (g_stage2_time_sec > 0) { 
                g_cook_time_total_sec = g_stage2_time_sec; 
                g_cook_power_level = g_stage2_power; 
                start_cooking_cycle(); 
                g_stage2_time_sec = 0; 
            }
            else { 
                g_state=STATE_FINISHED; 
                set_colon_mode(COLON_OFF); 
                g_post_cook_timer_ms = 0; 
                g_post_cook_sec_counter = 0; 
                set_magnetron(false); 
                set_fan(false); 
            }
        }
    }
}
void get_program_settings(const AutoProgramEntry* table, uint8_t len, uint16_t weight) {
    AutoProgramEntry entry; for (uint8_t i=0; i<len; i++) { memcpy_P(&entry, &table[i], sizeof(AutoProgramEntry)); if (i==(len-1) || weight < pgm_read_word(&table[i+1].weight_g)) break; }
    g_cook_time_total_sec=entry.time_sec; g_cook_power_level=entry.power_level;
}

// ============================================================================
// --- 8. ГОЛОВНА ЛОГІКА ---
// ============================================================================

void reset_to_idle() {
    g_state=STATE_IDLE; set_colon_mode(COLON_BLINK_SLOW);
    g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; g_input_hour=0; g_input_min=0; g_input_sec=0;
    g_cook_power_level=0; g_cook_time_total_sec=0; g_cook_original_total_time=0;
    g_stage1_time_sec=0; g_stage2_time_sec=0; g_quick_start_delay_ms=0; g_post_cook_timer_ms=0; g_clock_save_blink_ms=0;
    g_door_open_during_pause=false; set_magnetron(false); set_fan(false);
    g_active_auto_program_type = PROGRAM_NONE; g_door_overlay_timer_ms = 0;
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    g_beep_flip_sequence_timer = 0; g_clock_save_burst_timer = 0; g_flip_beep_timeout_ms = 0;
    g_magnetron_last_off_timestamp_ms = 0; 
    g_was_two_stage_cook = false;
    g_post_cook_sec_counter = 0;
}

void handle_time_input_odometer(char key) {
    uint16_t ts = (g_input_min_tens*10 + g_input_min_units)*60 + (g_input_sec_tens*10);
    if (ts >= 5990) { g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; return; }
    if (key == KEY_10_SEC) { g_input_sec_tens++; if (g_input_sec_tens > 5) { g_input_sec_tens = 0; g_input_min_units++; if (g_input_min_units > 9) { g_input_min_units = 0; g_input_min_tens++; if (g_input_min_tens > 9) { g_input_min_tens=9; g_input_min_units=9; g_input_sec_tens=5; } } } }
    else if (key == KEY_1_MIN) { g_input_min_units++; if (g_input_min_units > 9) { g_input_min_units = 0; g_input_min_tens++; if (g_input_min_tens > 9) g_input_min_tens = 0; } }
    else if (key == KEY_10_MIN) { g_input_min_tens++; if (g_input_min_tens > 9) g_input_min_tens = 0; }
}
void handle_clock_input(char key) {
    g_input_sec = 0;
    if (key == KEY_10_MIN) { g_input_hour++; if (g_input_hour >= 24) g_input_hour = 0; }
    else if (key == KEY_1_MIN) { uint8_t ones = g_input_min % 10; uint8_t tens = (g_input_min / 10); tens++; if (tens >= 6) tens = 0; g_input_min = (tens * 10) + ones; }
    else if (key == KEY_10_SEC) { uint8_t tens = (g_input_min / 10); uint8_t ones = g_input_min % 10; ones++; if (ones >= 10) ones = 0; g_input_min = (tens * 10) + ones; }
}

void handle_state_machine(char key, bool allow_beep) {
    if (g_state == STATE_SLEEPING) return;
    
    // --- 🔴 Логіка Більше/Менше під час приготування (v2.2.2) ---
    if (g_state == STATE_COOKING && g_active_auto_program_type == PROGRAM_NONE) {
        if (key == KEY_MORE) {
            if (g_cook_time_total_sec > 5989) { 
                g_cook_time_total_sec = 5999;
                g_cook_original_total_time = 5999;
            } else {
                g_cook_time_total_sec += 10;
                g_cook_original_total_time += 10;
            }
            if (allow_beep) do_short_beep();
            return;
        }

        if (key == KEY_LESS) {
            if (g_cook_time_total_sec <= 10) {
                g_cook_time_total_sec = 0;
                g_state = STATE_FINISHED;
                set_colon_mode(COLON_OFF);
                g_post_cook_timer_ms = 0;
                g_post_cook_sec_counter = 0;
                set_magnetron(false);
                set_fan(false);
            } else {
                g_cook_time_total_sec -= 10;
                g_cook_original_total_time -= 10;
            }
            if (allow_beep) do_short_beep();
            return;
        }
    }
    // --- 🔴 Кінець блоку ---


    if (key == KEY_START_QUICKSTART && g_state == STATE_IDLE) {
        g_cook_time_total_sec = 30; g_quick_start_delay_ms = 1000; g_state = STATE_QUICK_START_PREP; g_active_auto_program_type = PROGRAM_NONE; if (allow_beep) do_short_beep(); return;
    }
    if (key == KEY_START_QUICKSTART && g_state == STATE_COOKING) {
        if (g_cook_time_total_sec <= (5999 - 30)) { g_cook_time_total_sec += 30; g_cook_original_total_time += 30; } else { g_cook_time_total_sec = 5999; g_cook_original_total_time = 5999; }
        recalculate_adaptive_pwm(); if (allow_beep) do_short_beep(); return;
    }
    
    switch (g_state) {
        case STATE_IDLE:
            if (key == KEY_CLOCK) { g_state = STATE_SET_CLOCK_MODE; g_clock_24hr_mode = true; if (allow_beep) do_short_beep(); }
            else if (key == KEY_MICRO) { g_cook_power_level = 0; g_state = STATE_SET_POWER; if (allow_beep) do_short_beep(); }
            else if (key == KEY_AUTO_COOK) { g_auto_program = 1; g_state = STATE_SET_AUTO_COOK; if (allow_beep) do_short_beep(); }
            else if (key == KEY_AUTO_DEFROST) { g_auto_program = 1; g_state = STATE_SET_AUTO_DEFROST; if (allow_beep) do_short_beep(); }
            else if (key == KEY_QUICK_DEFROST) { g_active_auto_program_type = PROGRAM_DEFROST; g_auto_program = 1; get_program_settings(def1_meat, 6, 500); calculate_flip_schedule(1, 500); start_cooking_cycle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SLEEPING: break;
        case STATE_SET_CLOCK_MODE:
            if (key == KEY_CLOCK) { g_clock_24hr_mode = !g_clock_24hr_mode; if (allow_beep) do_short_beep(); }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { g_input_hour=g_clock_hour; g_input_min=g_clock_min; g_input_sec=g_clock_sec; g_state = STATE_SET_CLOCK_TIME; handle_clock_input(key); if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_CLOCK_TIME:
            if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { handle_clock_input(key); if (allow_beep) do_short_beep(); }
            else if (key == KEY_CLOCK) { g_clock_hour=g_input_hour; g_clock_min=g_input_min; g_clock_sec=g_input_sec; g_clock_save_blink_ms=2000; g_state=STATE_CLOCK_SAVED; g_clock_save_burst_timer = 800; }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_CLOCK_SAVED: break;
        case STATE_SET_TIME:
            if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { handle_time_input_odometer(key); if (allow_beep) do_short_beep(); }
            else if (key == KEY_START_QUICKSTART) { 
                uint16_t current_time_sec = (g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
                if (g_stage1_time_sec > 0) { 
                    g_stage2_time_sec = current_time_sec; g_stage2_power = g_cook_power_level; 
                    g_cook_time_total_sec = g_stage1_time_sec; g_cook_power_level = g_stage1_power; 
                    g_was_two_stage_cook = true; 
                } else { 
                    g_cook_time_total_sec = current_time_sec;
                }
                g_active_auto_program_type = PROGRAM_NONE; 
                if(g_cook_time_total_sec > 0) { start_cooking_cycle(); if (allow_beep) do_short_beep(); } else reset_to_idle();
            }
            else if (key == KEY_MICRO) { 
                g_cook_time_total_sec = (g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
                if (g_cook_time_total_sec > 0) { 
                    g_stage1_time_sec=g_cook_time_total_sec; g_stage1_power=g_cook_power_level; 
                    g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; g_cook_power_level=0; 
                    g_state=STATE_SET_POWER; 
                    if (allow_beep) do_short_beep();
                }
            }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_POWER:
            if (key == KEY_MICRO) { g_cook_power_level++; if(g_cook_power_level>=5) g_cook_power_level=0; if (allow_beep) do_short_beep(); }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { handle_time_input_odometer(key); g_state=STATE_SET_TIME; if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_AUTO_COOK:
            g_is_defrost_mode = false; if (key == KEY_AUTO_COOK) { g_auto_program++; if(g_auto_program>3) g_auto_program=1; if (allow_beep) do_short_beep(); }
            else if (key==KEY_MORE || key==KEY_LESS) { g_auto_weight_grams=100; if(g_auto_program>1) g_auto_weight_grams=100; g_state=STATE_SET_WEIGHT; if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_AUTO_DEFROST:
             g_is_defrost_mode = true; if (key == KEY_AUTO_DEFROST) { g_auto_program++; if(g_auto_program>4) g_auto_program=1; if (allow_beep) do_short_beep(); }
             else if (key==KEY_MORE || key==KEY_LESS) { g_auto_weight_grams=100; g_state=STATE_SET_WEIGHT; if (allow_beep) do_short_beep(); }
             else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
             break;
        case STATE_SET_WEIGHT: {
            uint16_t min_w=100, max_w=1000; if (g_is_defrost_mode) { if(g_auto_program==4) max_w=500; else max_w=4000; } else { if(g_auto_program>1) { min_w=100; max_w=800; } }
            if (g_auto_weight_grams < min_w) g_auto_weight_grams = min_w;
            if (key == KEY_MORE) { g_auto_weight_grams+=100; if(g_auto_weight_grams>max_w) g_auto_weight_grams=min_w; if (allow_beep) do_short_beep(); }
            else if (key == KEY_LESS) { if(g_auto_weight_grams>min_w) g_auto_weight_grams-=100; else g_auto_weight_grams=max_w; if (allow_beep) do_short_beep(); }
            else if (key == KEY_START_QUICKSTART) {
                if (g_is_defrost_mode) { g_active_auto_program_type = PROGRAM_DEFROST; switch (g_auto_program) { case 1: get_program_settings(def1_meat, 6, g_auto_weight_grams); calculate_flip_schedule(1, g_auto_weight_grams); break; case 2: get_program_settings(def2_poultry, 6, g_auto_weight_grams); calculate_flip_schedule(2, g_auto_weight_grams); break; case 3: get_program_settings(def3_fish, 6, g_auto_weight_grams); calculate_flip_schedule(3, g_auto_weight_grams); break; case 4: get_program_settings(def4_bread, 5, g_auto_weight_grams); break; } } 
                else { g_active_auto_program_type = PROGRAM_COOK; switch (g_auto_program) { case 1: get_program_settings(ac1_potato, 6, g_auto_weight_grams); break; case 2: get_program_settings(ac2_fresh_veg, 5, g_auto_weight_grams); break; case 3: get_program_settings(ac3_frozen_veg, 5, g_auto_weight_grams); break; } }
                if(g_cook_time_total_sec>0) { start_cooking_cycle(); if (allow_beep) do_short_beep(); } else reset_to_idle();
            }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
        } break;
        case STATE_QUICK_START_PREP:
            if (key == KEY_START_QUICKSTART) { g_cook_time_total_sec+=30; if(g_cook_time_total_sec>5999) g_cook_time_total_sec=30; g_quick_start_delay_ms=1000; if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_COOKING:
            if (key == KEY_STOP_RESET) { g_state=STATE_PAUSED; set_magnetron(false); set_fan(false); if (allow_beep) do_short_beep(); }
            break;
        case STATE_PAUSED:
            if (key == KEY_START_QUICKSTART) { resume_cooking(); if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_FLIP_PAUSE:
            if (key == KEY_START_QUICKSTART) { resume_after_flip(); if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_FINISHED: case STATE_POST_COOK:
            if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_LOCKED:
            if (key == KEY_STOP_RESET) { /* Розблокування тільки через довге утримання */ }
            break;
        default: reset_to_idle();
    }
}

// ============================================================================
// --- 9. ПЕРЕРИВАННЯ ТА ОСНОВНИЙ ЦИКЛ ---
// ============================================================================

void setup_timer1_1ms() {
    TCCR1A=0; TCCR1B=0; TCNT1=0; 
    OCR1A=1999; 
    TCCR1B|=(1<<WGM12)|(1<<CS11); 
    TIMSK|=(1<<OCIE1A); 
}

ISR(TIMER1_COMPA_vect) { 
    g_millis_counter++; 
    update_colon_state(); // З display_driver
    
    // --- Обробка звуку ---
    if (g_beep_ms_counter > 0) { if(g_beep_ms_counter == 800 || g_beep_ms_counter == 300) BEEPER_PORT|=BEEPER_BIT; g_beep_ms_counter--; if(g_beep_ms_counter == 0) BEEPER_PORT&=~BEEPER_BIT; }
    if (g_clock_save_burst_timer > 0) { g_clock_save_burst_timer--; if ((g_clock_save_burst_timer % 100) == 50) BEEPER_PORT |= BEEPER_BIT; else if ((g_clock_save_burst_timer % 100) == 0) BEEPER_PORT &= ~BEEPER_BIT; if (g_clock_save_burst_timer == 0) BEEPER_PORT &= ~BEEPER_BIT; }
    if (g_beep_flip_sequence_timer > 0) { if (g_beep_flip_sequence_timer == 1 || g_beep_flip_sequence_timer == 601 || g_beep_flip_sequence_timer == 1201) { do_short_beep(); } g_beep_flip_sequence_timer++; if (g_flip_beep_timeout_ms == 0 || g_beep_flip_sequence_timer > 1501) { g_beep_flip_sequence_timer = 0; } }

    // --- Обробка утримання кнопок ---
    char rk = get_key_press(); // З keypad_driver
    if (rk == g_last_key_for_hold && rk != 0) {
        if (!g_key_hold_3sec_flag && g_key_3sec_hold_timer_ms < 3000) { g_key_3sec_hold_timer_ms++; if(g_key_3sec_hold_timer_ms==3000) g_key_hold_3sec_flag=true; }
        if (g_key_continuous_hold_ms < 65000) g_key_continuous_hold_ms++;
    } else { g_key_3sec_hold_timer_ms=0; g_last_key_hold_duration=g_key_continuous_hold_ms; g_key_continuous_hold_ms=0; g_last_key_for_hold=rk; g_key_hold_3sec_flag=false; }

    // --- Обробка опитування АЦП (з keypad_driver) ---
    keypad_timer_tick(); 

    // --- Мультиплексування дисплея (з display_driver) ---
    if(g_state!=STATE_SLEEPING) run_display_multiplex();
    
    g_timer_ms++; 
    
    // --- Загальні таймери ---
    if(g_quick_start_delay_ms>0) { g_quick_start_delay_ms--; if(g_quick_start_delay_ms==0 && g_state==STATE_QUICK_START_PREP) start_cooking_cycle(); }
    if(g_state==STATE_FINISHED) { g_post_cook_timer_ms++; if(g_post_cook_timer_ms >= 30000) { g_state=STATE_POST_COOK; g_post_cook_timer_ms=0; g_post_cook_sec_counter = 0; do_long_beep(); } } 
    else if(g_state==STATE_POST_COOK) { g_post_cook_timer_ms++; }
    if(g_clock_save_blink_ms>0) { g_clock_save_blink_ms--; if(g_clock_save_blink_ms==0) reset_to_idle(); }
    if(g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms--;
    if (g_flip_beep_timeout_ms > 0) g_flip_beep_timeout_ms--;
    
    // --- 1-секундний таймер ---
    if(g_timer_ms>=1000) {
        g_timer_ms=0;
        if (g_door_overlay_timer_ms == 0) {
            if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE) update_cook_timer();
            
            if(g_state==STATE_POST_COOK) { 
                g_post_cook_sec_counter++;
                if(g_post_cook_sec_counter == 60) do_long_beep(); 
                else if(g_post_cook_sec_counter >= 120) { do_long_beep(); reset_to_idle(); } 
            }
            
            #if (ZVS_MODE==0)
                if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE) update_clock();
            #elif (ZVS_MODE==1 || ZVS_MODE==2)
                g_zvs_watchdog_counter++;
                if(g_zvs_watchdog_counter>1) { 
                    if(g_zvs_present) { 
                        g_zvs_present=false; 
                        #if (ZVS_MODE==2)
                            enter_sleep_mode(); 
                        #endif 
                    } 
                    if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE) update_clock(); 
                }
            #endif 
        }
    }
}

#if (ZVS_MODE!=0)
ISR(INT0_vect) {
    if(g_magnetron_request) MAGNETRON_PORT|=MAGNETRON_BIT;
    g_zvs_watchdog_counter=0; 
    if(!g_zvs_present) { 
        g_zvs_present=true; 
        #if (ZVS_MODE==2)
            if(g_state==STATE_SLEEPING) wake_up_from_sleep(); 
        #endif 
    }
    g_zvs_pulse_counter++;
    if(g_door_overlay_timer_ms == 0 && g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE) { 
        if(g_zvs_pulse_counter>=50) { 
            g_zvs_pulse_counter=0; g_timer_ms=0; update_clock(); 
        } 
    }
}
#endif

void setup() {
    setup_display_pins(); // з display_driver
    setup_hardware();
    #if ENABLE_KEYPAD
        keypad_init(); // з keypad_driver
    #endif
    setup_timer1_1ms(); 
    #if (ZVS_MODE!=0)
        MCUCR|=(1<<ISC01); MCUCR&=~(1<<ISC00); GIMSK|=(1<<INT0); g_zvs_watchdog_counter=0; g_zvs_present=true;
    #endif
    
    reset_to_idle(); 
    
    sei();
}

void loop() {
    static char s_lps=0; static uint16_t s_lht=0; char cks=0;
    static bool s_last_door_state = false;
    
    #if (ZVS_MODE==2)
        if(g_state==STATE_SLEEPING) { sleep_cpu(); s_lps=0; s_last_door_state = (CDD_PIN & CDD_BIT); }
    #endif
        
    if(g_state!=STATE_SLEEPING) {
    
        // --- Логіка блокування (утримання STOP) ---
        if(g_key_hold_3sec_flag) {
            g_key_hold_3sec_flag=false;
            char hk=g_last_key_for_hold;
            if(hk==KEY_STOP_RESET) { 
                if(g_state==STATE_IDLE) { 
                    g_state = STATE_LOCKED; 
                    do_short_beep();
                } 
                else if(g_state==STATE_LOCKED) {
                    reset_to_idle(); 
                    do_short_beep();
                }
            }
            s_lps=0;
        }

        // --- Логіка дверей ---
        bool door_is_open = (CDD_PIN & CDD_BIT);
        if (door_is_open != s_last_door_state) {
            if (door_is_open) {
                if (g_state == STATE_COOKING) { g_state = STATE_PAUSED; g_door_open_during_pause = true; set_magnetron(false); set_fan(false); } 
                else if (g_state == STATE_PAUSED || g_state == STATE_FLIP_PAUSE) { g_door_open_during_pause = true; g_flip_beep_timeout_ms = 0; } 
                else if (g_state == STATE_FINISHED || g_state == STATE_POST_COOK) { reset_to_idle(); } 
                else if (g_state != STATE_LOCKED) { g_door_overlay_timer_ms = 2000; }
            } else { 
                if (g_door_open_during_pause) g_door_open_during_pause = false;
                if (g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms = 0;
            }
            s_last_door_state = door_is_open;
        }

        // --- Логіка обробки кнопок ---
        bool allow_keys=true;
        if(g_state==STATE_LOCKED || g_state==STATE_CLOCK_SAVED || g_door_overlay_timer_ms > 0) allow_keys=false;
        if(g_door_open_during_pause) allow_keys=true;
        
        if(allow_keys) {
            cks=get_key_press(); // з keypad_driver
            if(g_door_open_during_pause && cks!=KEY_STOP_RESET && cks!=0) cks=0;
            
            // Обробка утримання
            if(cks!=0) { 
                if(g_key_continuous_hold_ms > 500) 
                    handle_key_hold_increment(cks, g_key_continuous_hold_ms, &s_lht); // з keypad_driver
            } else {
                s_lht=0;
            }
            
            // --- Обробка відпускання кнопки (v2.2.2) ---
            if(cks!=s_lps) { 
                if(cks==0) { // Кнопку щойно відпустили
                    
                    char released_key = s_lps; 

                    // 1. Обробка "Тапу" (короткого натискання)
                    if(released_key != 0 && g_last_key_hold_duration <= 500) {
                        handle_state_machine(released_key, true);
                    }

                    // 2. Перерахунок ШІМ при відпусканні
                    if (g_state == STATE_COOKING && 
                       (released_key == KEY_MORE || released_key == KEY_LESS)) 
                    {
                        if (g_cook_time_total_sec > 0) {
                            recalculate_adaptive_pwm();
                        }
                    }
                }
                s_lps=cks; 
            }
            // --- Кінець блоку ---

        } else { s_lps=0; s_lht=0; }
        
        // --- Обробка стану "Завершено" ---
        if(g_state==STATE_FINISHED || g_state==STATE_POST_COOK) handle_state_machine(0, false);
        
        // --- Оновлення дисплея ---
        if (g_state != STATE_SLEEPING) {
            update_display(); // з display_driver
        }
    }
}

int main(void) { 
    setup(); 
    while(1) { 
        loop(); 
    } 
    return 0; 
}