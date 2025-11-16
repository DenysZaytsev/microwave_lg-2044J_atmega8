#include "timers_isr.h"
#include "cooking_logic.h"  // Потрібен для update_cook_timer
#include "display_driver.h" // Потрібен для update_colon_state, run_display_multiplex
#include "keypad_driver.h"  // Потрібен для get_key_press, keypad_timer_tick

// (v2.9.2) Видалено невикористовувані константи
// #define ZVS_MIN_PULSES_PER_SEC 40 
// #define ZVS_QUALIFICATION_SECONDS 2 

// ============================================================================
// --- 🟨 РЕАЛІЗАЦІЯ ФУНКЦІЙ ---
// ============================================================================

// (v2.9.0) ВИДАЛЕНО setup_async_timer2_rtc, disable_async_timer2_rtc та ISR(TIMER2_OVF_vect)

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
        
        #if (ZVS_MODE != 0)
        if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION && g_state != STATE_ZVS_QUALIFICATION) 
            update_cook_timer();
        #else
        if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
            update_cook_timer();
        #endif
        
        
        if(g_state==STATE_POST_COOK) { 
            g_post_cook_sec_counter++;
            if(g_post_cook_sec_counter == 60) do_long_beep(); 
            else if(g_post_cook_sec_counter >= 120) { 
                do_long_beep(); 
            } 
        }
        
        // (v2.9.2) Годинник тепер завжди йде від Timer1.
        #if (ZVS_MODE != 0)
        if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION && g_state != STATE_ZVS_QUALIFICATION) 
            update_clock();
        #else
        if(g_state != STATE_PAUSED && g_state != STATE_FLIP_PAUSE && g_state != STATE_STAGE2_TRANSITION) 
            update_clock();
        #endif
    }
}


// ============================================================================
// --- 🔴 ПЕРЕРИВАННЯ (ISRs) ---
// ============================================================================

ISR(TIMER1_COMPA_vect) { 
    static uint8_t slow_task_phaser = 0; 
    slow_task_phaser++;
    g_millis_counter++; 
    
    // (v2.9.2) 16-бітний "знімок" часу для INT0
    g_millis_16bit_snapshot = (uint16_t)g_millis_counter;
    
    update_colon_state(); // З display_driver
    
    // 🔽🔽🔽 (v2.9.31) ОНОВЛЕНА ЛОГІКА ЗВУКУ ДЛЯ ПАСИВНОГО ЗУМЕРА 🔽🔽🔽
    
    // 1. Головний драйвер тону (500 Гц) - він керує g_beep_ms_counter
    if (g_beep_ms_counter > 0) { 
        BEEPER_PORT ^= BEEPER_BIT; // Toggles pin every 1ms
        g_beep_ms_counter--; 
    } else {
        // Якщо g_beep_ms_counter == 0, АЛЕ burst-таймер НЕ хоче увімкнути
        // (ми перевіримо це нижче), тоді пін має бути LOW.
        if (g_clock_save_burst_timer == 0 || (g_clock_save_burst_timer % 100) != 50) {
             BEEPER_PORT &= ~BEEPER_BIT;
        }
    }
    
    // 2. Логіка "burst" (збереження годинника) - вона ТРИГЕРИТЬ g_beep_ms_counter
    if (g_clock_save_burst_timer > 0) { 
        g_clock_save_burst_timer--; 
        if ((g_clock_save_burst_timer % 100) == 50) {
            g_beep_ms_counter = 50; // Запускаємо 50ms біп
        } 
    } 
    
    // 3. Логіка Flip (яка викликає do_short_beep)
    if (g_beep_flip_sequence_timer > 0) { 
        if (g_beep_flip_sequence_timer == 1 || g_beep_flip_sequence_timer == 601 || g_beep_flip_sequence_timer == 1201) { 
            do_short_beep(); // Це встановить g_beep_ms_counter = 300
        } 
        g_beep_flip_sequence_timer++; 
        if (g_flip_beep_timeout_ms == 0 || g_beep_flip_sequence_timer > 1501) { 
            g_beep_flip_sequence_timer = 0; 
        } 
    }
    // 🔼🔼🔼 (v2.9.31) КІНЕЦЬ ОНОВЛЕННЯ ЛОГІКИ ЗВУКУ 🔼🔼🔼

    // --- Обробка утримання кнопок ---
    char rk = get_key_press(); // З keypad_driver
    if (rk == g_last_key_for_hold && rk != 0) {
        if (!g_key_hold_3sec_flag && g_key_3sec_hold_timer_ms < 3000) { g_key_3sec_hold_timer_ms++; if(g_key_3sec_hold_timer_ms==3000) g_key_hold_3sec_flag=true; }
        if (g_key_continuous_hold_ms < 65000) g_key_continuous_hold_ms++;
    } else { g_key_3sec_hold_timer_ms=0; g_last_key_hold_duration=g_key_continuous_hold_ms; g_key_continuous_hold_ms=0; g_last_key_for_hold=rk; g_key_hold_3sec_flag=false; }

    // --- Обробка опитування АЦП (з keypad_driver) ---
    keypad_timer_tick(); 

    // --- Мультиплексування дисплея (з display_driver) ---
    static uint8_t display_phaser = 0;
    display_phaser++;
    run_display_multiplex(); // (v2.9.2) Видалено перевірку STATE_SLEEPING
    g_timer_ms++; 
    
    // --- Загальні таймери (мілісекундні) ---
    if(g_quick_start_delay_ms>0) { 
        g_quick_start_delay_ms--; 
        if(g_quick_start_delay_ms==0 && g_state==STATE_QUICK_START_PREP) 
            g_start_cooking_flag = true; 
    }

    if(g_state==STATE_FINISHED) { g_post_cook_timer_ms++; if(g_post_cook_timer_ms >= 30000) { g_state=STATE_POST_COOK; g_post_cook_timer_ms=0; g_post_cook_sec_counter = 0; do_long_beep(); } } 
    else if(g_state==STATE_POST_COOK) { g_post_cook_timer_ms++; }
    if(g_clock_save_blink_ms>0) { g_clock_save_blink_ms--; if(g_clock_save_blink_ms==0) g_state = STATE_IDLE; }
    if(g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms--;
    if (g_flip_beep_timeout_ms > 0) g_flip_beep_timeout_ms--;
    
    #if (ZVS_MODE != 0)
    // (v2.9.3) МАКСИМАЛЬНО СПРОЩЕНИЙ ТАЙМАУТ
    // ISR тільки зменшує лічильник. Вся логіка обробки перенесена в loop()
    if (g_zvs_qual_timeout_ms > 0) {
        g_zvs_qual_timeout_ms--;
    }
    #endif
    
    // --- 1-секундний таймер ---
    if(g_timer_ms>=1000) {
        g_timer_ms=0;
        g_1sec_tick_flag = true;
    }
    if (slow_task_phaser >= 2) slow_task_phaser = 0; 
}

#if (ZVS_MODE!=0)
ISR(INT0_vect) {
    // (v2.9.8) Спрощена логіка 
    if (g_state == STATE_ZVS_QUALIFICATION && g_zvs_qualification_counter < ZVS_QUALIFICATION_COUNT) {
        
        // (v2.9.8) Видалено запис в g_zvs_timestamps
        g_zvs_qualification_counter++;
        
        g_zvs_qual_timeout_ms = ZVS_QUAL_TIMEOUT_MS; // Скидаємо таймер таймауту
    }
    
    // (v2.9.2) Залишаємо ТІЛЬКИ логіку ввімкнення магнетрона
    if(g_magnetron_request) {
        MAGNETRON_PORT |= MAGNETRON_BIT;
    }
}
#endif