/*
 * main_loop_wayland.1.c
 *
 *  Created on: 2019Äê11ÔÂ18ÈÕ
 *      Author: zjm09
 */
#include <poll.h>
#include "main_loop/main_loop_simple.h"
#include "tkc/thread.h"

#include "../awtk-wayland/lcd_wayland.h"

static void *wayland_run(void* ctx);

static ret_t main_loop_linux_destroy(main_loop_t* l) {
  main_loop_simple_t* loop = (main_loop_simple_t*)l;

  main_loop_simple_reset(loop);

  return RET_OK;
}

main_loop_t* main_loop_init(int w, int h) {
	lcd_t *lcd = lcd_wayland_create();

	return_value_if_fail(lcd != NULL, NULL);
	main_loop_simple_t *loop = main_loop_simple_init(lcd->w, lcd->h);

	loop->base.destroy = main_loop_linux_destroy;
	canvas_init(&(loop->base.canvas), lcd, font_manager());

	tk_thread_t* thread = tk_thread_create(wayland_run, lcd);
	if (thread != NULL) {
		tk_thread_start(thread);
	}

	return (main_loop_t*)loop;
}

static void PlatformPollEvents(struct wayland_data *objs)
{
    struct wl_display* display = objs->display;
    struct pollfd fds[] = {
        { wl_display_get_fd(display), POLLIN },
    };

    while (wl_display_prepare_read(display) != 0)
        wl_display_dispatch_pending(display);
    wl_display_flush(display);
    if (poll(fds, 1, 50) > 0)
    {
        wl_display_read_events(display);
        wl_display_dispatch_pending(display);
    }
    else
    {
        wl_display_cancel_read(display);
    }
}

static void* wayland_run(void* ctx)
{
	lcd_wayland_t* lw = ((lcd_t *)ctx)->impl_data;

	while(1){
		PlatformPollEvents(&lw->objs);
	}
}

