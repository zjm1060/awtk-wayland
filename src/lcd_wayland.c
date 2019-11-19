/*
 * lcd_wayland_1.c
 *
 *  Created on: 2019��11��18��
 *      Author: zjm09
 */
#include "lcd_wayland.h"

#include "base/idle.h"
#include "base/timer.h"
#include "base/window_manager.h"
#include "main_loop/main_loop_simple.h"
#include "tkc/thread.h"

enum key_repeat_state{
	repeat_key_released = 0,
	repeat_key_pressed = 10,
	repeat_key_delay,
	repeat_key_rate,
};

enum key_repeat_state __repeat_state = repeat_key_released;
static int key_value;

static ret_t wayland_flush(lcd_t* lcd)
{
	lcd_wayland_t *lw = lcd->impl_data;
	ref_display(lw->objs.surface,lw->current->wl_buffer,lcd->w,lcd->h);

	return RET_OK;
}

static ret_t wayland_sync(lcd_t* lcd)
{
	lcd_wayland_t *lw = lcd->impl_data;
	struct buffer *buf = lw->impl_data;
	ThreadSignal_Wait(&buf->used);
	return RET_OK;
}

static ret_t wayland_begin_frame(lcd_t* lcd, rect_t* dirty_rect) {
	lcd_wayland_t *lw = lcd->impl_data;
	struct buffer *buf = lw->impl_data;
	lcd_mem_t* lcd_mem = (lcd_mem_t*)(lcd);

	lw->current = lw->current->next;
	lcd_mem->offline_fb = (void *)lw->current->pixels;

	return RET_OK;
}

static ret_t input_dispatch_to_main_loop(void* ctx, const event_queue_req_t* e) {
	main_loop_queue_event((main_loop_t*)ctx, e);
	return RET_OK;
}

extern int32_t map_key(uint8_t code);
static void key_input_dispatch(int state,int key)
{
	event_queue_req_t req;

    req.event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? EVT_KEY_DOWN : EVT_KEY_UP;
    req.key_event.key = map_key(key);

    input_dispatch_to_main_loop(main_loop(), &(req));

    req.event.type = EVT_NONE;

    if(state == WL_KEYBOARD_KEY_STATE_PRESSED){
    	__repeat_state = repeat_key_pressed;
    	key_value = key;
    }else{
    	__repeat_state = repeat_key_released;
    }

}

static void mouse_point_dispatch(int state,int button, int x,int y)
{
	pointer_event_t event;
	widget_t* widget = main_loop()->wm;
	static int __x,__y;

	if(x > 0){
		__x = x;
	}

	if(y > 0){
		__y = y;
	}

	switch(state){
		case WL_POINTER_BUTTON_STATE_PRESSED:
			pointer_event_init(&event, EVT_POINTER_DOWN, widget, __x, __y);
			event.button = button;
			event.pressed = 1;
			break;
		case WL_POINTER_BUTTON_STATE_RELEASED:
			pointer_event_init(&event, EVT_POINTER_UP, widget, __x, __y);
			event.button = button;
			event.pressed = 0;
			break;
		default:
			pointer_event_init(&event, EVT_POINTER_MOVE, widget, __x, __y);
			event.button = button;
			event.pressed = 0;
			break;
	}

    input_dispatch_to_main_loop(main_loop(), (event_queue_req_t *)&event);
}

static lcd_t* lcd_linux_create_flushable(lcd_wayland_t *lw)
{
	struct wayland_data *objs = &lw->objs;
	struct wayland_output *out = container_of ( objs->monitors->next,
		  struct wayland_output,
		  link);
	size_t width = out->info.width;
	size_t height = out->info.height;

	if(out->info.transform == WL_OUTPUT_TRANSFORM_90 || out->info.transform ==WL_OUTPUT_TRANSFORM_270){
		height = out->info.width;
		width = out->info.height;
	}

	int line_length = width * 4;

	struct buffer *buffer = wayland_create_double_buffer(objs->shm,width,height);

	uint8_t* online_fb = (void*)buffer->bufs->pixels;
	uint8_t* offline_fb = (void*)buffer->bufs->next->pixels;

	lw->current = buffer->bufs;
	lw->impl_data = buffer;

	lcd_t *lcd = lcd_mem_bgra8888_create_double_fb(width, height, online_fb, offline_fb);

	if(lcd != NULL) {
		lcd->impl_data = lw;
		lcd->sync = wayland_sync;
		lcd->flush = wayland_flush;
		lcd->begin_frame = wayland_begin_frame;
		lcd_mem_set_line_length(lcd, line_length);
	}

	lw->objs.inputs.keyboard.kb_xcb = key_input_dispatch;
	lw->objs.inputs.mouse.point_xcb = mouse_point_dispatch;

	return lcd;
}

static void kb_repeat(struct wayland_data *objs)
{
	static uint32_t repeat_count = 0;
	while(1){
		usleep(10*1000);
		switch(__repeat_state){
			case repeat_key_pressed:
				repeat_count = 0;
				__repeat_state = repeat_key_delay;
				break;
			case repeat_key_delay:
				repeat_count ++;
				if(repeat_count >= 20){
					repeat_count = 0;
					__repeat_state = repeat_key_rate;
				}
				break;
			case repeat_key_rate:
				repeat_count ++;
				if((repeat_count % 2) == 0){
					event_queue_req_t req;

					req.event.type = EVT_KEY_DOWN;
					req.key_event.key = map_key(key_value);
					input_dispatch_to_main_loop(main_loop(), &(req));
				}
				break;
			default:
			case repeat_key_released:
				break;
		}
	}
}

lcd_t *lcd_wayland_create(void)
{
	lcd_wayland_t *lw = calloc(1,sizeof(lcd_wayland_t));
	if (lw && setup_wayland (&lw->objs,0) != SETUP_OK){
		destroy_wayland_data (&lw->objs);
		return NULL;
	}

	tk_thread_t* thread = tk_thread_create(kb_repeat, NULL);
	if (thread != NULL) {
		tk_thread_start(thread);
	}

	return lcd_linux_create_flushable(lw);
}


