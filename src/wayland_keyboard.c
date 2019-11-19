/*
 * wayland_keyboard.c
 *
 *  Created on: 2019Äê9ÔÂ23ÈÕ
 *      Author: zjm09
 */
#include "../awtk-wayland/wayland_tools.h"

void
keymap_format_cb (void *data, struct wl_keyboard *keyboard, uint32_t format,
                  int32_t fd, uint32_t keymap_size )
{
  struct keyboard *kbd = data;
  char *str = mmap (NULL, keymap_size, PROT_READ, MAP_SHARED, fd, 0);
  xkb_keymap_unref (kbd->map);
  kbd->map = xkb_keymap_new_from_string (kbd->ctx, str, XKB_KEYMAP_FORMAT_TEXT_V1,
                                         XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap (str, keymap_size);
  close (fd);
  xkb_state_unref (kbd->kb_state);
  kbd->kb_state = xkb_state_new (kbd->map);

}

extern void key_input(int,int);
void
key_cb (void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
        uint32_t key, uint32_t state)
{
  (void) keyboard;
  (void) serial;
  (void) time;
  struct keyboard *keydata = data;
	  if(keydata->kb_xcb){
		  keydata->kb_xcb(state,key);
	  }
}

static void
motion_pointer_cb (void *data, struct wl_pointer *pointer, uint32_t time,
                   wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct point *point = data;
	if(point->point_xcb){
		point->point_xcb(-1,0,wl_fixed_to_int(surface_x),wl_fixed_to_int(surface_y));
	}
}

static void
button_pointer_cb (void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	struct point *point = data;

	if(point->point_xcb){
		point->point_xcb(state,button,-1,-1);
	}
}

static void enter_pointer_cb(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
//	printf ("enter_pointer_cb .\n");
}

const struct wl_pointer_listener pointer_listener =
  { enter_pointer_cb,
    do_nothing,
    motion_pointer_cb,
    button_pointer_cb,
    do_nothing,
    do_nothing,
    do_nothing,
    do_nothing,
    do_nothing };

static void _keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
    int32_t rate, int32_t delay)
{
    printf("repeat_info: rate %d, delay %d\n", rate, delay);
}

const struct wl_keyboard_listener xkb_keyboard_listener =
{
    keymap_format_cb,
    do_nothing,
    do_nothing,
    key_cb,
    do_nothing,
	_keyboard_repeat_info,
};

