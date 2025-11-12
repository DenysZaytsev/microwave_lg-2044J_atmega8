#include "display_driver.h"

// ============================================================================
// --- Глобальні змінні та карти символів (локальні для цього .c) ---
// ============================================================================

// Тип для карти символів
typedef struct { uint8_t portb_mask; uint8_t portc_mask; } char_pattern_t;

uint8_t g_display_buffer[4] = {0, 0, 0, 0};
volatile uint8_t g_colon_visible = 0;
volatile uint16_t g_colon_timer = 0;
volatile ColonDisplayMode g_colon_mode = COLON_OFF;

// Карти символів
static const char_pattern_t number_map[] = { 
    {0x05, 0x36}, // 0
    {0x04, 0x02}, // 1
    {0x03, 0x26}, // 2
    {0x07, 0x06}, // 3
    {0x06, 0x12}, // 4
    {0x07, 0x14}, // 5
    {0x07, 0x34}, // 6
    {0x04, 0x06}, // 7
    {0x07, 0x36}, // 8
    {0x07, 0x16}  // 9
};

static const char_pattern_t symbol_map[] = { 
    {0x00, 0x00}, // 10: CHAR_SPACE
    {0x01, 0x30}, // 11: CHAR_L
    {0x03, 0x34}, // 12: CHAR_E
    {0x07, 0x22}, // 13: CHAR_D
    {0x07, 0x30}, // 14: CHAR_B
    {0x02, 0x34}, // 15: CHAR_F
    {0x06, 0x36}, // 16: CHAR_A
    {0x01, 0x34}, // 17: CHAR_C
    {0x06, 0x32}, // 18: CHAR_H
    {0x02, 0x20}, // 19: CHAR_R
    {0x07, 0x20}, // 20: CHAR_O
    {0x06, 0x20}, // 21: CHAR_N
    {0x02, 0x36}, // 22: CHAR_P
    {0x02, 0x00}, // 23: CHAR_DASH
    {0x03, 0x30}  // 24: CHAR_T
};

// Карти пінів
static const uint8_t digit_portd_pins[] = {DIGIT1_CATHODE, DIGIT2_CATHODE, 0, 0};
static const uint8_t digit_portb_pins[] = {0, 0, DIGIT3_CATHODE, DIGIT4_CATHODE};
// Логічний індекс (0-3) -> Фізичний індекс (для масивів пінів)
static const uint8_t logical_to_physical_index[] = {3, 1, 0, 2}; // 0->3, 1->1, 2->0, 3->2

// ============================================================================
// --- Реалізація функцій ---
// ============================================================================

void setup_display_pins() {
    DDRB |= SEG_D | SEG_G | SEG_C | DIGIT3_CATHODE | DIGIT4_CATHODE;
    DDRC |= SEG_A | SEG_B | SEG_E | SEG_F | COLON_ANODE; 
    DDRD |= DIGIT1_CATHODE | DIGIT2_CATHODE;
    
    PORTB &= ~(SEG_D | SEG_G | SEG_C | DIGIT3_CATHODE | DIGIT4_CATHODE); 
    PORTC &= ~(SEG_A | SEG_B | SEG_E | SEG_F);
    PORTD &= ~(DIGIT1_CATHODE | DIGIT2_CATHODE); 
    PORTC &= ~COLON_ANODE; 
}

void disable_all_digits() { 
    PORTD &= ~(DIGIT1_CATHODE | DIGIT2_CATHODE); 
    PORTB &= ~(DIGIT3_CATHODE | DIGIT4_CATHODE); 
}

static void set_segments(uint8_t portb_mask, uint8_t portc_mask) { 
    PORTB = (PORTB & ~(ALL_SEGMENTS_B)) | portb_mask; 
    PORTC = (PORTC & ~(ALL_SEGMENTS_C)) | portc_mask; 
}

static void display_symbol(uint8_t ld, uint8_t sym) {
    uint8_t pid = logical_to_physical_index[ld]; 
    disable_all_digits();
    
    char_pattern_t p;
    if (sym < 10) { 
        p = number_map[sym]; 
    } else { 
        p = symbol_map[sym-10]; 
    } 
    
    set_segments(p.portb_mask, p.portc_mask);
    
    if (digit_portd_pins[pid] != 0) 
        PORTD |= digit_portd_pins[pid]; 
    else 
        PORTB |= digit_portb_pins[pid];
}

void set_display(uint8_t vis1, uint8_t vis2, uint8_t vis3, uint8_t vis4) { 
    g_display_buffer[3]=vis1; 
    g_display_buffer[2]=vis2; 
    g_display_buffer[1]=vis3; 
    g_display_buffer[0]=vis4; 
}

void display_time_suppressed(uint16_t total_seconds) {
    uint8_t min=total_seconds/60, sec=total_seconds%60, d1=min/10, d2=min%10;
    if (d1==0) { 
        d1=CHAR_SPACE; 
        if(d2==0) d2=CHAR_SPACE; 
    }
    set_display(d1, d2, sec/10, sec%10);
}

void display_clock(uint8_t h, uint8_t m) {
    if (!g_clock_24hr_mode && h==0) h=12;
    if (!g_clock_24hr_mode && h>12) h-=12;
    set_display(h/10, h%10, m/10, m%10);
}

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
        case COLON_BLINK_SLOW: g_colon_visible = (g_colon_timer < 500); if (g_colon_timer >= 1000) g_colon_timer = 0; break;
        case COLON_BLINK_FAST: g_colon_visible = (g_colon_timer < 200); if (g_colon_timer >= 400) g_colon_timer = 0; break;
        case COLON_BLINK_SUPERFAST: g_colon_visible = (g_colon_timer < 166); if (g_colon_timer >= 333) g_colon_timer = 0; break;
    }
}

void run_display_multiplex() {
    static uint8_t cur_disp_digit = 0;
    
    if (g_colon_visible) 
        PORTC |= COLON_ANODE; 
    else 
        PORTC &= ~COLON_ANODE;
        
    switch(cur_disp_digit) {
        case 0: display_symbol(0, g_display_buffer[0]); break;
        case 1: display_symbol(1, g_display_buffer[1]); break;
        case 2: display_symbol(2, g_display_buffer[2]); break;
        case 3: display_symbol(3, g_display_buffer[3]); break;
    }
    
    cur_disp_digit++; 
    if (cur_disp_digit >= 4) cur_disp_digit = 0;
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
        case STATE_FLIP_PAUSE: 
        {
            bool show_alt = (g_timer_ms < 500);
            if (g_door_open_during_pause) { 
                if (show_alt) { 
                    set_display(CHAR_D, CHAR_O, CHAR_O, CHAR_R); 
                    set_colon_mode(COLON_OFF); 
                } else { 
                    display_time_suppressed(g_cook_time_total_sec); 
                    set_colon_mode(COLON_ON); 
                } 
            } 
            else if (g_state == STATE_FLIP_PAUSE) { 
                if (show_alt) { 
                    set_display(CHAR_D, CHAR_E, CHAR_F, g_auto_program); 
                    set_colon_mode(COLON_OFF); 
                } else { 
                    display_time_suppressed(g_cook_time_total_sec); 
                    set_colon_mode(COLON_ON); 
                } 
            } 
            else if (g_active_auto_program_type != PROGRAM_NONE) { 
                if (show_alt) { 
                    if (g_active_auto_program_type == PROGRAM_COOK) 
                        set_display(CHAR_A, CHAR_C, CHAR_DASH, g_auto_program); 
                    else 
                        set_display(CHAR_D, CHAR_E, CHAR_F, g_auto_program); 
                    set_colon_mode(COLON_OFF); 
                } else { 
                    display_time_suppressed(g_cook_time_total_sec); 
                    set_colon_mode(COLON_ON); 
                } 
            } 
            else { 
                display_time_suppressed(g_cook_time_total_sec); 
                set_colon_mode(COLON_ON); 
            }
        } break;
        
        case STATE_SLEEPING: 
            disable_all_digits(); 
            break;
            
        case STATE_SET_CLOCK_MODE: 
            set_colon_mode(COLON_OFF); 
            if(g_clock_24hr_mode) 
                set_display(CHAR_SPACE, CHAR_SPACE, 2, 4); 
            else 
                set_display(CHAR_SPACE, CHAR_SPACE, 1, 2); 
            break;
            
        case STATE_SET_CLOCK_TIME: 
            set_colon_mode(COLON_ON); 
            display_clock(g_input_hour, g_input_min); 
            break;
            
        case STATE_CLOCK_SAVED: 
            set_colon_mode(COLON_BLINK_SUPERFAST); 
            display_clock(g_clock_hour, g_clock_min); 
            break;
            
        case STATE_SET_TIME: 
            set_colon_mode(COLON_ON); 
            set_display(g_input_min_tens, g_input_min_units, g_input_sec_tens, 0); 
            break;
            
        case STATE_SET_POWER: 
        { 
            set_colon_mode(COLON_OFF); 
            uint16_t w=power_levels_watt[g_cook_power_level]; 
            set_display(CHAR_P, (w/100)%10, (w/10)%10, w%10); 
        } break;
        
        case STATE_SET_AUTO_COOK: 
            set_colon_mode(COLON_OFF); 
            set_display(CHAR_A, CHAR_C, CHAR_DASH, g_auto_program); 
            break;
            
        case STATE_SET_AUTO_DEFROST: 
            set_colon_mode(COLON_OFF); 
            set_display(CHAR_D, CHAR_E, CHAR_F, g_auto_program); 
            break;
            
        case STATE_SET_WEIGHT: 
        { 
            set_colon_mode(COLON_OFF); 
            uint8_t d1,d2; 
            if(g_auto_weight_grams>=1000) { 
                d1=(g_auto_weight_grams/1000)%10; 
                d2=(g_auto_weight_grams/100)%10; 
            } else { 
                d1=CHAR_SPACE; 
                d2=(g_auto_weight_grams/100)%10; 
            } 
            set_display(d1,d2,(g_auto_weight_grams/10)%10,g_auto_weight_grams%10); 
        } break;
        
        case STATE_QUICK_START_PREP: 
            set_colon_mode(COLON_ON); 
            display_time_suppressed(g_cook_time_total_sec); 
            break;
            
        case STATE_LOCKED: 
            set_colon_mode(COLON_OFF); 
            set_display(CHAR_SPACE, CHAR_SPACE, CHAR_SPACE, CHAR_L); 
            break;
        
        case STATE_COOKING: 
            set_colon_mode(COLON_ON);
            if(g_cook_time_total_sec <= 5) {
                if (g_timer_ms < 750) { 
                    if (g_stage2_time_sec > 0) 
                        set_display(CHAR_E, CHAR_N, CHAR_D, 1); // "End1"
                    else if (g_was_two_stage_cook) 
                        set_display(CHAR_E, CHAR_N, CHAR_D, 2); // "End2"
                    else 
                        set_display(CHAR_E, CHAR_N, CHAR_D, CHAR_DASH); // "End-"
                }
                else 
                    display_time_suppressed(g_cook_time_total_sec); 
            } else {
                display_time_suppressed(g_cook_time_total_sec);
            }
            break;
            
        case STATE_FINISHED: 
        case STATE_POST_COOK: 
            set_colon_mode(COLON_OFF);
            if (g_was_two_stage_cook) 
                set_display(CHAR_E, CHAR_N, CHAR_D, 2); // "End2"
            else 
                set_display(CHAR_E, CHAR_N, CHAR_D, CHAR_DASH); // "End-"
            break;

        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.3 - Баг 2-го етапу) ---
        // Під час переходу показуємо прочерки, як у стані за замовчуванням
        case STATE_STAGE2_TRANSITION:
            set_colon_mode(COLON_OFF); 
            set_display(CHAR_DASH, CHAR_DASH, CHAR_DASH, CHAR_DASH); 
            break;
        // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
            
        default: 
            set_colon_mode(COLON_OFF); 
            set_display(CHAR_DASH, CHAR_DASH, CHAR_DASH, CHAR_DASH); 
            break;
    }
}