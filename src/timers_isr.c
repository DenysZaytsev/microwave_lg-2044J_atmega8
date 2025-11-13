#include "timers_isr.h"
#include "cooking_logic.h"  // Потрібен для update_cook_timer
#include "display_driver.h" // Потрібен для update_colon_state, run_display_multiplex
#include "keypad_driver.h"  // Потрібен для get_key_press, keypad_timer_tick

// Налаштування фільтра ZVS (мінімально допустима частота)
#define ZVS_MIN_PULSES_PER_SEC 40 
#define ZVS_QUALIFICATION_SECONDS 2 // "2 секунди стабільності"

// ============================================================================
// --- 🟨 РЕАЛІЗАЦІЯ ФУНКЦІЙ ---
// ============================================================================

void setup_timer1_1ms() {
    TCCR1A=0; TCCR1B=0; TCNT1=0; 
    
    #if (F_CPU == 16000000L)
        OCR1A = 1999; // 16МГц / 8 / 2000 = 1000Hz
    #elif (F_CPU == 8000000L)
        OCR1A = 999; // 8МГц / 8 / 1000 = 1000Hz
    #else
        #error "Непідтримувана F_CPU. Використовуйте 8МГц або 16МГц."
    #endif
    
    TCCR1B|=(1<<WGM12)|(1<<CS11); // Дільник Prescaler = 8
    TIMSK|=(1<<OCIE1A); 
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

void do_short_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 300; }
void do_long_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 800; }
void do_flip_beep() { if (g_beep_flip_sequence_timer == 0) g_beep_flip_sequence_timer = 1; }

void run_1sec_tasks(void) {
    if (g_door_overlay_timer_ms == 0) {
            
        if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
            update_cook_timer();
        
        if(g_state==STATE_POST_COOK) { 
            g_post_cook_sec_counter++;
            if(g_post_cook_sec_counter == 60) do_long_beep(); 
            else if(g_post_cook_sec_counter >= 120) { 
                do_long_beep(); 
                // Не викликаємо reset_to_idle() прямо звідси,
                // Головний цикл має обробити це.
                // Натомість, можна встановити прапор або змінити стан.
                // Для простоти, поки що залишимо так, але це "запах" коду.
                // Краще: g_state = STATE_IDLE; (якщо reset_to_idle() безпечний)
                // Або встановити прапор, який loop() перетворить на reset_to_idle().
                
                // (v2.8.0) Оскільки reset_to_idle() безпечний, викликаємо його.
                // (Потрібно включити "microwave_firmware.h" у "timers_isr.h")
                // reset_to_idle(); 
                // (v2.8.1) Ні, reset_to_idle() не є частиною цього модуля.
                // Головний loop() має обробити це.
            } 
        }
        
        #if (ZVS_MODE==0)
            if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
                update_clock();
        
        #elif (ZVS_MODE==1 || ZVS_MODE==2)
            g_zvs_watchdog_counter++; 
            
            bool valid_pulse_train = (g_zvs_watchdog_counter == 1) && (g_zvs_pulse_counter >= ZVS_MIN_PULSES_PER_SEC);

            g_zvs_pulse_counter = 0; 

            if(!valid_pulse_train) { 
                if(g_zvs_present) { 
                    g_zvs_present = false; 
                } 
                g_zvs_qualification_counter = 0; 
                
                #if (ZVS_MODE==2)
                    if (g_state != STATE_SLEEPING) enter_sleep_mode(); 
                #endif 
                
                if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
                    update_clock(); 
            } else {
                if(!g_zvs_present) {
                     g_zvs_qualification_counter++; 
                     
                     if (g_zvs_qualification_counter >= ZVS_QUALIFICATION_SECONDS) {
                         g_zvs_present = true;
                         #if (ZVS_MODE==2)
                            if(g_state==STATE_SLEEPING) wake_up_from_sleep(); 
                         #endif
                     }
                } else {
                    g_zvs_qualification_counter = 0;
                }
            }
        #endif 
    }
}


// ============================================================================
// --- 🔴 ПЕРЕРИВАННЯ (ISRs) ---
// ============================================================================

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
    
    // --- Загальні таймери (мілісекундні) ---
    if(g_quick_start_delay_ms>0) { 
        g_quick_start_delay_ms--; 
        if(g_quick_start_delay_ms==0 && g_state==STATE_QUICK_START_PREP) 
            g_start_cooking_flag = true; 
    }

    if(g_state==STATE_FINISHED) { g_post_cook_timer_ms++; if(g_post_cook_timer_ms >= 30000) { g_state=STATE_POST_COOK; g_post_cook_timer_ms=0; g_post_cook_sec_counter = 0; do_long_beep(); } } 
    else if(g_state==STATE_POST_COOK) { g_post_cook_timer_ms++; }
    if(g_clock_save_blink_ms>0) { g_clock_save_blink_ms--; if(g_clock_save_blink_ms==0) g_state = STATE_IDLE; } // (v2.8.0) Безпечніше, ніж reset
    if(g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms--;
    if (g_flip_beep_timeout_ms > 0) g_flip_beep_timeout_ms--;
    
    // --- 1-секундний таймер ---
    if(g_timer_ms>=1000) {
        g_timer_ms=0;
        g_1sec_tick_flag = true;
    }
}

#if (ZVS_MODE!=0)
ISR(INT0_vect) {
    g_zvs_watchdog_counter = 0; 
    
    if (g_zvs_pulse_counter < 254) g_zvs_pulse_counter++; 
    
    if(g_magnetron_request && g_zvs_present) {
        MAGNETRON_PORT |= MAGNETRON_BIT;
    }
    
    if(g_zvs_present && g_door_overlay_timer_ms == 0 && g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) { 
        if(g_zvs_pulse_counter >= 50) { 
            g_zvs_pulse_counter = 0; 
            g_timer_ms = 0;          
            update_clock();          
        } 
    }
}
#endif