#include "keypad_driver.h"

// ============================================================================
// --- Глобальні змінні (локальні для цього .c) ---
// ============================================================================

static volatile uint16_t g_adc_value = 1023;
static volatile char g_debounced_key_state = 0;
static volatile char g_key_last_state = 0;
static volatile uint8_t g_debounce_counter = 0;
static volatile bool g_adc_read_pending = false;

// --- 🔴 ПОЧАТОК БЛОКУ ВІДКОЧЕННЯ (v2.6.2 - Повернення до v2.4.4) ---
// (Логіка s_last_adc_check_ms видалена)
// --- 🔴 КІНЕЦЬ БЛОКУ ВІДКОЧЕННЯ ---

// Карти значень ADC
static const uint16_t adc_key_values[] = {
    1003, 964, 926, 887, 848, 820, 744, 606, 539, 494, 395, 350
};
static const char adc_key_map[] = { 
    KEY_AUTO_COOK, KEY_AUTO_DEFROST, KEY_QUICK_DEFROST, KEY_10_SEC, 
    KEY_1_MIN, KEY_10_MIN, KEY_LESS, KEY_MORE, KEY_MICRO, 
    KEY_START_QUICKSTART, KEY_STOP_RESET, KEY_CLOCK 
};

// ============================================================================
// --- Реалізація функцій ---
// ============================================================================

void keypad_init() { 
    ADMUX = (1<<REFS0)|(KEYPAD_ADC_CHANNEL & 0x07); 
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); 
}

char get_key_press() { 
    cli(); 
    char key = g_debounced_key_state; 
    sei(); 
    return key; 
}

// Логіка, що викликається з ISR таймера для опитування АЦП
void keypad_timer_tick(void) {
    #if ENABLE_KEYPAD
        
        // --- 🔴 ПОЧАТОК БЛОКУ ВІДКОЧЕННЯ (v2.6.2 - Повернення до v2.4.4) ---
        // Повертаємо лічильник 'kp', оскільки ISR знову стабільний
        static uint8_t kp = 0;
        kp++; 
        if(kp >= 20) { // Кожні 20 мс
            kp = 0; 
        // --- 🔴 КІНЕЦЬ БЛОКУ ВІДКОЧЕННЯ ---

            if(!g_adc_read_pending) { 
                ADCSRA |= (1<<ADSC); 
                g_adc_read_pending = true; 
            } 
        }
        
        if(g_adc_read_pending && !(ADCSRA & (1<<ADSC))) { 
            g_adc_value = ADC; 
            g_adc_read_pending = false; 
            char ck = 0; 
            
            if(g_adc_value < ADC_NOISE_THRESHOLD) { 
                for(uint8_t i=0; i < 12; i++) { 
                    if(g_adc_value >= (adc_key_values[i] - ADC_TOLERANCE) && g_adc_value <= (adc_key_values[i] + ADC_TOLERANCE)) { 
                        ck = adc_key_map[i]; 
                        break; 
                    } 
                } 
            } 
            
            // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.4.3 - Виправлення Debounce) ---
            // Це оригінальна, правильна логіка з v2.2.0.
            if(ck == g_key_last_state) { 
                if(g_debounce_counter < DEBOUNCE_TIME) 
                    g_debounce_counter++; 
                else 
                    g_debounced_key_state = ck; 
            } else { 
                g_debounce_counter = 0; 
                g_key_last_state = ck; 
            } 
            // (Видалено 'if (ck == 0)')
            // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
        }
    #endif
}

void handle_key_hold_increment(char key, uint16_t hold_duration, uint16_t* last_trigger_ms) {
    uint16_t interval = 0;
    switch (g_state) {
        case STATE_SET_TIME: 
        case STATE_SET_CLOCK_TIME: 
        // --- 🔴 ПОЧАТОК БЛОКУ ВИПРАВЛЕННЯ (v2.3.9 - Оптимізація пам'яті) ---
        // Видалено 'case STATE_COOKING:'
        // --- 🔴 КІНЕЦЬ БЛОКУ ВИПРАВЛЕННЯ ---
            if (hold_duration > 3000) interval = 50; 
            else if (hold_duration > 1500) interval = 100; 
            else interval = 200; 
            break;
            
        case STATE_SET_WEIGHT:
            if (hold_duration > 2000) interval = 100; 
            else interval = 200; 
            break;
            
        default: 
            return;
    }
    
    if ((uint16_t)(hold_duration - *last_trigger_ms) >= interval) { 
        handle_state_machine(key, false); 
        *last_trigger_ms = hold_duration; 
    }
}