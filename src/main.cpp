#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <string.h> // Для memcpy_P

// ============================================================================
// --- 🔴 ГОЛОВНА КОНФІГУРАЦІЯ ---
// ============================================================================

#define ZVS_MODE 1 
#define ENABLE_KEYPAD 1 

// ============================================================================
// --- 🟡 ТИПИ ДАНИХ (ENUMS & STRUCTS) v1.7.1 ---
// ============================================================================

typedef enum {
    COLON_OFF = 0,
    COLON_ON = 1,
    COLON_BLINK_SLOW = 2,
    COLON_BLINK_FAST = 3,
    COLON_BLINK_SUPERFAST=4
} ColonDisplayMode;

typedef enum {
    STATE_IDLE, STATE_SLEEPING, STATE_SET_CLOCK_MODE, STATE_SET_CLOCK_TIME,
    STATE_CLOCK_SAVED, STATE_SET_TIME, STATE_SET_POWER, STATE_TWO_STAGE_1,
    STATE_TWO_STAGE_2, STATE_SET_AUTO_COOK, STATE_SET_AUTO_DEFROST,
    STATE_SET_WEIGHT, STATE_QUICK_START_PREP, STATE_COOKING, STATE_PAUSED,
    STATE_FLIP_PAUSE, 
    STATE_FINISHED, STATE_POST_COOK, STATE_LOCKED 
    // STATE_ADC_DEBUG видалено v1.7.1
} AppState_t;

typedef enum {
    PROGRAM_NONE,
    PROGRAM_COOK,
    PROGRAM_DEFROST
} AutoProgramType;

typedef struct {
    uint16_t weight_g; uint16_t time_sec; uint8_t power_level; bool add_beep;
} AutoProgramEntry;

typedef struct {
    uint8_t portb_mask; uint8_t portc_mask;
} char_pattern_t;

// --- Структури для логіки перевертання (v1.7.0) ---
typedef struct {
  uint16_t weight_g;
  uint8_t num_flips;
  uint8_t flip_percentages[5];
} FlipSchedule;

volatile struct {
  uint8_t num_flips_total;
  uint16_t flip_times_sec[5]; // Час, що ЗАЛИШИВСЯ
  uint8_t next_flip_index;
} g_defrost_flip_info;

// Додайте ці два рядки:
void reset_to_idle();
void handle_state_machine(char key, bool allow_beep); 

// ============================================================================
// --- 🥩 ТАБЛИЦІ ПЕРЕВЕРТАННЯ (PROGMEM) v1.7.0 ---
// ============================================================================
const FlipSchedule def_meat_flips[] PROGMEM = {
  {300,  1, {50, 0, 0, 0, 0}}, {500,  2, {33, 67, 0, 0, 0}}, {750,  2, {33, 67, 0, 0, 0}},
  {1000, 3, {25, 50, 75, 0, 0}}, {1500, 3, {25, 50, 75, 0, 0}}, {2000, 4, {20, 40, 60, 80, 0}},
  {4000, 5, {20, 40, 60, 80, 99}} // Використовуємо 5 слотів
};
const FlipSchedule def_poultry_flips[] PROGMEM = {
  {300,  1, {50, 0, 0, 0, 0}}, {500,  2, {33, 67, 0, 0, 0}}, {750,  2, {33, 67, 0, 0, 0}},
  {1000, 3, {25, 50, 75, 0, 0}}, {1500, 3, {25, 50, 75, 0, 0}}, {2000, 4, {20, 40, 60, 80, 0}},
  {4000, 5, {20, 40, 60, 80, 99}}
};
const FlipSchedule def_fish_flips[] PROGMEM = {
  {300,  1, {50, 0, 0, 0, 0}}, {400,  1, {50, 0, 0, 0, 0}}, {600,  2, {40, 80, 0, 0, 0}},
  {750,  2, {40, 80, 0, 0, 0}}, {1000, 2, {33, 67, 0, 0, 0}}
};

// ============================================================================
// --- 1. ТАБЛИЦІ АВТО-ПРОГРАМ (PROGMEM) ---
// ============================================================================
const AutoProgramEntry PROGMEM ac1_potato[] = { { 100, 120, 0, false }, { 200, 210, 0, false }, { 400, 360, 0, false }, { 600, 510, 0, false }, { 800, 660, 0, false }, { 1000, 780, 0, false } };
const AutoProgramEntry PROGMEM ac2_fresh_veg[] = { { 100, 90, 0, false }, { 200, 180, 0, false }, { 400, 300, 0, false }, { 600, 420, 0, false }, { 800, 540, 0, false } };
const AutoProgramEntry PROGMEM ac3_frozen_veg[] = { { 100, 120, 0, false }, { 200, 240, 0, false }, { 400, 420, 0, false }, { 600, 570, 0, false }, { 800, 720, 0, false } };
const AutoProgramEntry PROGMEM def1_meat[] = { { 100, 120, 3, true }, { 500, 600, 3, true }, { 1000, 1260, 3, true }, { 2000, 2760, 3, true }, { 3000, 4500, 3, true }, { 4000, 5999, 3, true } };
const AutoProgramEntry PROGMEM def2_poultry[] = { { 100, 120, 3, true }, { 500, 570, 3, true }, { 1000, 1200, 3, true }, { 2000, 2640, 3, true }, { 3000, 4320, 3, true }, { 4000, 5999, 3, true } };
const AutoProgramEntry PROGMEM def3_fish[] = { { 100, 90, 3, true }, { 500, 420, 3, true }, { 1000, 900, 3, true }, { 2000, 1920, 3, true }, { 3000, 3000, 3, true }, { 4000, 4200, 3, true } };
const AutoProgramEntry PROGMEM def4_bread[] = { { 100, 40, 4, false }, { 200, 70, 4, false }, { 300, 100, 4, false }, { 400, 130, 4, false }, { 500, 150, 4, false } };

// ============================================================================
// --- 2. АПАРАТНІ ВИЗНАЧЕННЯ ---
// ============================================================================
#define ZVS_DDR DDRD
#define ZVS_PIN PIND
#define ZVS_BIT (1 << PD2)
#define CDD_DDR DDRD
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

#define TURNTABLE_DDR DDRD
#define TURNTABLE_PORT PORTD
#define TURNTABLE_BIT (1 << PD3)

#define LAMP_DDR DDRB
#define LAMP_PORT PORTB
#define LAMP_BIT (1 << PB5)


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

uint8_t g_display_buffer[4] = {0, 0, 0, 0};
const char_pattern_t number_map[] = { {0x05, 0x36}, {0x04, 0x02}, {0x03, 0x26}, {0x07, 0x06}, {0x06, 0x12}, {0x07, 0x14}, {0x07, 0x34}, {0x04, 0x06}, {0x07, 0x36}, {0x07, 0x16} };
const char_pattern_t symbol_map[] = { {0x00, 0x00}, {0x01, 0x30}, {0x03, 0x34}, {0x07, 0x22}, {0x07, 0x30}, {0x02, 0x34}, {0x06, 0x36}, {0x01, 0x34}, {0x06, 0x32}, {0x02, 0x20}, {0x07, 0x20}, {0x06, 0x20}, {0x02, 0x36}, {0x02, 0x00}, {0x03, 0x30} };
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
// #define CHAR_G 25 // 'G'
const uint8_t digit_portd_pins[] = {DIGIT1_CATHODE, DIGIT2_CATHODE, 0, 0};
const uint8_t digit_portb_pins[] = {0, 0, DIGIT3_CATHODE, DIGIT4_CATHODE};
const uint8_t logical_to_physical_index[] = {3, 1, 0, 2};

void setup_display_pins() {
    DDRB |= SEG_D | SEG_G | SEG_C | DIGIT3_CATHODE | DIGIT4_CATHODE; DDRC |= SEG_A | SEG_B | SEG_E | SEG_F | COLON_ANODE; DDRD |= DIGIT1_CATHODE | DIGIT2_CATHODE;
    PORTB &= ~(SEG_D | SEG_G | SEG_C | DIGIT3_CATHODE | DIGIT4_CATHODE); PORTC &= ~(SEG_A | SEG_B | SEG_E | SEG_F); PORTD &= ~(DIGIT1_CATHODE | DIGIT2_CATHODE);
    PORTC &= ~COLON_ANODE; 
}
void disable_all_digits() { PORTD &= ~(DIGIT1_CATHODE | DIGIT2_CATHODE); PORTB &= ~(DIGIT3_CATHODE | DIGIT4_CATHODE); }
void set_segments(uint8_t portb_mask, uint8_t portc_mask) { PORTB = (PORTB & ~(ALL_SEGMENTS_B)) | portb_mask; PORTC = (PORTC & ~(ALL_SEGMENTS_C)) | portc_mask; }
void display_symbol(uint8_t ld, uint8_t sym) {
    uint8_t pid = logical_to_physical_index[ld]; disable_all_digits();
    char_pattern_t p;
    if (sym < 10) { p = number_map[sym]; }
    // else if (sym == CHAR_G) { p.portb_mask=0x05; p.portc_mask=0x34; }
    else { p = symbol_map[sym-10]; }
    set_segments(p.portb_mask, p.portc_mask);
    if (digit_portd_pins[pid] != 0) PORTD |= digit_portd_pins[pid]; else PORTB |= digit_portb_pins[pid];
}


// ============================================================================
// --- 4. ГЛОБАЛЬНІ ЗМІННІ СТАНУ ---
// ============================================================================
volatile AppState_t g_state = STATE_IDLE;
volatile uint16_t g_timer_ms = 0;
// g_blink_counter видалено (v1.7.1)
volatile uint8_t g_colon_visible = 0;
volatile uint16_t g_colon_timer = 0;
volatile ColonDisplayMode g_colon_mode = COLON_OFF;
volatile uint16_t g_beep_ms_counter = 0; // v1.7.0: uint16_t (для 300/800мс)
volatile uint16_t g_beep_flip_sequence_timer = 0; // v1.7.0: Таймер для серії біпів перевороту
volatile uint16_t g_clock_save_burst_timer = 0;   // v1.7.0: Таймер для серії біпів збереження часу
volatile uint16_t g_flip_beep_timeout_ms = 0;     // v1.7.1: Виправлення (було пропущено)
volatile uint16_t g_key_3sec_hold_timer_ms = 0;
volatile uint16_t g_key_continuous_hold_ms = 0;
volatile uint16_t g_last_key_hold_duration = 0;
volatile char g_last_key_for_hold = 0;
volatile bool g_key_hold_3sec_flag = false;
volatile uint16_t g_quick_start_delay_ms = 0;
volatile uint32_t g_post_cook_timer_ms = 0;
volatile uint16_t g_clock_save_blink_ms = 0;
volatile bool g_door_open_during_pause = false;
volatile bool g_magnetron_request = false;
volatile bool g_adc_read_pending = false;
volatile bool g_zvs_present = false;
volatile uint8_t g_zvs_pulse_counter = 0;
volatile uint8_t g_zvs_watchdog_counter = 0;
uint8_t g_clock_hour = 0, g_clock_min = 0, g_clock_sec = 0;
bool g_clock_24hr_mode = true;
uint16_t g_cook_time_total_sec = 0, g_cook_original_total_time = 0;
uint8_t g_cook_power_level = 0;
uint16_t g_stage1_time_sec = 0; uint8_t g_stage1_power = 0;
uint16_t g_stage2_time_sec = 0; uint8_t g_stage2_power = 0;
uint8_t g_auto_program = 0; uint16_t g_auto_weight_grams = 0;
uint8_t g_input_min_tens=0, g_input_min_units=0, g_input_sec_tens=0;
uint8_t g_input_hour=0, g_input_min=0, g_input_sec=0;
bool g_is_defrost_mode = false;
volatile AutoProgramType g_active_auto_program_type = PROGRAM_NONE;
volatile uint16_t g_door_overlay_timer_ms = 0;
const uint16_t power_levels_watt[] = {700, 560, 420, 280, 140};
#define ADAPTIVE_PWM_THRESHOLD_SEC 30
// v1.7.0: Безпека магнетрона
#define MIN_SAFE_ON_TIME_SEC 5
#define MIN_SAFE_OFF_TIME_SEC 2
#define MIN_SAFE_CYCLE_SEC (MIN_SAFE_ON_TIME_SEC + MIN_SAFE_OFF_TIME_SEC) // 7s
#define MAGNETRON_COAST_TIME_SEC 10 // < 10s

volatile uint8_t g_pwm_cycle_duration = 10;
volatile uint8_t g_pwm_cycle_counter_seconds = 0;
uint8_t g_pwm_on_time_seconds = 0;


// ============================================================================
// --- 5. СЕКЦІЯ КНОПОК (ADC) ---
// ============================================================================
#define KEYPAD_ADC_CHANNEL 0
#define ADC_NOISE_THRESHOLD 1015
#define ADC_TOLERANCE 10
#define DEBOUNCE_TIME 3 
volatile uint16_t g_adc_value = 1023;
volatile char g_debounced_key_state = 0;
volatile char g_key_last_state = 0;
volatile uint8_t g_debounce_counter = 0;
#define KEY_10_MIN '1'
#define KEY_1_MIN '4'
#define KEY_10_SEC '7'
#define KEY_1_SEC '8'
#define KEY_AUTO_COOK '2'
#define KEY_AUTO_DEFROST '5'
#define KEY_QUICK_DEFROST '8'
#define KEY_MICRO '3' 
#define KEY_CLOCK '6'
#define KEY_MORE '9'
#define KEY_LESS 'A'
#define KEY_STOP_RESET 'B'
#define KEY_START_QUICKSTART 'C'
const uint16_t adc_key_values[] = {1003, 964, 926, 887, 848, 820, 744, 606, 539, 494, 395, 350};
const char adc_key_map[] = { KEY_AUTO_COOK, KEY_AUTO_DEFROST, KEY_QUICK_DEFROST, KEY_10_SEC, KEY_1_MIN, KEY_10_MIN, KEY_LESS, KEY_MORE, KEY_MICRO, KEY_START_QUICKSTART, KEY_STOP_RESET, KEY_CLOCK };
void keypad_init() { ADMUX = (1<<REFS0)|(KEYPAD_ADC_CHANNEL&0x07); ADCSRA=(1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); }
char get_key_press() { cli(); char key = g_debounced_key_state; sei(); return key;
}


// ============================================================================
// --- 4.5 ФУНКЦІЇ КЕРУВАННЯ ДВОКРАПКОЮ ---
// ============================================================================
void set_colon_mode(ColonDisplayMode mode) {
    if (g_colon_mode != mode) { 
        g_colon_mode = mode;
        g_colon_timer = 0;
    }
}

void update_colon_state() {
    g_colon_timer++;
    switch (g_colon_mode) {
        case COLON_OFF: g_colon_visible = 0; break;
        case COLON_ON: g_colon_visible = 1; break;
        case COLON_BLINK_SLOW:
            if (g_colon_timer < 500) g_colon_visible = 1; else if (g_colon_timer < 1000) g_colon_visible = 0; else g_colon_timer = 0;
            break;
        case COLON_BLINK_FAST:
            if (g_colon_timer < 200) g_colon_visible = 1; else if (g_colon_timer < 400) g_colon_visible = 0; else g_colon_timer = 0;
            break;
        case COLON_BLINK_SUPERFAST:
            if (g_colon_timer < 166) g_colon_visible = 1; else if (g_colon_timer < 333) g_colon_visible = 0; else g_colon_timer = 0;
            break;
    }
}

void run_display_multiplex() {
    static uint8_t current_display_digit = 0;
    if (g_colon_visible) PORTC |= COLON_ANODE; else PORTC &= ~COLON_ANODE;
    switch(current_display_digit) {
        case 0: display_symbol(0, g_display_buffer[0]); break;
        case 1: display_symbol(1, g_display_buffer[1]); break;
        case 2: display_symbol(2, g_display_buffer[2]); break;
        case 3: display_symbol(3, g_display_buffer[3]); break;
    }
    current_display_digit++; if (current_display_digit >= 4) current_display_digit = 0;
}


// ============================================================================
// --- 6. АПАРАТНІ ФУНКЦІЇ ---
// ============================================================================
void set_magnetron(bool on) {
    #if (ZVS_MODE == 0)
        if (on) MAGNETRON_PORT |= MAGNETRON_BIT; else MAGNETRON_PORT &= ~MAGNETRON_BIT;
    #else
        if (on) g_magnetron_request = true; else { g_magnetron_request = false; MAGNETRON_PORT &= ~MAGNETRON_BIT; }
    #endif
}
void set_fan(bool on) { if (on) FAN_PORT |= FAN_BIT; else FAN_PORT &= ~FAN_BIT; }
void set_turntable(bool on) { if (on) TURNTABLE_PORT |= TURNTABLE_BIT; else TURNTABLE_PORT &= ~TURNTABLE_BIT; }
void set_lamp(bool on) { if (on) LAMP_PORT |= LAMP_BIT; else LAMP_PORT &= ~LAMP_BIT; }

void do_short_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 300; } // 300мс
void do_long_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 800; } // 800мс
void do_flip_beep() { if (g_beep_flip_sequence_timer == 0) g_beep_flip_sequence_timer = 1; } // Запустити серію

void setup_hardware() {
    DDRD &= ~ZVS_BIT; CDD_DDR &= ~CDD_BIT; PORTD |= CDD_BIT;
    MAGNETRON_DDR |= MAGNETRON_BIT; MAGNETRON_PORT &= ~MAGNETRON_BIT;
    FAN_DDR |= FAN_BIT; FAN_PORT &= ~FAN_BIT;
    BEEPER_DDR |= BEEPER_BIT; BEEPER_PORT &= ~BEEPER_BIT;
    TURNTABLE_DDR |= TURNTABLE_BIT; TURNTABLE_PORT &= ~TURNTABLE_BIT;
    LAMP_DDR |= LAMP_BIT; LAMP_PORT &= ~LAMP_BIT;
}


// ============================================================================
// --- 6. ФУНКЦІЇ ОНОВЛЕННЯ СТАНУ ---
// ============================================================================

void calculate_pwm_on_time() {
    if (g_cook_power_level == 0) { 
        g_pwm_on_time_seconds = g_pwm_cycle_duration;
        return; 
    }
    uint16_t watts = power_levels_watt[g_cook_power_level];
    uint16_t on_time = (uint16_t)(((uint32_t)watts * g_pwm_cycle_duration) / 700);
    
    if (on_time < MIN_SAFE_ON_TIME_SEC) on_time = MIN_SAFE_ON_TIME_SEC;
    if (on_time > (g_pwm_cycle_duration - MIN_SAFE_OFF_TIME_SEC)) {
        on_time = g_pwm_cycle_duration - MIN_SAFE_OFF_TIME_SEC;
    }
    g_pwm_on_time_seconds = (uint8_t)on_time;
}
void recalculate_adaptive_pwm() {
    if (g_cook_original_total_time < MAGNETRON_COAST_TIME_SEC) {
        g_pwm_cycle_duration = g_cook_original_total_time;
        g_cook_power_level = 0; 
    } else if (g_cook_original_total_time < ADAPTIVE_PWM_THRESHOLD_SEC) {
        g_pwm_cycle_duration = g_cook_original_total_time;
    } else {
        g_pwm_cycle_duration = 30;
    }
    calculate_pwm_on_time();
}

bool start_cooking_cycle() {
    if (CDD_PIN & CDD_BIT) { do_short_beep(); reset_to_idle(); return false; }
    set_fan(true);
    g_cook_original_total_time = g_cook_time_total_sec; 
    recalculate_adaptive_pwm(); 
    g_pwm_cycle_counter_seconds = 0;
    set_turntable(true); set_lamp(true); 
    
    if (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) {
        set_magnetron(false);
    } else {
        set_magnetron(true);
    }
    
    g_state = STATE_COOKING; 
    set_colon_mode(COLON_ON); 
    return true;
}

void resume_cooking() {
    if (!(CDD_PIN & CDD_BIT)) { 
        g_state=STATE_COOKING; 
        set_colon_mode(COLON_ON); 
        set_turntable(true); 
        set_fan(true); 
        if (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) {
            set_magnetron(false);
        } else {
            if(g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds) set_magnetron(true); else set_magnetron(false);
        }
    }
}
void update_clock() {
    g_clock_sec++; if(g_clock_sec>=60) { g_clock_sec=0; g_clock_min++; if(g_clock_min>=60) { g_clock_min=0; g_clock_hour++; if(g_clock_hour>=24) g_clock_hour=0;
    } }
}

void calculate_flip_schedule(uint8_t program_num, uint16_t weight) {
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    const FlipSchedule* flip_table = NULL;
    uint8_t table_len = 0;

    if (program_num == 1) { // М'ясо (та Швидка розморозка)
        flip_table = def_meat_flips; table_len = sizeof(def_meat_flips) / sizeof(FlipSchedule);
    } else if (program_num == 2) { // Птиця
        flip_table = def_poultry_flips; table_len = sizeof(def_poultry_flips) / sizeof(FlipSchedule);
    } else if (program_num == 3) { // Риба
        flip_table = def_fish_flips; table_len = sizeof(def_fish_flips) / sizeof(FlipSchedule);
    } else {
        return; // Немає графіка (напр. Хліб, AC)
    }

    FlipSchedule sched;
    for (uint8_t i = 0; i < table_len; i++) {
        memcpy_P(&sched, &flip_table[i], sizeof(FlipSchedule));
        if (weight <= sched.weight_g) break; 
    }
  
    g_defrost_flip_info.num_flips_total = sched.num_flips;
    uint16_t total_time = g_cook_time_total_sec;
  
    for (uint8_t i = 0; i < sched.num_flips; i++) {
        uint8_t perc = sched.flip_percentages[i];
        if (perc > 0 && perc < 100) {
            // Розрахунок часу, що ЗАЛИШИВСЯ
            g_defrost_flip_info.flip_times_sec[i] = total_time - (((uint32_t)total_time * perc) / 100);
        }
    }
}

void initiate_flip_pause() {
    set_magnetron(false);
    set_turntable(false);
    g_state = STATE_FLIP_PAUSE;
    g_flip_beep_timeout_ms = 5000; // 5-секундний таймер для біпів
    do_flip_beep(); // Запустити серію
}

void resume_after_flip() {
    g_defrost_flip_info.next_flip_index++;
    g_state = STATE_COOKING;
    set_turntable(true);
    if (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) {
        set_magnetron(false);
    } else {
        set_magnetron(true);
    }
}

void check_flip_required() {
    if (g_active_auto_program_type != PROGRAM_DEFROST || g_defrost_flip_info.next_flip_index >= g_defrost_flip_info.num_flips_total) {
        return;
    }
    uint16_t next_flip_time = g_defrost_flip_info.flip_times_sec[g_defrost_flip_info.next_flip_index];
    if (next_flip_time > 0 && g_cook_time_total_sec == next_flip_time) {
        initiate_flip_pause();
    }
}

void update_cook_timer() {
    if (g_state == STATE_COOKING && g_cook_time_total_sec > 0) {
        if (g_cook_time_total_sec <= 3) do_long_beep();
        
        g_cook_time_total_sec--;
        
        check_flip_required();
        
        if (g_cook_time_total_sec < MAGNETRON_COAST_TIME_SEC) {
            set_magnetron(false);
        } else {
             if (g_pwm_cycle_counter_seconds < g_pwm_on_time_seconds) set_magnetron(true); else set_magnetron(false);
        }
       
        g_pwm_cycle_counter_seconds++;
        if (g_pwm_cycle_counter_seconds >= g_pwm_cycle_duration) g_pwm_cycle_counter_seconds = 0;
        
        if (g_cook_time_total_sec == 0) {
            if (g_stage2_time_sec > 0) { 
                g_cook_time_total_sec=g_stage2_time_sec;
                g_cook_power_level=g_stage2_power; 
                start_cooking_cycle(); 
                g_stage2_time_sec=0; 
                do_short_beep();
            }
            else { 
                g_state=STATE_FINISHED; 
                set_colon_mode(COLON_OFF);
                g_post_cook_timer_ms=0; 
                set_magnetron(false); 
                set_turntable(false); 
                set_fan(false); 
            }
        }
    }
}
void get_program_settings(const AutoProgramEntry* table, uint8_t len, uint16_t weight) {
    AutoProgramEntry entry;
    for (uint8_t i=0; i<len; i++) { memcpy_P(&entry, &table[i], sizeof(AutoProgramEntry)); if (i==(len-1) || weight<pgm_read_word(&table[i+1].weight_g)) break; }
    g_cook_time_total_sec=entry.time_sec; g_cook_power_level=entry.power_level;
}


// ============================================================================
// --- 7. ФУНКЦІЇ ОНОВЛЕННЯ ДИСПЛЕЯ ---
// ============================================================================

void set_display(uint8_t vis1, uint8_t vis2, uint8_t vis3, uint8_t vis4) { g_display_buffer[3]=vis1;
g_display_buffer[2]=vis2; g_display_buffer[1]=vis3; g_display_buffer[0]=vis4; }
void display_time_suppressed(uint16_t total_seconds) {
    uint8_t min=total_seconds/60, sec=total_seconds%60, d1=min/10, d2=min%10;
    if (d1==0) { d1=CHAR_SPACE;
    if(d2==0) d2=CHAR_SPACE; }
    set_display(d1, d2, sec/10, sec%10);
}
void display_clock(uint8_t h, uint8_t m) {
    if (!g_clock_24hr_mode && h==0) h=12; if (!g_clock_24hr_mode && h>12) h-=12;
    set_display(h/10, h%10, m/10, m%10);
}

void update_display() {
    if (g_door_overlay_timer_ms > 0) {
        set_display(CHAR_D, CHAR_O, CHAR_O, CHAR_R);
        set_colon_mode(COLON_OFF);
        return;
    }

    switch (g_state) {
        case STATE_IDLE: 
            display_clock(g_clock_hour, g_clock_min);
            set_colon_mode(COLON_BLINK_SLOW);
            break;
        case STATE_PAUSED:
        case STATE_FLIP_PAUSE: {
            bool show_alt = (g_timer_ms / 1000) % 2 == 0; // 1с / 1с
            if (g_door_open_during_pause) {
                if (show_alt) { set_display(CHAR_D, CHAR_O, CHAR_O, CHAR_R); set_colon_mode(COLON_OFF); }
                else { display_time_suppressed(g_cook_time_total_sec); set_colon_mode(COLON_ON); }
            } else if (g_state == STATE_FLIP_PAUSE) {
                if (show_alt) { set_display(CHAR_D, CHAR_E, CHAR_F, g_auto_program); set_colon_mode(COLON_OFF); }
                else { display_time_suppressed(g_cook_time_total_sec); set_colon_mode(COLON_ON); }
            } else if (g_active_auto_program_type != PROGRAM_NONE) { 
                if (show_alt) {
                    if (g_active_auto_program_type == PROGRAM_COOK) set_display(CHAR_A, CHAR_C, CHAR_DASH, g_auto_program);
                    else set_display(CHAR_D, CHAR_E, CHAR_F, g_auto_program);
                    set_colon_mode(COLON_OFF);
                } else {
                    display_time_suppressed(g_cook_time_total_sec); set_colon_mode(COLON_ON);
                }
            } else {
                display_time_suppressed(g_cook_time_total_sec);
                set_colon_mode(COLON_ON);
            }
            } break;
        case STATE_SLEEPING: disable_all_digits(); break;
        case STATE_SET_CLOCK_MODE: set_colon_mode(COLON_OFF); if(g_clock_24hr_mode) set_display(CHAR_SPACE, CHAR_SPACE, 2, 4); else set_display(CHAR_SPACE, CHAR_SPACE, 1, 2); break;
        case STATE_SET_CLOCK_TIME: set_colon_mode(COLON_ON); display_clock(g_input_hour, g_input_min); break;
        case STATE_CLOCK_SAVED: set_colon_mode(COLON_BLINK_SUPERFAST); display_clock(g_clock_hour, g_clock_min); break;
        case STATE_SET_TIME: set_colon_mode(COLON_ON); set_display(g_input_min_tens, g_input_min_units, g_input_sec_tens, 0); break;
        case STATE_SET_POWER: { set_colon_mode(COLON_OFF); uint16_t w=power_levels_watt[g_cook_power_level];
        set_display(CHAR_P, (w/100)%10, (w/10)%10, w%10); } break;
        case STATE_SET_AUTO_COOK: set_colon_mode(COLON_OFF); set_display(CHAR_A, CHAR_C, CHAR_DASH, g_auto_program); break;
        case STATE_SET_AUTO_DEFROST: set_colon_mode(COLON_OFF); set_display(CHAR_D, CHAR_E, CHAR_F, g_auto_program); break;
        case STATE_SET_WEIGHT: { set_colon_mode(COLON_OFF); uint8_t d1,d2; if(g_auto_weight_grams>=1000) { d1=(g_auto_weight_grams/1000)%10; d2=(g_auto_weight_grams/100)%10; } else { d1=CHAR_SPACE; d2=(g_auto_weight_grams/100)%10; } set_display(d1,d2,(g_auto_weight_grams/10)%10,g_auto_weight_grams%10); } break;
        case STATE_QUICK_START_PREP: set_colon_mode(COLON_ON); display_time_suppressed(g_cook_time_total_sec); break;
        // STATE_ADC_DEBUG видалено v1.7.1
        case STATE_LOCKED: set_colon_mode(COLON_OFF); set_display(CHAR_SPACE, CHAR_SPACE, CHAR_SPACE, CHAR_L); break;
        case STATE_COOKING: 
            set_colon_mode(COLON_ON);
            if(g_cook_time_total_sec <= 5) {
                if ((g_timer_ms / 750) % 2 == 0) set_display(CHAR_E, CHAR_N, CHAR_D, CHAR_DASH);
                else display_time_suppressed(g_cook_time_total_sec); 
            } else {
                display_time_suppressed(g_cook_time_total_sec);
            }
            break;
        case STATE_FINISHED: case STATE_POST_COOK: set_colon_mode(COLON_OFF); set_display(CHAR_E, CHAR_N, CHAR_D, CHAR_DASH);
        break;
        default: set_colon_mode(COLON_OFF); set_display(CHAR_DASH, CHAR_DASH, CHAR_DASH, CHAR_DASH); break;
    }
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
    g_door_open_during_pause=false; set_magnetron(false); set_turntable(false); set_fan(false);
    set_lamp(false);
    g_active_auto_program_type = PROGRAM_NONE;
    g_door_overlay_timer_ms = 0;
    memset((void*)&g_defrost_flip_info, 0, sizeof(g_defrost_flip_info));
    g_beep_flip_sequence_timer = 0;
    g_clock_save_burst_timer = 0;
    g_flip_beep_timeout_ms = 0;
}

void handle_key_hold_increment(char key, uint16_t hold_duration, uint16_t* last_trigger_ms) {
    uint16_t interval = 0;
    switch (g_state) {
        case STATE_SET_TIME: case STATE_SET_CLOCK_TIME: case STATE_COOKING:
            if (hold_duration > 3000) interval = 50;
            else if (hold_duration > 1500) interval = 100;
            else interval = 200;
            break;
        case STATE_SET_WEIGHT:
            if (hold_duration > 2000) interval = 100;
            else interval = 200;
            break;
        default: return;
    }
    if ((uint16_t)(hold_duration - *last_trigger_ms) >= interval) {
         handle_state_machine(key, false); // v1.7.0: allow_beep = false
         *last_trigger_ms = hold_duration; 
    }
}

void handle_time_input_odometer(char key) {
    // v1.7.1: Оптимізація (було uint32_t)
    uint16_t ts = (g_input_min_tens*10 + g_input_min_units)*60 + (g_input_sec_tens*10);
    if (ts >= 5990) { g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; return; }
    if (key == KEY_10_SEC) {
        g_input_sec_tens++;
        if (g_input_sec_tens > 5) { g_input_sec_tens = 0; g_input_min_units++;
            if (g_input_min_units > 9) { g_input_min_units = 0; g_input_min_tens++;
            if (g_input_min_tens > 9) { g_input_min_tens=9; g_input_min_units=9; g_input_sec_tens=5; }
            }
        }
    } else if (key == KEY_1_MIN) {
        g_input_min_units++;
        if (g_input_min_units > 9) { g_input_min_units = 0; g_input_min_tens++;
            if (g_input_min_tens > 9) g_input_min_tens = 0;
        }
    } else if (key == KEY_10_MIN) {
        g_input_min_tens++;
        if (g_input_min_tens > 9) g_input_min_tens = 0;
    }
}
void handle_clock_input(char key) {
    // Секунди більше не встановлюються, тому скидаємо їх в 0
    g_input_sec = 0; 

    if (key == KEY_10_MIN) { 
        // '1' -> Керує годинами (0-23)
        g_input_hour++;
        if (g_input_hour >= 24) {
            g_input_hour = 0;
        }
    }
    else if (key == KEY_1_MIN) { 
        // '4' -> Керує ДЕСЯТКАМИ хвилин (0-5)
        uint8_t ones = g_input_min % 10; // Зберігаємо одиниці
        uint8_t tens = (g_input_min / 10);
        
        tens++; // Збільшуємо десятки
        if (tens >= 6) { // Цикл 0..5
            tens = 0;
        }
        g_input_min = (tens * 10) + ones; // Збираємо хвилини назад
    }
    else if (key == KEY_10_SEC) { 
        // '7' -> Керує ОДИНИЦЯМИ хвилин (0-9)
        uint8_t tens = (g_input_min / 10); // Зберігаємо десятки
        uint8_t ones = g_input_min % 10;
        
        ones++; // Збільшуємо одиниці
        if (ones >= 10) { // Цикл 0..9
            ones = 0;
        }
        g_input_min = (tens * 10) + ones; // Збираємо хвилини назад
    }
}

void handle_state_machine(char key, bool allow_beep) {
    if (g_state == STATE_SLEEPING) return;

    if (key == KEY_START_QUICKSTART && g_state == STATE_IDLE) {
        g_cook_time_total_sec = 30; g_quick_start_delay_ms = 1000;
        g_state = STATE_QUICK_START_PREP; g_active_auto_program_type = PROGRAM_NONE;
        if (allow_beep) do_short_beep(); return;
    }
    if (key == KEY_START_QUICKSTART && g_state == STATE_COOKING) {
        if (g_cook_time_total_sec <= (5999 - 30)) { g_cook_time_total_sec += 30; g_cook_original_total_time += 30; }
        else { g_cook_time_total_sec = 5999; g_cook_original_total_time = 5999; }
        recalculate_adaptive_pwm();
        if (allow_beep) do_short_beep(); return;
    }
    if (g_state == STATE_COOKING) {
        if (key == KEY_MORE) { if (g_cook_time_total_sec <= (5999 - 10)) { g_cook_time_total_sec += 10; g_cook_original_total_time += 10; recalculate_adaptive_pwm(); if (allow_beep) do_short_beep(); } return; }
        if (key == KEY_LESS) { if (g_cook_time_total_sec >= 10) { g_cook_time_total_sec -= 10; g_cook_original_total_time -= 10; recalculate_adaptive_pwm(); if (allow_beep) do_short_beep(); } return; }
    }
    
    switch (g_state) {
        case STATE_IDLE:
            if (key == KEY_CLOCK) { g_state = STATE_SET_CLOCK_MODE; g_clock_24hr_mode = true; if (allow_beep) do_short_beep(); }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { g_input_min_tens=0;
            g_input_min_units=0; g_input_sec_tens=0; handle_time_input_odometer(key); g_state = STATE_SET_TIME; if (allow_beep) do_short_beep(); }
            else if (key == KEY_MICRO) { g_cook_power_level = 0; g_state = STATE_SET_POWER; if (allow_beep) do_short_beep(); }
            else if (key == KEY_AUTO_COOK) { g_auto_program = 1; g_state = STATE_SET_AUTO_COOK; if (allow_beep) do_short_beep(); }
            else if (key == KEY_AUTO_DEFROST) { g_auto_program = 1; g_state = STATE_SET_AUTO_DEFROST; if (allow_beep) do_short_beep(); }
            else if (key == KEY_QUICK_DEFROST) { 
                g_active_auto_program_type = PROGRAM_DEFROST; g_auto_program = 1;
                get_program_settings(def1_meat, (sizeof(def1_meat)/sizeof(AutoProgramEntry)), 500);
                calculate_flip_schedule(1, 500); 
                start_cooking_cycle(); if (allow_beep) do_short_beep();
            }
            break;
        case STATE_SLEEPING: break;
        case STATE_SET_CLOCK_MODE:
            if (key == KEY_CLOCK) { g_clock_24hr_mode = !g_clock_24hr_mode; if (allow_beep) do_short_beep(); }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { g_input_hour=g_clock_hour; g_input_min=g_clock_min; g_input_sec=g_clock_sec; g_state = STATE_SET_CLOCK_TIME; handle_clock_input(key); if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_CLOCK_TIME:
            if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { handle_clock_input(key); if (allow_beep) do_short_beep(); }
            else if (key == KEY_CLOCK) { g_clock_hour=g_input_hour; g_clock_min=g_input_min; g_clock_sec=g_input_sec; g_clock_save_blink_ms=2000; g_state=STATE_CLOCK_SAVED; g_clock_save_burst_timer = 800; } // Біп при збереженні
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_CLOCK_SAVED: break;
        case STATE_SET_TIME:
            if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { handle_time_input_odometer(key); if (allow_beep) do_short_beep(); }
            else if (key == KEY_START_QUICKSTART) { 
                g_cook_time_total_sec = (g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10); 
                g_active_auto_program_type = PROGRAM_NONE; 
                if(g_cook_time_total_sec>0) { start_cooking_cycle(); if (allow_beep) do_short_beep(); } else reset_to_idle();
            }
            else if (key == KEY_MICRO) { g_cook_time_total_sec = (g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
            g_stage1_time_sec=g_cook_time_total_sec; g_stage1_power=g_cook_power_level; g_input_min_tens=0; g_input_min_units=0; g_input_sec_tens=0; g_cook_power_level=0; g_state=STATE_SET_POWER; if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_POWER:
            if (key == KEY_MICRO) { g_cook_power_level++; if(g_cook_power_level>=5) g_cook_power_level=0; if (allow_beep) do_short_beep(); }
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { handle_time_input_odometer(key); g_state=STATE_SET_TIME; if (allow_beep) do_short_beep(); }
            else if (key == KEY_START_QUICKSTART) { 
                g_cook_time_total_sec=(g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
                g_stage2_time_sec=g_cook_time_total_sec; g_stage2_power=g_cook_power_level; g_cook_time_total_sec=g_stage1_time_sec; g_cook_power_level=g_stage1_power; 
                g_active_auto_program_type = PROGRAM_NONE; 
                if(g_cook_time_total_sec>0) { start_cooking_cycle(); if (allow_beep) do_short_beep(); } else reset_to_idle(); 
            }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_AUTO_COOK:
            g_is_defrost_mode = false;
            if (key == KEY_AUTO_COOK) { g_auto_program++; if(g_auto_program>3) g_auto_program=1; if (allow_beep) do_short_beep(); }
            else if (key==KEY_MORE || key==KEY_LESS) { g_auto_weight_grams=100; if(g_auto_program>1) g_auto_weight_grams=100; g_state=STATE_SET_WEIGHT; if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_SET_AUTO_DEFROST:
             g_is_defrost_mode = true;
             if (key == KEY_AUTO_DEFROST) { g_auto_program++; if(g_auto_program>4) g_auto_program=1; if (allow_beep) do_short_beep(); }
             else if (key==KEY_MORE || key==KEY_LESS) { g_auto_weight_grams=100; g_state=STATE_SET_WEIGHT; if (allow_beep) do_short_beep(); }
             else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
             break;
        case STATE_SET_WEIGHT: {
            uint16_t min_w=100, max_w=1000;
            if (g_is_defrost_mode) { if(g_auto_program==4) max_w=500; else max_w=4000; }
            else { if(g_auto_program>1) { min_w=100; max_w=800; } }
            if (g_auto_weight_grams < min_w) g_auto_weight_grams = min_w;
            if (key == KEY_MORE) { g_auto_weight_grams+=100; if(g_auto_weight_grams>max_w) g_auto_weight_grams=min_w; if (allow_beep) do_short_beep(); }
            else if (key == KEY_LESS) { if(g_auto_weight_grams>min_w) g_auto_weight_grams-=100; else g_auto_weight_grams=max_w; if (allow_beep) do_short_beep(); }
            else if (key == KEY_START_QUICKSTART) {
                if (g_is_defrost_mode) {
                    g_active_auto_program_type = PROGRAM_DEFROST; 
                    switch (g_auto_program) {
                        case 1: get_program_settings(def1_meat, 6, g_auto_weight_grams); calculate_flip_schedule(1, g_auto_weight_grams); break;
                        case 2: get_program_settings(def2_poultry, 6, g_auto_weight_grams); calculate_flip_schedule(2, g_auto_weight_grams); break;
                        case 3: get_program_settings(def3_fish, 6, g_auto_weight_grams); calculate_flip_schedule(3, g_auto_weight_grams); break;
                        case 4: get_program_settings(def4_bread, 5, g_auto_weight_grams); break;
                    }
                } else {
                    g_active_auto_program_type = PROGRAM_COOK; 
                    switch (g_auto_program) {
                        case 1: get_program_settings(ac1_potato, 6, g_auto_weight_grams); break;
                        case 2: get_program_settings(ac2_fresh_veg, 5, g_auto_weight_grams); break;
                        case 3: get_program_settings(ac3_frozen_veg, 5, g_auto_weight_grams); break;
                    }
                }
                if(g_cook_time_total_sec>0) { start_cooking_cycle(); if (allow_beep) do_short_beep(); }
                else reset_to_idle();
            }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
        } break;
        case STATE_QUICK_START_PREP:
            if (key == KEY_START_QUICKSTART) { g_cook_time_total_sec+=30; if(g_cook_time_total_sec>5999) g_cook_time_total_sec=30; g_quick_start_delay_ms=1000; if (allow_beep) do_short_beep(); }
            else if (key == KEY_STOP_RESET) { reset_to_idle(); if (allow_beep) do_short_beep(); }
            break;
        case STATE_COOKING:
            if (key == KEY_STOP_RESET) { g_state=STATE_PAUSED; set_magnetron(false); set_turntable(false); if (allow_beep) do_short_beep(); }
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
        // STATE_ADC_DEBUG видалено v1.7.1
        case STATE_LOCKED: break;
        default: reset_to_idle();
    }
}
// --- ⚠️ КІНЕЦЬ ЗМІНИ v1.7.0 ---

#if (ZVS_MODE == 2)
void enter_sleep_mode() { reset_to_idle(); g_state=STATE_SLEEPING; set_colon_mode(COLON_OFF); set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); }
void wake_up_from_sleep() { sleep_disable(); g_state=STATE_IDLE; set_colon_mode(COLON_BLINK_SLOW);
}
#endif

void setup_timer1_1ms() {
    TCCR1A=0; TCCR1B=0; TCNT1=0; OCR1A=1999; TCCR1B|=(1<<WGM12)|(1<<CS11); TIMSK|=(1<<OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
    update_colon_state();
    
    // --- ⚠️ ПОЧАТОК ЗМІНИ v1.7.0 (Нова логіка звуку) ---
    // 1. Звичайний (300мс) / Довгий (800мс) біп
    if (g_beep_ms_counter > 0) { 
        if(g_beep_ms_counter == 800 || g_beep_ms_counter == 300) BEEPER_PORT|=BEEPER_BIT; 
        g_beep_ms_counter--; 
        if(g_beep_ms_counter == 0) BEEPER_PORT&=~BEEPER_BIT; 
    }
    
    // 2. Серія біпів для збереження часу (0.8с)
    if (g_clock_save_burst_timer > 0) {
        g_clock_save_burst_timer--;
        if ((g_clock_save_burst_timer % 100) == 50) BEEPER_PORT |= BEEPER_BIT; // 50ms ON
        else if ((g_clock_save_burst_timer % 100) == 0) BEEPER_PORT &= ~BEEPER_BIT; // 50ms OFF
        if (g_clock_save_burst_timer == 0) BEEPER_PORT &= ~BEEPER_BIT;
    }

    // 3. Серія біпів для перевертання (3 рази по 300мс, з паузами ~300мс)
    if (g_beep_flip_sequence_timer > 0) {
        if (g_beep_flip_sequence_timer == 1 || g_beep_flip_sequence_timer == 601 || g_beep_flip_sequence_timer == 1201) {
            do_short_beep(); // 300ms
        }
        g_beep_flip_sequence_timer++;
        // Зупинити або по таймауту 5с, або після 3-го біпа (1201+300=1501)
        if (g_flip_beep_timeout_ms == 0 || g_beep_flip_sequence_timer > 1501) {
            g_beep_flip_sequence_timer = 0; 
        }
    }
    // --- ⚠️ КІНЕЦЬ ЗМІНИ v1.7.0 ---

    char rk = g_debounced_key_state;
    if (rk == g_last_key_for_hold && rk != 0) {
        if (!g_key_hold_3sec_flag && g_key_3sec_hold_timer_ms < 3000) { g_key_3sec_hold_timer_ms++;
        if(g_key_3sec_hold_timer_ms==3000) g_key_hold_3sec_flag=true; }
        if (g_key_continuous_hold_ms < 65000) g_key_continuous_hold_ms++;
    } else { g_key_3sec_hold_timer_ms=0; g_last_key_hold_duration=g_key_continuous_hold_ms;
    g_key_continuous_hold_ms=0; g_last_key_for_hold=rk; g_key_hold_3sec_flag=false; }

    static uint8_t kp = 0;
    #if ENABLE_KEYPAD
        kp++; if(kp>=20) { kp=0; if(!g_adc_read_pending) { ADCSRA|=(1<<ADSC); g_adc_read_pending=true;
        } }
        if(g_adc_read_pending && !(ADCSRA&(1<<ADSC))) {
            g_adc_value=ADC;
            g_adc_read_pending=false; char ck=0;
            if(g_adc_value<ADC_NOISE_THRESHOLD) { for(uint8_t i=0;i<12;i++) if(g_adc_value>=(adc_key_values[i]-ADC_TOLERANCE) && g_adc_value<=(adc_key_values[i]+ADC_TOLERANCE)) { ck=adc_key_map[i]; break;
            } }
            if(ck==g_key_last_state) { if(g_debounce_counter<DEBOUNCE_TIME) g_debounce_counter++; else g_debounced_key_state=ck;
            } else { g_debounce_counter=0; g_key_last_state=ck; }
        }
    #endif

    if(g_state!=STATE_SLEEPING) run_display_multiplex();
    g_timer_ms++; 
    // g_blink_counter++ видалено (v1.7.1)
    
    if(g_quick_start_delay_ms>0) { g_quick_start_delay_ms--; if(g_quick_start_delay_ms==0 && g_state==STATE_QUICK_START_PREP) start_cooking_cycle(); }
    
    if(g_state==STATE_FINISHED) { 
        g_post_cook_timer_ms++; 
        if(g_post_cook_timer_ms >= 30000) { 
            g_state=STATE_POST_COOK; g_post_cook_timer_ms=0; 
            do_long_beep(); 
        } 
    } else if(g_state==STATE_POST_COOK) {
        g_post_cook_timer_ms++;
    }

    if(g_clock_save_blink_ms>0) { g_clock_save_blink_ms--; if(g_clock_save_blink_ms==0) reset_to_idle();
    }
    
    if(g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms--;
    if (g_flip_beep_timeout_ms > 0) g_flip_beep_timeout_ms--;

    if(g_timer_ms>=1000) {
        g_timer_ms=0; 
        
        if (g_door_overlay_timer_ms == 0) {
            if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE) update_cook_timer();
            
            if(g_state==STATE_POST_COOK) { 
                uint16_t s = g_post_cook_timer_ms / 1000; 
                if(s == 60) do_long_beep();
                else if(s >= 120) { do_long_beep(); reset_to_idle(); }
            }
        
            #if (ZVS_MODE==0)
                if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE) update_clock();
            #elif (ZVS_MODE==1 || ZVS_MODE==2)
                g_zvs_watchdog_counter++;
                if(g_zvs_watchdog_counter>1) {
                    if(g_zvs_present) { g_zvs_present=false;
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
    setup_display_pins(); setup_hardware();
    #if ENABLE_KEYPAD
        keypad_init();
    #endif
    setup_timer1_1ms();
    #if (ZVS_MODE!=0)
        MCUCR|=(1<<ISC01); MCUCR&=~(1<<ISC00); GIMSK|=(1<<INT0); g_zvs_watchdog_counter=0; g_zvs_present=true;
    #endif
    reset_to_idle(); sei();
}

void loop() {
    static char s_lps=0; static uint16_t s_lht=0; char cks=0;
    static bool s_last_door_state = false; 

    #if (ZVS_MODE==2)
        if(g_state==STATE_SLEEPING) { 
            sleep_cpu(); s_lps=0; s_last_door_state = (CDD_PIN & CDD_BIT);
        }
    #endif

    if(g_state!=STATE_SLEEPING) {
        if(g_key_hold_3sec_flag) {
            g_key_hold_3sec_flag=false;
            char hk=g_last_key_for_hold;
            // v1.7.1: Видалено логіку ADC_DEBUG
            if(hk==KEY_STOP_RESET) { if(g_state==STATE_IDLE) { g_state=STATE_LOCKED; } else if(g_state==STATE_LOCKED) reset_to_idle(); }
            s_lps=0;
        }

        // --- v1.7.0: Нова логіка обробки дверцят ---
        bool door_is_open = (CDD_PIN & CDD_BIT);
        if (door_is_open != s_last_door_state) {
            if (door_is_open) {
                if (g_state == STATE_COOKING) {
                    g_state = STATE_PAUSED; g_door_open_during_pause = true;
                    set_magnetron(false); set_turntable(false);
                } else if (g_state == STATE_PAUSED || g_state == STATE_FLIP_PAUSE) {
                    g_door_open_during_pause = true;
                    g_flip_beep_timeout_ms = 0; // Зупинити біпи при відкритті
                } else if (g_state == STATE_FINISHED || g_state == STATE_POST_COOK) {
                    reset_to_idle(); 
                } else if (g_state != STATE_LOCKED) {
                    g_door_overlay_timer_ms = 2000; 
                }
            } 
            else {
                if (g_door_open_during_pause) {
                    g_door_open_during_pause = false;
                }
                if (g_door_overlay_timer_ms > 0) {
                    g_door_overlay_timer_ms = 0; 
                }
            }
            s_last_door_state = door_is_open;
        }

        bool ak=true; 
        if(g_state==STATE_LOCKED || g_state==STATE_CLOCK_SAVED || g_door_overlay_timer_ms > 0) {
            ak=false;
        }
        if(g_door_open_during_pause) {
            ak=true; 
        }

        if(ak) {
            cks=get_key_press();
            if(g_door_open_during_pause && cks!=KEY_STOP_RESET && cks!=0) cks=0;
            
            if(cks!=0) { if(g_key_continuous_hold_ms>500) handle_key_hold_increment(cks, g_key_continuous_hold_ms, &s_lht); } else s_lht=0;
            if(cks!=s_lps) { 
                if(cks==0) { 
                    if(s_lps!=0 && g_last_key_hold_duration<=500) {
                        handle_state_machine(s_lps, true); // v1.7.0: allow_beep = true
                    }
                }
                s_lps=cks;
            }
        } else { 
            s_lps=0; s_lht=0;
        }
        
        if(g_state==STATE_FINISHED || g_state==STATE_POST_COOK) handle_state_machine(0, false);
        
        update_display();
    }
}

int main(void) { setup(); while(1) loop(); return 0; }