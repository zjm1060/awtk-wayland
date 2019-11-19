/*
 * wayland_tools.c
 *
 *  Created on: 2019��9��23��
 *      Author: zjm09
 */
#include "wayland_tools.h"

static void output_mode_cb(void *data, struct wl_output *out, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh_rate) {
	struct output_info *info = data;
	info->width = width;
	info->height = height;
	info->hertz = refresh_rate;
}

static void output_geometry_cb(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	struct output_info *info = data;

	info->transform = transform;
}

static const struct wl_output_listener output_listener = { output_geometry_cb,
		output_mode_cb, do_nothing, do_nothing };

static void shell_ping_respond(void *data, struct zxdg_shell_v6 *shell,
		uint32_t serial) {
	(void) data;
	zxdg_shell_v6_pong(shell, serial);
}

static const struct zxdg_shell_v6_listener xdg_shell_listener = {
		shell_ping_respond };

extern const struct wl_keyboard_listener xkb_keyboard_listener;
extern const struct wl_pointer_listener pointer_listener;

static void calc_capabilities(void *data, struct wl_seat *s, uint32_t cap)
{
	struct wayland_data *objs = data;
	struct input_bundle *inputs = &(objs->inputs);
	(void) s;
	//Cap is a bitfield. The WL_SEAT_CAPABILITY_XXX enum is a mask
	// that selects the corresponding bit.
	inputs->seat = s;
	if (cap & WL_SEAT_CAPABILITY_KEYBOARD) {
		printf("Has a keyboard.\n");
		if(inputs->keyboard.kbd == NULL){
			inputs->keyboard.kbd = wl_seat_get_keyboard(s);
			wl_keyboard_add_listener(inputs->keyboard.kbd, &xkb_keyboard_listener,
					&inputs->keyboard);
		}
	}

	if (cap & WL_SEAT_CAPABILITY_POINTER) {
		printf("Has a pointer.\n");
		inputs->mouse.pointer = wl_seat_get_pointer(s);
		if(inputs->mouse.pointer ){
//			struct wl_cursor_theme *cursor_theme = wl_cursor_theme_load(NULL, 1, objs->shm);
			struct wl_cursor *default_cursor = wl_cursor_theme_get_cursor(inputs->mouse.cursor_theme, "left_ptr");
			struct wl_cursor_image *image = default_cursor->images[0];
			struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
			inputs->mouse.point_surface =
					wl_compositor_create_surface(objs->compositor);
			wl_pointer_set_cursor(inputs->mouse.pointer, 1, inputs->mouse.point_surface, 0, 0);
			wl_surface_attach(inputs->mouse.point_surface, buffer, 0, 0);
			wl_surface_damage(inputs->mouse.point_surface, 0, 0,
					image->width, image->height);
			wl_surface_commit(inputs->mouse.point_surface);
			wl_pointer_add_listener(inputs->mouse.pointer, &pointer_listener, &inputs->mouse);
		}
	}else{
		if(inputs->mouse.pointer){
			wl_surface_destroy(inputs->mouse.point_surface);
			wl_pointer_release(inputs->mouse.pointer);
			inputs->mouse.pointer = NULL;
		}
	}

	if (cap & WL_SEAT_CAPABILITY_TOUCH)
		printf("Has a touchscreen.\n");
}

static const struct wl_seat_listener seat_listener = {
		calc_capabilities,
		do_nothing,
};

static void global_registry_handler(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version) {
	struct wayland_data *objs = data;
	printf("Got a registry event for %s id %d version %d \n", interface, id,
			version);
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		objs->compositor = wl_registry_bind(registry, id,
				&wl_compositor_interface, 4);
		objs->surface = wl_compositor_create_surface(objs->compositor);
	} else if (strcmp(interface, zxdg_shell_v6_interface.name) == 0) {
		objs->shell = wl_registry_bind(registry, id, &zxdg_shell_v6_interface,
				1);
		zxdg_shell_v6_add_listener(objs->shell, &xdg_shell_listener, NULL);
	} else if (strcmp(interface, wl_shm_interface.name) == 0)
		objs->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		objs->inputs.seat = wl_registry_bind(registry, id, &wl_seat_interface,
				5);
		wl_seat_add_listener(objs->inputs.seat, &seat_listener, objs);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *wl_out = wl_registry_bind(registry, id,
				&wl_output_interface, 2);
		// TODO Free
		struct wayland_output *ua_out = malloc(sizeof(struct wayland_output));
		ua_out->out = wl_out;
		wl_output_add_listener(ua_out->out, &output_listener, &ua_out->info);
		wl_list_insert(objs->monitors, &ua_out->link);
	}

}

static const struct wl_registry_listener registry_listener = {
		global_registry_handler,
		do_nothing,
};

static void surface_configure_cb(void *data, struct zxdg_surface_v6 *surface,
		uint32_t serial) {
	(void) data;
	zxdg_surface_v6_ack_configure(surface, serial);
}

static const struct zxdg_surface_v6_listener xdg_surface_listener = {
		surface_configure_cb,
};

static const struct zxdg_toplevel_v6_listener xdg_top_listener = {
		do_nothing,
		do_nothing
};

WAYLAND_SETUP_ERR setup_wayland(struct wayland_data *objs, int fullscreen) {
	memset(objs, 0, sizeof(struct wayland_data));
	objs->monitors = malloc(sizeof(struct wl_list));
	wl_list_init(objs->monitors);

	memset(&objs->inputs, 0, sizeof(struct input_bundle));
	objs->inputs.keyboard.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);


	objs->display = wl_display_connect(NULL);
	if (objs->display == NULL)
		return NO_WAY_DISP;

	objs->registry = wl_display_get_registry(objs->display);
	if (objs->registry == NULL)
		return NO_REG;
	wl_registry_add_listener(objs->registry, &registry_listener, objs);
	wl_display_roundtrip(objs->display); // Wait for registry listener to run
	if (objs->compositor == NULL)
		return NO_COMP;
	if (objs->shell == NULL)
		return NO_SHELL;
	if (objs->inputs.seat == NULL)
		return NO_SEAT;
	if (objs->shm == NULL)
		return NO_SHM;
	objs->inputs.mouse.cursor_theme = wl_cursor_theme_load(NULL, 1, objs->shm);
	if (wl_list_empty(objs->monitors))
		return NO_MONITORS;
	if (objs->surface == NULL)
		return NO_SURFACE;

	objs->shell_surface = zxdg_shell_v6_get_xdg_surface(objs->shell,
			objs->surface);
	if (objs->shell_surface == NULL)
		return NO_SHELL_SURFACE;

	zxdg_surface_v6_add_listener(objs->shell_surface, &xdg_surface_listener,
	NULL);

	objs->top = zxdg_surface_v6_get_toplevel(objs->shell_surface);
	if (objs->top == NULL)
		return NO_TOPLEVEL;

	if (fullscreen) {
		zxdg_toplevel_v6_set_maximized(objs->top);
		zxdg_toplevel_v6_set_title(objs->top, "Unknown Animal");
		wl_display_roundtrip(objs->display);

		struct wayland_output *out = container_of(objs->monitors->next,
				struct wayland_output, link);

		zxdg_toplevel_v6_set_fullscreen(objs->top, out->out);
		zxdg_toplevel_v6_add_listener(objs->top, &xdg_top_listener, NULL);
	}
	wl_surface_commit(objs->surface);

	wl_display_roundtrip(objs->display);

	return SETUP_OK;
}

void destroy_wayland_data(struct wayland_data *objs) {
	struct wayland_output *out = NULL;
	struct wl_list *head = objs->monitors;
	struct wl_list *current = head->next;

	while (current != head) {
		struct wl_list *next = current->next;
		out = container_of(current, struct wayland_output, link);
		wl_output_destroy(out->out);
		free(out);
		current = next;
	}
	free(head);
	xkb_keymap_unref(objs->inputs.keyboard.map);
	xkb_state_unref(objs->inputs.keyboard.kb_state);
	xkb_context_unref(objs->inputs.keyboard.ctx);
	wl_keyboard_destroy(objs->inputs.keyboard.kbd);

//  free (objs->inputs.keyboard.pressed.data);
//  free (objs->inputs.keyboard.pressed.valid);

	wl_pointer_destroy(objs->inputs.mouse.pointer);
	wl_seat_destroy(objs->inputs.seat);
	wl_shm_destroy(objs->shm);
	zxdg_toplevel_v6_destroy(objs->top);
	zxdg_surface_v6_destroy(objs->shell_surface);
	zxdg_shell_v6_destroy(objs->shell);
	wl_surface_destroy(objs->surface);
	wl_compositor_destroy(objs->compositor);
	wl_registry_destroy(objs->registry);
	wl_display_disconnect(objs->display);
}

static void buffer_release_cb(void *data, struct wl_buffer *buf) {
	(void) buf;
	struct buffer *b = data;
	ThreadSignal_Signal(&b->used);
}

static const struct wl_buffer_listener buffer_listener = { buffer_release_cb };

struct buffer *wayland_create_double_buffer(struct wl_shm *shm, int width,
		int height) {
	size_t size = (width * height * 4) * 2;
	int fd = shm_open("/wayland_frame_buffer",
	O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	ftruncate(fd, size);

	void *raw = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (raw == MAP_FAILED) {
		perror("Could not map file to memory.\n");
		return MAP_FAILED;
	}

	struct buffer *buffer = calloc(1, sizeof(struct buffer));
	buffer->width = width;
	buffer->height = height;

	buffer->bufs = malloc(sizeof(struct double_buffer_list));

	struct wl_shm_pool * pool = wl_shm_create_pool(shm, fd, size);

	buffer->bufs->wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
			width * 4, WL_SHM_FORMAT_ARGB8888);
	if (buffer->bufs->wl_buffer == NULL)
		return NULL;

	ThreadSignal_Init(&buffer->used);

	buffer->bufs->pixels = raw;

	wl_buffer_add_listener(buffer->bufs->wl_buffer, &buffer_listener,
			buffer);

	struct double_buffer_list *back_buf = malloc(
			sizeof(struct double_buffer_list));
	back_buf->wl_buffer = wl_shm_pool_create_buffer(pool, 4 * width * height,
			width, height, width * 4, WL_SHM_FORMAT_ARGB8888);
	if (back_buf == NULL)
		return NULL;

	back_buf->pixels = ((uint32_t*) raw) + width * height;
//	back_buf->used = false;

	wl_buffer_add_listener(back_buf->wl_buffer, &buffer_listener,
			buffer);

	buffer->bufs->next = back_buf;
	back_buf->next = buffer->bufs;

	return buffer;
}

void ref_display(struct wl_surface *surface, struct wl_buffer *buffer,
		int width, int height) {
	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_damage(surface, 0, 0, width, height);
	wl_surface_commit(surface);
}
