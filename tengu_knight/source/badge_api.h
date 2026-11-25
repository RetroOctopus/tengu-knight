/* Observed ABI of the supplied TenguCon 3.0 firmware (69d9791-dirty).
 * This is a small recovered interface, not an official badge SDK.
 * Function signatures were checked against the firmware and its sample ELF.
 */
#ifndef TENGUCON_BADGE_API_H
#define TENGUCON_BADGE_API_H

#include <stdint.h>

extern void badge_clear(uint16_t color);
extern void badge_fill_rect(int32_t x, int32_t y, int32_t width,
                            int32_t height, uint16_t color);
/* On hardware, y is the TOP of the text, not the baseline. */
extern void badge_draw_text(int32_t x, int32_t y, const char *text,
                            uint16_t color, int32_t font_index);
extern int32_t badge_screen_width(void);
extern int32_t badge_screen_height(void);

/* Key code (row << 4) | column, both counted from 1; -1 means no key press.
 * This function takes NO argument. Codes are not ASCII characters.
 */
extern int32_t badge_key_pressed(void);
extern int32_t badge_key_held(int32_t matrix_code);
extern uint32_t badge_millis(void);
extern void badge_delay_ms(uint32_t milliseconds);

#endif
