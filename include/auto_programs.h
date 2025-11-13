#ifndef AUTO_PROGRAMS_H_
#define AUTO_PROGRAMS_H_

#include "microwave_firmware.h"

// ============================================================================
// --- 🥩 ЗОВНІШНІ ОГОЛОШЕННЯ ТАБЛИЦЬ PROGMEM (v2.8.6) ---
// ============================================================================
// (Визначені в auto_programs.c, використовуються в microwave_firmware.c)

extern const FlipSchedule def_meat_flips[] PROGMEM;
extern const FlipSchedule def_poultry_flips[] PROGMEM;
extern const FlipSchedule def_fish_flips[] PROGMEM;
extern const AutoProgramEntry ac1_potato[] PROGMEM;
extern const AutoProgramEntry ac2_fresh_veg[] PROGMEM;
extern const AutoProgramEntry ac3_frozen_veg[] PROGMEM;
extern const AutoProgramEntry def1_meat[] PROGMEM;
extern const AutoProgramEntry def2_poultry[] PROGMEM;
extern const AutoProgramEntry def3_fish[] PROGMEM;
extern const AutoProgramEntry def4_bread[] PROGMEM;


// ============================================================================
// --- 🟨 ПРОТОТИПИ ФУНКЦІЙ ---
// ============================================================================

/**
 * @brief Розраховує час (у секундах, що залишилися) для перевертань.
 * Зберігає результат у g_defrost_flip_info.
 */
void calculate_flip_schedule(uint8_t program_num, uint16_t weight);

/**
 * @brief Переводить МК у стан FLIP_PAUSE, зупиняє магнетрон/вентилятор,
 * та запускає серію звукових сигналів.
 */
void initiate_flip_pause(void);

/**
 * @brief Відновлює готування після перевертання.
 * Інкрементує індекс, вмикає вентилятор/магнетрон, скидає таймер g_timer_ms.
 */
void resume_after_flip(void);

/**
 * @brief Перевіряє, чи не настав час для наступного перевертання.
 * Викликається щосекунди з update_cook_timer().
 */
void check_flip_required(void);

/**
 * @brief Знаходить у PROGMEM таблиці потрібний час/потужність для ваги.
 * Записує результат у g_cook_time_total_sec та g_cook_power_level.
 */
void get_program_settings(const AutoProgramEntry* table, uint8_t len, uint16_t weight);


#endif // AUTO_PROGRAMS_H_