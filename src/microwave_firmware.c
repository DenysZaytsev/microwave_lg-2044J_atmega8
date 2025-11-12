/*
 * ПОВНА ПРОШИВКА МІКРОХВИЛЬОВКИ (v_final_2.4.4_isr_refactor)
 * Модель: LG MS-2044J (на базі ATmega8, 16МГц)
 *
 * --- ОПИС ФУНКЦІОНАЛУ v2.4.4 ---
 * 1.  (v2.3.3) Виправлено баг 2-го етапу.
 * 2.  (v2.3.8) Виправлено фільтр ZVS (надійний підрахунок >= 40 Гц).
 * 3.  (v2.3.9) Видалено логіку "Більше/Менше" під час готування (економія пам'яті).
 * 4.  (v2.4.4) РЕФАКТОРИНГ АРХІТЕКТУРИ (Виправлення таймінгу АЦП):
 * Повертаємо 1мс-завдання (дисплей, АЦП) назад у ISR(TIMER1_COMPA_vect),
 * оскільки рефакторинг v2.4.1 зламав їх стабільний таймінг.
 * "Важка" 1-секундна логіка залишається в loop() (через прапор g_1sec_tick_flag).
 * Це архітектура v2.2.0 (яка працювала) + виправлення (ZVS, 2-етап).
 * 5.  (v2.4.3) Повернено відсутню функцію setup_timer1_1ms().
 */

// ============================================================================
// --- 🔴 ВКЛЮЧЕННЯ МОДУЛІВ ---
// ============================================================================
#include "microwave_firmware.h" // < Містить всі спільні визначення та extern
#include "display_driver.h"     // < Містить прототипи для дисплея
#include "keypad_driver.h"      // < Містить прототипи для кнопок

// --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.8 - Економний ZVS фільтр) ---
// Налаштування фільтра ZVS (мінімально допустима частота)
#define ZVS_MIN_PULSES_PER_SEC 40 
#define ZVS_QUALIFICATION_SECONDS 2 // "2 секунди стабільності"
// --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---


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

// --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.4 - Правильна Архітектура ISR/loop) ---
volatile bool g_1sec_tick_flag = false;
// --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---

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
volatile uint16_t g_cook_time_total_sec = 0, g_cook_original_total_time = 0;
volatile uint8_t g_cook_power_level = 0;
uint16_t g_stage1_time_sec = 0; uint8_t g_stage1_power = 0; // (Безпечно, тільки в loop)
volatile uint16_t g_stage2_time_sec = 0; volatile uint8_t g_stage2_power = 0;
volatile bool g_was_two_stage_cook = false; 
volatile uint8_t g_auto_program = 0; volatile uint16_t g_auto_weight_grams = 0;
volatile uint8_t g_input_min_tens=0, g_input_min_units=0, g_input_sec_tens=0;
volatile uint8_t g_input_hour=0, g_input_min=0, g_input_sec=0;
bool g_is_defrost_mode = false; // (Безпечно, тільки в loop)
volatile AutoProgramType g_active_auto_program_type = PROGRAM_NONE;
volatile uint16_t g_door_overlay_timer_ms = 0;
const uint16_t power_levels_watt[] = {700, 560, 420, 280, 140};
volatile uint8_t g_pwm_cycle_duration = 10;
volatile uint8_t g_pwm_cycle_counter_seconds = 0;
volatile uint8_t g_pwm_on_time_seconds = 0;
volatile uint32_t g_magnetron_last_off_timestamp_ms = 0;
volatile bool g_magnetron_is_on = false;
volatile uint8_t g_clock_hour = 0, g_clock_min = 0, g_clock_sec = 0;
volatile bool g_clock_24hr_mode = true;
volatile DefrostFlipInfo_t g_defrost_flip_info;

// --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.8 - Економний ZVS фільтр) ---
volatile uint8_t g_zvs_qualification_counter = 0;
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

// --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.2) ---
// Повністю переписана логіка для коректної обробки
// конфліктів MIN_SAFE_ON_TIME та MIN_SAFE_OFF_TIME на коротких циклах
void calculate_pwm_on_time() {
    if (g_cook_power_level == 0) { 
        g_pwm_on_time_seconds = g_pwm_cycle_duration; // Max power (700W)
        return; 
    }

    // 1. Розрахувати ідеальний час увімкнення
    uint16_t watts = power_levels_watt[g_cook_power_level];
    uint16_t on_time = (uint16_t)(((uint32_t)watts * g_pwm_cycle_duration) / 700);

    // 2. Визначити жорсткі межі
    uint8_t min_on = MIN_SAFE_ON_TIME_SEC;
    uint8_t max_on = g_pwm_cycle_duration;
    
    // Переконатися, що ми залишаємо час для MIN_SAFE_OFF_TIME_SEC,
    // але тільки якщо цикл взагалі довший за цей час
    if (g_pwm_cycle_duration > MIN_SAFE_OFF_TIME_SEC) {
         max_on = g_pwm_cycle_duration - MIN_SAFE_OFF_TIME_SEC;
    } else {
         // Якщо цикл ДУЖЕ короткий (<= 2 сек), max_on має бути 0
         max_on = 0; 
    }

    // 3. Обробка конфлікту: (наприклад, цикл 5с -> min=5, max=3)
    // Це означає, що цикл занадто короткий, щоб безпечно дотримуватись обох правил.
    // У цьому випадку, пріоритет має MIN_SAFE_OFF_TIME (безпека вимкнення).
    if (min_on > max_on) {
        on_time = max_on; 
    } else {
        // 4. Конфлікту немає, просто застосовуємо межі
        if (on_time < min_on) on_time = min_on;
        if (on_time > max_on) on_time = max_on;
    }

    g_pwm_on_time_seconds = (uint8_t)on_time;
}
// --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---

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

    set_fan(true); 
    g_cook_original_total_time = g_cook_time_total_sec; 
    recalculate_adaptive_pwm(); 
    
    set_magnetron(true);
    
    g_state = STATE_COOKING; 
    set_colon_mode(COLON_ON); 
    return true;
}

void resume_cooking() {
    if (!(CDD_PIN & CDD_BIT)) { 
        g_state=STATE_COOKING; 
        set_colon_mode(COLON_ON); 
        set_fan(true); 
        
        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.2 - аналогічно update_cook_timer) ---
        bool should_be_on = (g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds);
        if (should_be_on && 
            (g_cook_original_total_time >= ADAPTIVE_PWM_THRESHOLD_SEC) && 
            (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) && 
            (g_cook_power_level != 0))
        {
            set_magnetron(false); // Coasting
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
    }
}

void update_clock() { 
    g_clock_sec++; 
    if(g_clock_sec>=60) { 
        g_clock_sec=0; 
        g_clock_min++; 
        if(g_clock_min>=60) { 
            g_clock_min=0; 
            g_clock_hour++; 
            if(g_clock_hour>=24) g_clock_hour=0; 
        } 
    } 
}

void calculate_flip_schedule(uint8_t program_num, uint16_t weight) {
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    const FlipSchedule* flip_table = NULL; 
    uint8_t table_len = 0;
    
    if (program_num == 1) { 
        flip_table = def_meat_flips; 
        table_len = sizeof(def_meat_flips) / sizeof(FlipSchedule); 
    } 
    else if (program_num == 2) { 
        flip_table = def_poultry_flips; 
        table_len = sizeof(def_poultry_flips) / sizeof(FlipSchedule); 
    } 
    else if (program_num == 3) { 
        flip_table = def_fish_flips; 
        table_len = sizeof(def_fish_flips) / sizeof(FlipSchedule); 
    } 
    else return;
    
    FlipSchedule sched;
    for (uint8_t i = 0; i < table_len; i++) { 
        memcpy_P(&sched, &flip_table[i], sizeof(FlipSchedule)); 
        if (weight <= sched.weight_g) break; 
    }
    
    g_defrost_flip_info.num_flips_total = sched.num_flips;
    uint16_t total_time = g_cook_time_total_sec; 
    for (uint8_t i = 0; i < sched.num_flips; i++) { 
        if (sched.flip_percentages[i] > 0 && sched.flip_percentages[i] < 100) {
            g_defrost_flip_info.flip_times_sec[i] = total_time - (((uint32_t)total_time * sched.flip_percentages[i]) / 100); 
        }
    }
}

void initiate_flip_pause() { 
    set_magnetron(false); 
    set_fan(false); 
    g_state = STATE_FLIP_PAUSE; 
    g_flip_beep_timeout_ms = 5000; 
    do_flip_beep(); 
}

void resume_after_flip() { 
    g_defrost_flip_info.next_flip_index++; 
    g_state = STATE_COOKING; 
    set_fan(true); 
    
    // Використовуємо ту ж логіку, що і в resume_cooking
    bool should_be_on = (g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds);
    if (should_be_on && 
        (g_cook_original_total_time >= ADAPTIVE_PWM_THRESHOLD_SEC) && 
        (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) && 
        (g_cook_power_level != 0))
    {
        set_magnetron(false); // Coasting
    } 
    else if (should_be_on) 
    {
        set_magnetron(true);
    } 
    else 
    {
        set_magnetron(false);
    }
}

void check_flip_required() {
    if (g_active_auto_program_type != PROGRAM_DEFROST || g_defrost_flip_info.next_flip_index >= g_defrost_flip_info.num_flips_total) return;
    
    uint16_t next_flip_time = g_defrost_flip_info.flip_times_sec[g_defrost_flip_info.next_flip_index];
    if (next_flip_time > 0 && g_cook_time_total_sec == next_flip_time) 
        initiate_flip_pause();
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
            // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.3 - Баг 2-го етапу) ---
            if (g_stage2_time_sec > 0) { 
                // НЕ запускаємо 2-й етап одразу з ISR.
                // Встановлюємо новий стан і вимикаємо все.
                // Головний цикл loop() підхопить цей стан і безпечно запустить 2-й етап.
                g_state = STATE_STAGE2_TRANSITION;
                set_colon_mode(COLON_OFF); 
                set_magnetron(false); // Примусово вимкнути (це встановить g_magnetron_last_off_timestamp_ms)
                set_fan(false);
            }
            // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
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
    AutoProgramEntry entry; 
    for (uint8_t i=0; i<len; i++) { 
        memcpy_P(&entry, &table[i], sizeof(AutoProgramEntry)); 
        if (i==(len-1) || weight < pgm_read_word(&table[i+1].weight_g)) 
            break; 
    }
    g_cook_time_total_sec=entry.time_sec; 
    g_cook_power_level=entry.power_level;
}

// ============================================================================
// --- 8. ГОЛОВНА ЛОГІКА ---
// ============================================================================

void reset_to_idle() {
    g_state=STATE_IDLE; 
    set_colon_mode(COLON_BLINK_SLOW);
    g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; g_input_hour=0; g_input_min=0; g_input_sec=0;
    g_cook_power_level=0; g_cook_time_total_sec=0; g_cook_original_total_time=0;
    g_stage1_time_sec=0; g_stage2_time_sec=0; g_quick_start_delay_ms=0; g_post_cook_timer_ms=0; g_clock_save_blink_ms=0;
    g_door_open_during_pause=false; 
    set_magnetron(false); 
    set_fan(false);
    g_active_auto_program_type = PROGRAM_NONE; 
    g_door_overlay_timer_ms = 0;
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    g_beep_flip_sequence_timer = 0; 
    g_clock_save_burst_timer = 0; 
    g_flip_beep_timeout_ms = 0;
    g_magnetron_last_off_timestamp_ms = 0; 
    g_was_two_stage_cook = false;
    g_post_cook_sec_counter = 0;
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.8 - Економний ZVS фільтр) ---
    g_zvs_qualification_counter = 0;
    g_zvs_pulse_counter = 0;
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
}

void handle_time_input_odometer(char key) {
    uint16_t ts = (g_input_min_tens*10 + g_input_min_units)*60 + (g_input_sec_tens*10);
    if (ts >= 5990) { 
        g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; 
        return; 
    }
    
    if (key == KEY_10_SEC) { 
        g_input_sec_tens++; 
        if (g_input_sec_tens > 5) { 
            g_input_sec_tens = 0; 
            g_input_min_units++; 
            if (g_input_min_units > 9) { 
                g_input_min_units = 0; 
                g_input_min_tens++; 
                if (g_input_min_tens > 9) { 
                    g_input_min_tens=9; g_input_min_units=9; g_input_sec_tens=5; 
                } 
            } 
        } 
    }
    else if (key == KEY_1_MIN) { 
        g_input_min_units++; 
        if (g_input_min_units > 9) { 
            g_input_min_units = 0; 
            g_input_min_tens++; 
            if (g_input_min_tens > 9) g_input_min_tens = 0; 
        } 
    }
    else if (key == KEY_10_MIN) { 
        g_input_min_tens++; 
        if (g_input_min_tens > 9) g_input_min_tens = 0; 
    }
}

void handle_clock_input(char key) {
    g_input_sec = 0;
    if (key == KEY_10_MIN) { 
        g_input_hour++; 
        if (g_input_hour >= 24) g_input_hour = 0; 
    }
    else if (key == KEY_1_MIN) { 
        uint8_t ones = g_input_min % 10; 
        uint8_t tens = (g_input_min / 10); 
        tens++; 
        if (tens >= 6) tens = 0; 
        g_input_min = (tens * 10) + ones; 
    }
    else if (key == KEY_10_SEC) { 
        uint8_t tens = (g_input_min / 10); 
        uint8_t ones = g_input_min % 10; 
        ones++; 
        if (ones >= 10) ones = 0; 
        g_input_min = (tens * 10) + ones; 
    }
}

void handle_state_machine(char key, bool allow_beep) {
    if (g_state == STATE_SLEEPING) return;
    
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.9 - Оптимізація пам'яті) ---
    // Повністю видалено логіку "Більше/Менше" для стану STATE_COOKING
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---


    if (key == KEY_START_QUICKSTART && g_state == STATE_IDLE) {
        g_cook_time_total_sec = 30; 
        g_quick_start_delay_ms = 1000; 
        g_state = STATE_QUICK_START_PREP; 
        g_active_auto_program_type = PROGRAM_NONE; 
        if (allow_beep) do_short_beep(); 
        return;
    }
    if (key == KEY_START_QUICKSTART && g_state == STATE_COOKING) {
        if (g_cook_time_total_sec <= (5999 - 30)) { 
            g_cook_time_total_sec += 30; 
            g_cook_original_total_time += 30; 
        } else { 
            g_cook_time_total_sec = 5999; 
            g_cook_original_total_time = 5999; 
        }
        recalculate_adaptive_pwm(); 
        if (allow_beep) do_short_beep(); 
        return;
    }
    
    switch (g_state) {
        case STATE_IDLE:
            if (key == KEY_CLOCK) { 
                g_state = STATE_SET_CLOCK_MODE; 
                g_clock_24hr_mode = true; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_MICRO) { 
                g_cook_power_level = 0; 
                g_state = STATE_SET_POWER; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_AUTO_COOK) { 
                g_auto_program = 1; 
                g_state = STATE_SET_AUTO_COOK; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_AUTO_DEFROST) { 
                g_auto_program = 1; 
                g_state = STATE_SET_AUTO_DEFROST; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_QUICK_DEFROST) { 
                g_active_auto_program_type = PROGRAM_DEFROST; 
                g_auto_program = 1; 
                get_program_settings(def1_meat, 6, 500); 
                calculate_flip_schedule(1, 500); 
                start_cooking_cycle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_SLEEPING: 
            break;
            
        case STATE_SET_CLOCK_MODE:
            if (key == KEY_CLOCK) { 
                g_clock_24hr_mode = !g_clock_24hr_mode; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { 
                g_input_hour=g_clock_hour; 
                g_input_min=g_clock_min; 
                g_input_sec=g_clock_sec; 
                g_state = STATE_SET_CLOCK_TIME; 
                handle_clock_input(key); 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_SET_CLOCK_TIME:
            if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { 
                handle_clock_input(key); 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_CLOCK) { 
                g_clock_hour=g_input_hour; 
                g_clock_min=g_input_min; 
                g_clock_sec=g_input_sec; 
                g_clock_save_blink_ms=2000; 
                g_state=STATE_CLOCK_SAVED; 
                g_clock_save_burst_timer = 800; 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_CLOCK_SAVED: 
            break;
            
        case STATE_SET_TIME:
            if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { 
                handle_time_input_odometer(key); 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_START_QUICKSTART) { 
                uint16_t current_time_sec = (g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
                if (g_stage1_time_sec > 0) { 
                    g_stage2_time_sec = current_time_sec; 
                    g_stage2_power = g_cook_power_level; 
                    g_cook_time_total_sec = g_stage1_time_sec; 
                    g_cook_power_level = g_stage1_power; 
                    g_was_two_stage_cook = true; 
                } else { 
                    g_cook_time_total_sec = current_time_sec;
                }
                g_active_auto_program_type = PROGRAM_NONE; 
                if(g_cook_time_total_sec > 0) { 
                    start_cooking_cycle(); 
                    if (allow_beep) do_short_beep(); 
                } else 
                    reset_to_idle();
            }
            else if (key == KEY_MICRO) { 
                g_cook_time_total_sec = (g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
                if (g_cook_time_total_sec > 0) { 
                    g_stage1_time_sec=g_cook_time_total_sec; 
                    g_stage1_power=g_cook_power_level; 
                    g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; 
                    g_cook_power_level=0; 
                    g_state=STATE_SET_POWER; 
                    if (allow_beep) do_short_beep();
                }
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_SET_POWER:
            if (key == KEY_MICRO) { 
                g_cook_power_level++; 
                if(g_cook_power_level>=5) g_cook_power_level=0; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { 
                handle_time_input_odometer(key); 
                g_state=STATE_SET_TIME; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_SET_AUTO_COOK:
            g_is_defrost_mode = false; 
            if (key == KEY_AUTO_COOK) { 
                g_auto_program++; 
                if(g_auto_program>3) g_auto_program=1; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key==KEY_MORE || key==KEY_LESS) { 
                g_auto_weight_grams=100; 
                if(g_auto_program>1) g_auto_weight_grams=100; 
                g_state=STATE_SET_WEIGHT; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_SET_AUTO_DEFROST:
             g_is_defrost_mode = true; 
             if (key == KEY_AUTO_DEFROST) { 
                 g_auto_program++; 
                 if(g_auto_program>4) g_auto_program=1; 
                 if (allow_beep) do_short_beep(); 
             }
             else if (key==KEY_MORE || key==KEY_LESS) { 
                 g_auto_weight_grams=100; 
                 g_state=STATE_SET_WEIGHT; 
                 if (allow_beep) do_short_beep(); 
             }
             else if (key == KEY_STOP_RESET) { 
                 reset_to_idle(); 
                 if (allow_beep) do_short_beep(); 
             }
             break;
             
        case STATE_SET_WEIGHT: 
        {
            uint16_t min_w=100, max_w=1000; 
            if (g_is_defrost_mode) { 
                if(g_auto_program==4) max_w=500; else max_w=4000; 
            } else { 
                if(g_auto_program>1) { min_w=100; max_w=800; } 
            }
            
            if (g_auto_weight_grams < min_w) g_auto_weight_grams = min_w;
            
            if (key == KEY_MORE) { 
                g_auto_weight_grams+=100; 
                if(g_auto_weight_grams>max_w) g_auto_weight_grams=min_w; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_LESS) { 
                if(g_auto_weight_grams>min_w) g_auto_weight_grams-=100; 
                else g_auto_weight_grams=max_w; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_START_QUICKSTART) {
                if (g_is_defrost_mode) { 
                    g_active_auto_program_type = PROGRAM_DEFROST; 
                    switch (g_auto_program) { 
                        case 1: get_program_settings(def1_meat, 6, g_auto_weight_grams); calculate_flip_schedule(1, g_auto_weight_grams); break; 
                        case 2: get_program_settings(def2_poultry, 6, g_auto_weight_grams); calculate_flip_schedule(2, g_auto_weight_grams); break; 
                        case 3: get_program_settings(def3_fish, 6, g_auto_weight_grams); calculate_flip_schedule(3, g_auto_weight_grams); break; 
                        case 4: get_program_settings(def4_bread, 5, g_auto_weight_grams); break; 
                    } 
                } 
                else { 
                    g_active_auto_program_type = PROGRAM_COOK; 
                    switch (g_auto_program) { 
                        case 1: get_program_settings(ac1_potato, 6, g_auto_weight_grams); break; 
                        case 2: get_program_settings(ac2_fresh_veg, 5, g_auto_weight_grams); break; 
                        case 3: get_program_settings(ac3_frozen_veg, 5, g_auto_weight_grams); break; 
                    } 
                }
                if(g_cook_time_total_sec>0) { 
                    start_cooking_cycle(); 
                    if (allow_beep) do_short_beep(); 
                } else 
                    reset_to_idle();
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
        } break;
        
        case STATE_QUICK_START_PREP:
            if (key == KEY_START_QUICKSTART) { 
                g_cook_time_total_sec+=30; 
                if(g_cook_time_total_sec>5999) g_cook_time_total_sec=30; 
                g_quick_start_delay_ms=1000; 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_COOKING:
            if (key == KEY_STOP_RESET) { 
                g_state=STATE_PAUSED; 
                set_magnetron(false); 
                set_fan(false); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_PAUSED:
            if (key == KEY_START_QUICKSTART) { 
                resume_cooking(); 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_FLIP_PAUSE:
            if (key == KEY_START_QUICKSTART) { 
                resume_after_flip(); 
                if (allow_beep) do_short_beep(); 
            }
            else if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_FINISHED: 
        case STATE_POST_COOK:
            if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        case STATE_LOCKED:
            if (key == KEY_STOP_RESET) { /* Розблокування тільки через довге утримання */ }
            break;
            
        case STATE_STAGE2_TRANSITION:
            break;
            
        default: 
            reset_to_idle();
    }
}

// ============================================================================
// --- 9. ПЕРЕРИВАННЯ ТА ОСНОВНИЙ ЦИКЛ ---
// ============================================================================

void setup_timer1_1ms() {
    TCCR1A=0; TCCR1B=0; TCNT1=0; 
    
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.5 - Динамічний F_CPU) ---
    // Автоматично вибираємо правильне значення OCR1A
    // на основі 'board_build.f_cpu' з platformio.ini
    
    #if F_CPU == 16000000L
        // 16МГц / 8 (дільник) / 2000 (тиків) = 1000 Гц = 1мс
        OCR1A = 1999; 
    #elif F_CPU == 8000000L
        // 8МГц / 8 (дільник) / 1000 (тиків) = 1000 Гц = 1мс
        OCR1A = 999;
    #else
        #error "Непідтримувана частота F_CPU! Будь ласка, оновіть setup_timer1_1ms()"
    #endif
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
    
    TCCR1B|=(1<<WGM12)|(1<<CS11); // Дільник Prescaler = 8
    TIMSK|=(1<<OCIE1A); 
}

// --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.2 - Правильна Архітектура ISR/loop) ---
// Ця функція тепер виконується в loop(), а не в ISR
void run_1sec_tasks(void) {
    if (g_door_overlay_timer_ms == 0) {
            
        if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
            update_cook_timer();
        
        if(g_state==STATE_POST_COOK) { 
            g_post_cook_sec_counter++;
            if(g_post_cook_sec_counter == 60) do_long_beep(); 
            else if(g_post_cook_sec_counter >= 120) { do_long_beep(); reset_to_idle(); } 
        }
        
        #if (ZVS_MODE==0)
            if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
                update_clock();
        
        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.8 - Економний ZVS фільтр) ---
        #elif (ZVS_MODE==1 || ZVS_MODE==2)
            g_zvs_watchdog_counter++; 
            
            // Перевіряємо, чи був сигнал стабільним (>=40 Гц) 
            // І чи був хоч один імпульс (watchdog == 1)
            bool valid_pulse_train = (g_zvs_watchdog_counter == 1) && (g_zvs_pulse_counter >= ZVS_MIN_PULSES_PER_SEC);

            // Скидаємо лічильник імпульсів для наступної секунди
            g_zvs_pulse_counter = 0; 

            if(!valid_pulse_train) { 
                // Якщо імпульсів не було (watchdog > 1) АБО їх було замало (шум)
                if(g_zvs_present) { 
                    g_zvs_present = false; // Вважаємо сигнал ВТРАЧЕНИМ
                } 
                g_zvs_qualification_counter = 0; // Скидаємо лічильник кваліфікації
                
                #if (ZVS_MODE==2)
                    if (g_state != STATE_SLEEPING) enter_sleep_mode(); 
                #endif 
                
                // Використовуємо резервний годинник, оскільки ZVS немає
                if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
                    update_clock(); 
            } else {
                // (valid_pulse_train == true)
                // Ми отримали >40 імпульсів за останню секунду.
                
                if(!g_zvs_present) {
                     g_zvs_qualification_counter++; // Збільшуємо лічильник "хороших" секунд
                     
                     // Кваліфікуємо сигнал, лише якщо він стабільний 2 секунди поспіль
                     if (g_zvs_qualification_counter >= ZVS_QUALIFICATION_SECONDS) {
                         g_zvs_present = true;
                         #if (ZVS_MODE==2)
                            if(g_state==STATE_SLEEPING) wake_up_from_sleep(); 
                         #endif
                     }
                } else {
                    // Якщо сигнал вже кваліфікований, просто скидаємо лічильник (все добре)
                    g_zvs_qualification_counter = 0;
                }
                // НЕ викликаємо update_clock(), оскільки він викликається з ISR(INT0_vect)
            }
        #endif 
        // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
    }
}
// --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---


ISR(TIMER1_COMPA_vect) { 
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.4 - Правильна Архітектура ISR/loop) ---
    // Повертаємо 1мс-завдання назад в ISR (як у v2.2.0), 
    // щоб гарантувати стабільний таймінг для АЦП та дисплея
    
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
    
    // --- Загальні таймери (мілісекундні) ---
    if(g_quick_start_delay_ms>0) { g_quick_start_delay_ms--; if(g_quick_start_delay_ms==0 && g_state==STATE_QUICK_START_PREP) start_cooking_cycle(); }
    if(g_state==STATE_FINISHED) { g_post_cook_timer_ms++; if(g_post_cook_timer_ms >= 30000) { g_state=STATE_POST_COOK; g_post_cook_timer_ms=0; g_post_cook_sec_counter = 0; do_long_beep(); } } 
    else if(g_state==STATE_POST_COOK) { g_post_cook_timer_ms++; }
    if(g_clock_save_blink_ms>0) { g_clock_save_blink_ms--; if(g_clock_save_blink_ms==0) reset_to_idle(); }
    if(g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms--;
    if (g_flip_beep_timeout_ms > 0) g_flip_beep_timeout_ms--;
    
    // --- 1-секундний таймер ---
    if(g_timer_ms>=1000) {
        g_timer_ms=0;
        // Встановлюємо прапор для loop(), "важка" логіка (1-сек) тепер там
        g_1sec_tick_flag = true; 
    }
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
}

#if (ZVS_MODE!=0)
ISR(INT0_vect) {
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.8 - Економний ZVS фільтр) ---
    // Цей ISR тепер ДУЖЕ швидкий. Він не робить ніякої логіки,
    // крім підрахунку та ввімкнення магнетрона (якщо дозволено).

    g_zvs_watchdog_counter = 0; // Доказ, що імпульс був
    
    // Інкрементуємо лічильник, але не більше 254, щоб уникнути переповнення
    if (g_zvs_pulse_counter < 254) g_zvs_pulse_counter++; 
    
    // Вмикаємо магнетрон, ТІЛЬКИ ЯКЩО:
    // 1. Є запит на ввімкнення (g_magnetron_request)
    // 2. І сигнал ZVS вже визнаний стабільним (g_zvs_present)
    if(g_magnetron_request && g_zvs_present) {
        MAGNETRON_PORT |= MAGNETRON_BIT;
    }
    
    // Синхронізація годинника (тільки якщо сигнал кваліфікований)
    if(g_zvs_present && g_door_overlay_timer_ms == 0 && g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) { 
        // Ми використовуємо g_zvs_pulse_counter для підрахунку до 50 (для 50Гц)
        if(g_zvs_pulse_counter >= 50) { 
            g_zvs_pulse_counter = 0; // Скидаємо лічильник імпульсів
            g_timer_ms = 0;          // Скидаємо 1-секундний таймер (синхронізація)
            update_clock();          // Рухаємо годинник
        } 
    }
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
}
#endif

void setup() {
    setup_display_pins(); // з display_driver
    setup_hardware();
    #if ENABLE_KEYPAD
        keypad_init(); // з keypad_driver
    #endif
    setup_timer1_1ms(); 
    
    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.4) ---
    #if (ZVS_MODE!=0)
        // Встановлюємо g_zvs_present в 'false' за замовчуванням.
        MCUCR|=(1<<ISC01); MCUCR&=~(1<<ISC00); GIMSK|=(1<<INT0); g_zvs_watchdog_counter=0; g_zvs_present=false;
    #endif
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
    
    reset_to_idle(); 
    
    sei();
}

void loop() {
    static char s_lps=0; static uint16_t s_lht=0; char cks=0;
    static bool s_last_door_state = false;

    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.4 - Правильна Архітектура ISR/loop) ---
    // (run_1ms_tasks() видалено, код повернуто в ISR)
    
    // Обробляємо 1-секундні завдання (таймери, ZVS) з головного циклу
    if (g_1sec_tick_flag) {
        g_1sec_tick_flag = false; // Скидаємо прапор
        run_1sec_tasks();         // Виконуємо 1-секундні задачі
    }
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---

    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.3 - Баг 2-го етапу) ---
    // Обробник переходу між етапами (викликається з головного циклу, не з ISR)
    if (g_state == STATE_STAGE2_TRANSITION) {
        // Чекаємо, доки не мине 2 секунди з моменту вимкнення магнетрона
        uint32_t elapsed_off_time = g_millis_counter - g_magnetron_last_off_timestamp_ms;
        
        // Додаємо +100мс на випадок дрібних похибок таймінгу
        if (g_magnetron_last_off_timestamp_ms == 0 || elapsed_off_time > ((uint32_t)MIN_SAFE_OFF_TIME_SEC * 1000UL + 100UL)) 
        {
            // 2 секунди пройшло. Безпечно запускаємо 2-й етап.
            g_cook_time_total_sec = g_stage2_time_sec; 
            g_cook_power_level = g_stage2_power; 
            g_stage2_time_sec = 0; // Очищуємо прапор
            
            // Викликаємо start_cooking_cycle() з головного циклу - це безпечно!
            start_cooking_cycle(); 
        }
        // Якщо 2 секунди ще не пройшло, ми просто залишимося в цьому стані 
        // і перевіримо ще раз на наступній ітерації loop().
    }
    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
    
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
                // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.3 - Баг 2-го етапу) ---
                // Не показувати "DOOR" під час переходу між етапами
                else if (g_state != STATE_LOCKED && g_state != STATE_STAGE2_TRANSITION) { g_door_overlay_timer_ms = 2000; }
                // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
            } else { 
                if (g_door_open_during_pause) g_door_open_during_pause = false;
                if (g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms = 0;
            }
            s_last_door_state = door_is_open;
        }

        // --- Логіка обробки кнопок ---
        bool allow_keys=true;
        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.3 - Баг 2-го етапу) ---
        if(g_state==STATE_LOCKED || g_state==STATE_CLOCK_SAVED || g_door_overlay_timer_ms > 0 || g_state == STATE_STAGE2_TRANSITION) 
            allow_keys=false;
        // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
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

                    // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.9 - Оптимізація пам'яті) ---
                    // Видалено перерахунок ШІМ при відпусканні "Більше"/"Менше"
                    // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
                }
                s_lps=cks; 
            }
            // --- Кінець блоку ---

        } else { s_lps=0; s_lht=0; }
        
        // --- Обробка стану "Завершено" ---
        if(g_state==STATE_FINISHED || g_state==STATE_POST_COOK) handle_state_machine(0, false);
        
        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.2 - Архітектура ISR/loop) ---
        // Оновлення дисплея тепер НЕ в ISR, а в головному циклі,
        // але воно все ще керується 1мс-тіком (всередині run_display_multiplex)
        if (g_state != STATE_SLEEPING) {
            update_display(); 
        }
        // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
    }
}

int main(void) { 
    setup(); 
    while(1) { 
        loop(); 
    } 
    return 0; 
}