#include "timers_isr.h"
#include "cooking_logic.h"  // Потрібен для update_cook_timer
#include "display_driver.h" // Потрібен для update_colon_state, run_display_multiplex
#include "keypad_driver.h"  // Потрібен для get_key_press, keypad_timer_tick

// (v2.9.2) Видалено невикористовувані константи

// ============================================================================
// --- 🟨 РЕАЛІЗАЦІЯ ФУНКЦІЙ ---
// ============================================================================

// (v2.9.0) ВИДАЛЕНО setup_async_timer2_rtc, disable_async_timer2_rtc та ISR(TIMER2_OVF_vect)

// (v2.9.32) Змінено на 500µs (2000Hz)
void setup_timer1_500us() {
    TCCR1A=0; TCCR1B=0; TCNT1=0; 
    
    #if (F_CPU == 16000000L)
        OCR1A = 999; // 16МГц / 8 / 1000 = 2000Hz (було 1999)
    #elif (F_CPU == 8000000L)
        OCR1A = 499; // 8МГц / 8 / 500 = 2000Hz (було 999)
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

// 🔽🔽🔽 (v2.9.36) Повертаємо do_long_beep до 800ms 🔽🔽🔽
void do_short_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 150; } // Залишаємо 150
void do_long_beep() { if (g_beep_ms_counter == 0) g_beep_ms_counter = 800; } // Було 1000
// 🔼🔼🔼 (v2.9.36) Кінець змін 🔼🔼🔼

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
    // (v2.9.32) Зміни для 1кГц тону
    static uint8_t prescaler_1ms = 0; // Дільник для 1ms логіки
    
    // 1. Логіка звуку (виконується кожні 500µs для 1kHz тону)
    if (g_beep_ms_counter > 0) { 
        BEEPER_PORT ^= BEEPER_BIT; // Toggles pin every 0.5ms = 1kHz tone
        g_beep_ms_counter--; 
    } else {
        if (g_clock_save_burst_timer == 0 || (g_clock_save_burst_timer % 100) != 50) {
             BEEPER_PORT &= ~BEEPER_BIT;
        }
    }

    // 2. Логіка "burst" (збереження годинника)
    if (g_clock_save_burst_timer > 0) { 
        g_clock_save_burst_timer--; 
        if ((g_clock_save_burst_timer % 100) == 50) {
            // (v2.9.34) Використовуємо коротший біп
            g_beep_ms_counter = 25; // Запускаємо 25ms біп
        } 
    } 
    
    // 3. Логіка Flip (яка викликає do_short_beep)
    if (g_beep_flip_sequence_timer > 0) { 
        if (g_beep_flip_sequence_timer == 1 || g_beep_flip_sequence_timer == 601 || g_beep_flip_sequence_timer == 1201) { 
            do_short_beep(); // Це встановить g_beep_ms_counter = 150
        } 
        g_beep_flip_sequence_timer++; 
        if (g_flip_beep_timeout_ms == 0 || g_beep_flip_sequence_timer > 1501) { 
            g_beep_flip_sequence_timer = 0; 
        } 
    }

    // 4. Дільник (Phaser) для 1ms логіки
    prescaler_1ms++;
    if (prescaler_1ms < 2) {
        return; // Пропускаємо, поки не пройде 1ms (2 цикли * 500µs)
    }
    prescaler_1ms = 0; // Скидаємо дільник
    
    // 🔽🔽🔽 УСЯ РЕШТА ЛОГІКИ (ВИКОНУЄТЬСЯ КОЖНІ 1ms) 🔽🔽🔽
    
    g_millis_counter++; 
    
    // (v2.9.2) 16-бітний "знімок" часу для INT0
    g_millis_16bit_snapshot = (uint16_t)g_millis_counter;
    
    update_colon_state(); // З display_driver
    
    g_timer_ms++; 
    
    // --- 1-секундний таймер ---
    if(g_timer_ms>=1000) {
        g_timer_ms=0;
        g_1sec_tick_flag = true;
    }

    // Встановлюємо прапор для loop()
    g_1ms_tick_flag = true;
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