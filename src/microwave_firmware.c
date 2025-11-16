/*
 * ПОВНА ПРОШИВКА МІКРОХВИЛЬОВКИ (v_final_2.9.32 - 1kHz Beep)
 *
 * --- ОПИС ФУНКЦІОНАЛУ v2.9.32 ---
 * 1. (v2.9.32) ЗМІНА: Timer1 тепер працює на 2000Hz (500µs) для генерації 1kHz тону.
 * 2. (v2.9.32) ЗМІНА: ISR(TIMER1_COMPA_vect) тепер має дільник (phaser) для 1ms логіки.
 * 3. (v2.9.3) ОПТИМІЗАЦІЯ: Винесено логіку таймауту ZVS з ISR(TIMER1_COMPA_vect) в loop().
 * 4. (v2.9.2) ВИДАЛЕНО: Повністю видалено STATE_SLEEPING.
 * 5. (v2.9.0) ВИПРАВЛЕНО: Інвертована логіка дверей (5В=Закрито, 0В=Відкрито).
 */

// ============================================================================
// --- 🔴 ВКЛЮЧЕННЯ МОДУЛІВ ---
// ============================================================================
#include "microwave_firmware.h" // < Містить всі визначення, extern та інші .h

// (Решта .h файлів вже включені через "microwave_firmware.h")


// ============================================================================
// --- 4. ГЛОБАЛЬНІ ЗМІННІ (ВИЗНАЧЕННЯ) ---
// ============================================================================
// (Оголошені 'extern' у .h, визначені тут ОДИН РАЗ)

volatile AppState_t g_state = STATE_IDLE;
volatile uint32_t g_millis_counter = 0; 
volatile uint16_t g_timer_ms = 0; 

// (v2.9.2) 16-бітний "знімок" часу
volatile uint16_t g_millis_16bit_snapshot = 0;

volatile bool g_1sec_tick_flag = false;
volatile bool g_start_cooking_flag = false; 

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
uint16_t g_stage1_time_sec = 0; uint8_t g_stage1_power = 0; 
volatile uint16_t g_stage2_time_sec = 0; volatile uint8_t g_stage2_power = 0;
volatile bool g_was_two_stage_cook = false; 
volatile uint8_t g_auto_program = 0; volatile uint16_t g_auto_weight_grams = 0;
volatile uint8_t g_input_min_tens=0, g_input_min_units=0, g_input_sec_tens=0;
volatile uint8_t g_input_hour=0, g_input_min=0, g_input_sec=0;
bool g_is_defrost_mode = false; 
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

#if (ZVS_MODE != 0)
// (v2.9.8) Спрощено
volatile uint8_t g_zvs_qualification_counter = 0;
// volatile uint16_t g_zvs_timestamps[ZVS_QUALIFICATION_COUNT]; // Видалено
volatile uint16_t g_zvs_qual_timeout_ms = 0;
// volatile bool g_zvs_error_flag = false; // Видалено
#endif

volatile uint8_t g_clock_save_beep_phaser = 0; 


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
    
    #if (ZVS_MODE != 0)
    g_zvs_qualification_counter = 0;
    g_zvs_qual_timeout_ms = 0;
    #endif
    
    g_start_cooking_flag = false; 

    g_clock_save_beep_phaser = 0; 
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
    // (v2.9.2) Видалено: if (g_state == STATE_SLEEPING) return;
    
    // (v2.6.9) Дозволяємо +30 сек ТІЛЬКИ в ручному режимі
    if (key == KEY_START_QUICKSTART && 
        g_state == STATE_COOKING && 
        g_active_auto_program_type == PROGRAM_NONE) 
    {
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

    if (key == KEY_START_QUICKSTART && g_state == STATE_IDLE) {
        g_cook_time_total_sec = 30; 
        g_quick_start_delay_ms = 1000; 
        g_state = STATE_QUICK_START_PREP; 
        g_active_auto_program_type = PROGRAM_NONE; 
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
            
            // (v2.6.3) Відновлено логіку v1.8.1 
            else if (key==KEY_10_MIN || key==KEY_1_MIN || key==KEY_10_SEC) { 
                g_input_min_tens=0;
                g_input_min_units=0; 
                g_input_sec_tens=0; 
                handle_time_input_odometer(key); 
                g_state = STATE_SET_TIME; 
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
                
                g_start_cooking_flag = true; // (v2.6.3) Безпечно
                
                if (allow_beep) do_short_beep(); 
            }
            break;
            
        // (v2.9.2) Видалено: case STATE_SLEEPING: 
            
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
                    g_start_cooking_flag = true; // (v2.6.3) Безпечно
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

            // (v2.6.3) Відновлено логіку v1.8.1
            else if (key == KEY_START_QUICKSTART) { 
                g_cook_time_total_sec=(g_input_min_tens*10+g_input_min_units)*60+(g_input_sec_tens*10);
                g_stage2_time_sec=g_cook_time_total_sec; 
                g_stage2_power=g_cook_power_level; 
                g_cook_time_total_sec=g_stage1_time_sec; 
                g_cook_power_level=g_stage1_power; 
                g_active_auto_program_type = PROGRAM_NONE; 
                g_was_two_stage_cook = true; 
                
                if(g_cook_time_total_sec>0) { 
                    g_start_cooking_flag = true; // (v2.6.3) Безпечно
                    if (allow_beep) do_short_beep(); 
                } else reset_to_idle(); 
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
                    g_start_cooking_flag = true; // (v2.6.3) Безпечно
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
            
        #if (ZVS_MODE != 0)
        case STATE_ZVS_QUALIFICATION: // (v2.9.0)
            if (key == KEY_STOP_RESET) { 
                reset_to_idle(); 
                if (allow_beep) do_short_beep(); 
            }
            break;
        #endif
            
        default: 
            reset_to_idle();
    }
}

// ============================================================================
// --- 9. ГОЛОВНИЙ ЦИКЛ (main, setup, loop) ---
// ============================================================================

void setup() {
    setup_display_pins(); // з display_driver
    setup_hardware();     // з cooking_logic
    #if ENABLE_KEYPAD
        keypad_init();    // з keypad_driver
    #endif
    
    // 🔽🔽🔽 (v2.9.32) Змінено на 500µs 🔽🔽🔽
    setup_timer1_500us();   // з timers_isr
    
    #if (ZVS_MODE!=0)
        MCUCR|=(1<<ISC01); MCUCR&=~(1<<ISC00); GIMSK|=(1<<INT0); 
        // (v2.9.2) Видалено: g_zvs_watchdog_counter=0; g_zvs_present=false;
    #endif
    
    reset_to_idle(); 
    
    sei();
}

void loop() {
    static char s_lps=0; static uint16_t s_lht=0; char cks=0;
    static bool s_last_door_state = false;

    // Обробник прапора безпечного старту (v2.6.3)
    if (g_start_cooking_flag) {
        g_start_cooking_flag = false;
        
        // (v2.9.0) ІНВЕРТОВАНА ПЕРЕВІРКА ДВЕРЕЙ (0В=Відкрито)
        if (!(CDD_PIN & CDD_BIT)) {
             reset_to_idle();
             do_short_beep();
        } else {
            #if (ZVS_MODE != 0)
            // (v2.9.0) ЛОГІКА ZVS КВАЛІФІКАЦІЇ
            g_state = STATE_ZVS_QUALIFICATION;
            g_zvs_qualification_counter = 0;
            g_zvs_qual_timeout_ms = ZVS_QUAL_TIMEOUT_MS;
            #else
            // ZVS_MODE 0 (чистий релейний)
            if (!start_cooking_cycle()) {
                reset_to_idle(); 
            }
            #endif
        }
    }
    
    #if (ZVS_MODE != 0)
    // --- Обробник стану ZVS_QUALIFICATION (v2.9.8 - Спрощено) ---
    if (g_state == STATE_ZVS_QUALIFICATION) {
        
        // (v2.9.3) ОБРОБКА ТАЙМАУТУ ПЕРЕНЕСЕНА З ISR
        if (g_zvs_qual_timeout_ms == 0) {
            // Таймаут стався (встановлено в ISR), а імпульси не зібрано
            if (g_zvs_qualification_counter < ZVS_QUALIFICATION_COUNT) {
                do_short_beep();
                reset_to_idle();
                goto loop_end_skip; // Пропускаємо решту loop()
            }
        }
        
        // (v2.9.8) Перевірка успіху (без 16-бітної математики)
        if (g_zvs_qualification_counter >= ZVS_QUALIFICATION_COUNT) {
            // Успіх! (Отримали 3 імпульси)
            g_state = STATE_IDLE; 
            if (!start_cooking_cycle()) {
                reset_to_idle(); 
            }
        }
    }
    #endif

    // (v2.8.6) Логіка g_1sec_tick_flag
    if (g_1sec_tick_flag) {
        g_1sec_tick_flag = false; 
        run_1sec_tasks();         
    }

    // Обробник переходу між 2-ма етапами
    if (g_state == STATE_STAGE2_TRANSITION) {
        uint32_t elapsed_off_time = g_millis_counter - g_magnetron_last_off_timestamp_ms;
        
        if (g_magnetron_last_off_timestamp_ms == 0 || elapsed_off_time > ((uint32_t)MIN_SAFE_OFF_TIME_SEC * 1000UL + 100UL)) 
        {
            g_cook_time_total_sec = g_stage2_time_sec; 
            g_cook_power_level = g_stage2_power; 
            g_stage2_time_sec = 0; 
            
            g_start_cooking_flag = true; // (v2.6.3) Безпечно
        }
    }
    
    // (v2.9.2) ВИДАЛЕНО обгортку if(g_state!=STATE_SLEEPING)
    
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
    // (v2.9.0) 5В=Закрито, 0В=Відкрито
    bool door_is_open = !(CDD_PIN & CDD_BIT);
    if (door_is_open != s_last_door_state) {
        if (door_is_open) {
            if (g_state == STATE_COOKING) { g_state = STATE_PAUSED; g_door_open_during_pause = true; set_magnetron(false); set_fan(false); } 
            else if (g_state == STATE_PAUSED || g_state == STATE_FLIP_PAUSE) { g_door_open_during_pause = true; g_flip_beep_timeout_ms = 0; } 
            else if (g_state == STATE_FINISHED || g_state == STATE_POST_COOK) { reset_to_idle(); } 
            #if (ZVS_MODE != 0)
            else if (g_state != STATE_LOCKED && g_state != STATE_STAGE2_TRANSITION && g_state != STATE_ZVS_QUALIFICATION) { g_door_overlay_timer_ms = 2000; }
            #else
            else if (g_state != STATE_LOCKED && g_state != STATE_STAGE2_TRANSITION) { g_door_overlay_timer_ms = 2000; }
            #endif
        } else { 
            if (g_door_open_during_pause) g_door_open_during_pause = false;
            if (g_door_overlay_timer_ms > 0) g_door_overlay_timer_ms = 0;
        }
        s_last_door_state = door_is_open;
    }

    // --- Логіка обробки кнопок ---
    bool allow_keys=true;
    #if (ZVS_MODE != 0)
    if(g_state==STATE_LOCKED || g_state==STATE_CLOCK_SAVED || g_door_overlay_timer_ms > 0 || g_state == STATE_STAGE2_TRANSITION || g_state == STATE_ZVS_QUALIFICATION) 
        allow_keys=false;
    #else
    if(g_state==STATE_LOCKED || g_state==STATE_CLOCK_SAVED || g_door_overlay_timer_ms > 0 || g_state == STATE_STAGE2_TRANSITION) 
        allow_keys=false;
    #endif
    
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
        
        if(cks!=s_lps) { 
            if(cks==0) { 
                char released_key = s_lps; 
                if(released_key != 0 && g_last_key_hold_duration <= 500) {
                    handle_state_machine(released_key, true);
                }
            }
            s_lps=cks; 
        }

    } else { s_lps=0; s_lht=0; }
    
    // (v2.8.6)
    if(g_state==STATE_FINISHED || g_state==STATE_POST_COOK) {
        handle_state_machine(0, false);
    }
    
    // (v2.9.2) Видалено перевірку STATE_SLEEPING
    update_display(); 
    
    loop_end_skip:; // (v2.9.3) Мітка для goto
}

int main(void) { 
    setup(); 
    while(1) { 
        loop(); 
    } 
    return 0; 
}