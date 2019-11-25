/*
 * lcd_wayland.h
 *
 *  Created on: 2019Äê11ÔÂ18ÈÕ
 *      Author: zjm09
 */

#ifndef UI_AWTK_WAYLAND_LCD_WAYLAND_H_
#define UI_AWTK_WAYLAND_LCD_WAYLAND_H_

#include "pthread_signal.h"
#include "wayland_tools.h"
#include "tkc/mem.h"
#include "base/lcd.h"
#include "lcd/lcd_mem_bgr565.h"
#include "lcd/lcd_mem_rgb565.h"
#include "lcd/lcd_mem_bgra8888.h"
#include "lcd/lcd_mem_rgba8888.h"



typedef struct{
//	struct double_buffer_list *current;
	struct wayland_data objs;
	void *impl_data;
}lcd_wayland_t;

lcd_wayland_t *lcd_wayland_create(void);

#endif /* UI_AWTK_WAYLAND_LCD_WAYLAND_H_ */
