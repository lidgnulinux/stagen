/*-
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <stdio.h>
#include <stdbool.h>

#include <linux/input-event-codes.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <libinput.h>

#include "conf/config.h"
#define dbg_printf(args...)

enum stage_cursor_mode {
	STAGE_CURSOR_PASSTHROUGH,
	STAGE_CURSOR_MOVE,		/* mod + left mouse button + move */
	STAGE_CURSOR_RESIZE,		/* mod + right mouse button + move */
	STAGE_CURSOR_SCROLL,		/* left mouse button + move */
};

struct stage_server {
	struct wl_display *wl_disp;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_presentation *presentation;
	struct wl_listener new_output;
	struct wl_listener new_xdg_surface;
	struct wl_list views;
	struct wlr_xdg_shell *xdg_shell;
	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_cursor *cursor;
	struct wlr_input_device *device;

	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;
	struct wl_list keyboards;
	enum stage_cursor_mode cursor_mode;

	struct stage_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;
	double cur_saved_x;
	double cur_saved_y;

	int current_layout;

	struct wlr_input_inhibit_manager *inhibit_manager;

	struct wlr_compositor *compositor;
	struct wlr_xdg_activation_v1 *activation;
	struct wl_listener request_activate;
	struct wl_listener layout_change;

	struct wlr_output_manager_v1 *output_manager;
	struct wl_listener output_manager_apply;
	struct wl_listener output_manager_test;

	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_manager;
	struct wl_listener new_virtual_pointer;
};

struct stage_output {
	struct wl_list link;
	struct stage_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	int curws;
	struct wlr_scene_rect *background;
};

enum stage_view_type {
	VIEW_XDG,
};

struct stage_view {
	struct wl_list link;
	struct stage_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_minimize;
	struct wl_listener commit;
	int x, y, w, h;
	int sx, sy, sw, sh;	/* saved */
	int maxverted;
	int maximized;
	struct wlr_scene_rect *rect[4];	/* borders */
	bool was_focused;

	enum stage_view_type type;
};

#define	N_WORKSPACES	10

#define	STAGE_MODIFIER	WLR_MODIFIER_LOGO

#define END(A)                  ((A) + LENGTH(A))
#define LENGTH(X)               (sizeof X / sizeof X[0])

#define SHFTKEY WLR_MODIFIER_SHIFT

static struct stage_workspace {
	struct wl_list views;
} workspaces[N_WORKSPACES];

#define TERMINAL_FONT_WIDTH 12

struct stage_keyboard {
	struct wl_list link;
	struct stage_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wlr_input_device *device;
	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

static int enable_exczone = 1;

static void
create_borders(struct stage_view *view)
{
	int i;

	for (i = 0; i < 4; i++)
		view->rect[i] = wlr_scene_rect_create(view->scene_tree, 0, 0,
		    color_default);
}

static void
update_borders(struct stage_view *view)
{
	/* left */
	wlr_scene_node_set_position(&view->rect[0]->node, 0, 0);
	wlr_scene_rect_set_size(view->rect[0], 1, view->h);

	/* top */
	wlr_scene_node_set_position(&view->rect[1]->node, 0, 0);
	wlr_scene_rect_set_size(view->rect[1], view->w, 1);

	/* bottom */
	wlr_scene_node_set_position(&view->rect[2]->node, 0, view->h - 1);
	wlr_scene_rect_set_size(view->rect[2], view->w, 1);

	/* right */
	wlr_scene_node_set_position(&view->rect[3]->node, view->w - 1, 0);
	wlr_scene_rect_set_size(view->rect[3], 1, view->h);

	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, view->w, view->h);
}

static struct stage_output *
output_at(struct stage_server *server, double x, double y)
{
	struct wlr_output *o;

	o = wlr_output_layout_output_at(server->output_layout, x, y);
	if (o)
		return (o->data);

	return (NULL);
}

static struct stage_output *
cursor_at(struct stage_server *server)
{
	struct stage_output *out;

	out = output_at(server, server->cursor->x, server->cursor->y);

	assert(out != NULL);

	return (out);
}

static void
view_geometry(struct stage_view *view, struct wlr_box *geom)
{
	wlr_xdg_surface_get_geometry(view->xdg_surface, geom);
}

static void
view_set_borders_active(struct stage_view *view, bool active)
{
	const float *color;
	int i;

	if (active)
		color = color_focused;
	else
		color = color_default;

	for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(view->rect[i], color);
}

static struct wlr_surface *
view_surface(struct stage_view *view)
{
	struct wlr_surface *surface;

	surface = NULL;

	switch (view->type) {
	case VIEW_XDG:
		surface = view->xdg_surface->surface;
		break;
	default:
		break;
	}

	return (surface);
}

static const char *
get_app_id(struct stage_view *view)
{
	const char *res;

	res = view->xdg_surface->toplevel->app_id;

	return (res);
}

static void
focus_view(struct stage_view *view, struct wlr_surface *surface)
{
	struct wlr_surface *prev_surface;
	struct wlr_xdg_surface *previous;
	struct wlr_scene_node *scene_node;
	struct stage_server *server;
	struct wlr_keyboard *kb;
	struct wlr_seat *seat;
	struct stage_view *prev_view;
	struct stage_output *out;
	struct stage_workspace *ws;
	const char *app_id;

	out = cursor_at(view->server);
	
	ws = &workspaces[out->curws];

	if (view == NULL)
		return;

	server = view->server;
	seat = server->seat;

	kb = wlr_seat_get_keyboard(seat);

	prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface)
		return;

	if (prev_surface) {
		previous =
		    wlr_xdg_surface_try_from_wlr_surface(prev_surface);
		assert(previous != NULL);
		assert(previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
		wlr_xdg_toplevel_set_activated(previous->toplevel,
		    false);
		scene_node = previous->data;
		prev_view = scene_node->data;
		view_set_borders_active(prev_view, false);
	}

	wl_list_remove(&view->link);
	wl_list_insert(&ws->views, &view->link);

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") == 0)
		wlr_scene_node_raise_to_top(&view->scene_tree->node);

	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);

	wlr_seat_keyboard_notify_enter(seat, view_surface(view),
	    kb->keycodes, kb->num_keycodes, &kb->modifiers);

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") != 0)
		view_set_borders_active(view, true);
}

static struct stage_view *
desktop_view_at(struct stage_server *server, double lx, double ly,
    struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_surface *scene_surface;
	struct wlr_scene_buffer *scene_buffer;
	struct wlr_scene_node *node;
	struct wlr_scene_tree *tree;
	struct stage_view *view;

#if 0
	out = output_at(server, lx, ly);

	ws = &workspaces[out->curws];
	wl_list_for_each(view, &ws->views, link) {
		box = &view->xdg_toplevel->base->current.geometry;
		if (wlr_box_contains_point(box, lx, ly))
			return (view);
	}

	return (NULL);
#endif

#if 1
	node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER)
		return (NULL);

	if (surface && node->type == WLR_SCENE_NODE_BUFFER) {
		scene_buffer = wlr_scene_buffer_from_node(node);
		scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
		if (scene_surface == NULL)
			return (NULL);

		*surface = scene_surface->surface;
	}

	tree = node->parent;

	while (tree != NULL && tree->node.data == NULL)
		tree = tree->node.parent;

	view = tree->node.data;
#endif

	return (view);
}

static void
view_align(struct stage_view *view)
{
	struct wlr_box geom;
	struct stage_output *out;
	struct wlr_output *output;

	out = cursor_at(view->server);
	output = out->wlr_output;

	wlr_xdg_surface_get_geometry(view->xdg_surface, &geom);

	printf("%s: view geoms %d %d %d %d\n", __func__, geom.x, geom.y,
	    geom.width, geom.height);

	view->w = geom.width;
	view->h = geom.height;
	view->x = (output->width - geom.width) / 2;
	view->y = (output->height - geom.height) / 2;
}

static void
set_client_geometry(struct stage_view *view, int pos_x, int pos_y)
{
	struct wlr_box geom;

	wlr_xdg_surface_get_geometry(view->xdg_surface, &geom);

	printf("%s: view geoms %d %d %d %d\n", __func__, geom.x, geom.y,
	    geom.width, geom.height);

	view->w = geom.width;
	view->h = geom.height;
	view->x = pos_x;
	view->y = pos_y;
}

static void
applyrules(struct stage_view *view)
{
	const char *app_id;
	const Rule *r;

	for (r = rules; r < END(rules); r++) {
		app_id = get_app_id(view);
		if (strcmp(app_id, r->app_id) == 0)
			set_client_geometry(view, r->x, r->y);
	}
}

static void
xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	struct stage_workspace *curws;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_box geom;

	view = wl_container_of(listener, view, map);

	out = cursor_at(view->server);
	curws = &workspaces[out->curws];
	wl_list_insert(&curws->views, &view->link);

#if 0
	enum wlr_edges edges = WLR_EDGE_NONE;
	edges = WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
	wlr_xdg_toplevel_set_tiled(view->xdg_toplevel, edges);
#endif

	view_align(view);

	applyrules(view);

	view_geometry(view, &geom);
	view->w = geom.width;
	view->h = geom.height;

	update_borders(view);
	focus_view(view, view_surface(view));
}

void
request_activate(struct wl_listener *listener, void *data)
{
	printf("%s\n", __func__);
}

void
layout_change(struct wl_listener *listener, void *data)
{
	printf("%s\n", __func__);
}

void
output_manager_apply(struct wl_listener *listener, void *data)
{
	printf("%s\n", __func__);
}

void
output_manager_test(struct wl_listener *listener, void *data)
{
	printf("%s\n", __func__);
}

static struct stage_view *
view_from_surface(struct stage_server *server, struct wlr_surface *surface)
{
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_scene_node *scene_node;
	struct stage_view *view;

	xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
	assert(xdg_surface != NULL);

	scene_node = xdg_surface->data;

	view = scene_node->data;

	assert(view != NULL);

	return (view);
}

static void
changeworkspace(struct stage_server *server, int newws)
{
	struct stage_workspace *ws;
	struct stage_view *view, *tmpview;
	struct stage_view *focused_view;
	struct stage_output *out;
	struct wlr_surface *surface;
	struct wlr_seat *seat;
	int oldws;
	const char *app_id;

	out = cursor_at(server);
	oldws = out->curws;
	out->curws = newws;

	focused_view = NULL;
	seat = server->seat;
	surface = seat->keyboard_state.focused_surface;
	if (surface)
		focused_view = view_from_surface(server, surface);

	dbg_printf("%s: ws %d -> %d\n", __func__, oldws, newws);

	ws = &workspaces[oldws];
	wl_list_for_each_safe(view, tmpview, &ws->views, link) {
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		if (view == focused_view)
			view->was_focused = true;
		else
			view->was_focused = false;
	}

	ws = &workspaces[newws];
	wl_list_for_each_safe(view, tmpview, &ws->views, link) {
		wlr_scene_node_set_enabled(&view->scene_tree->node, true);
		if (view->was_focused)
			focus_view(view, view_surface(view));
	}

	// sticky ?
	ws = &workspaces[oldws];
	wl_list_for_each_safe(view, tmpview, &ws->views, link) {
		app_id = get_app_id(view);
		if (strcmp(app_id, "lxqt-panel") == 0)
			wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	}

}

static void
output_frame(struct wl_listener *listener, void *data)
{
	struct wlr_scene_output *scene_output;
	struct wlr_scene *scene;
	struct stage_output *output;
	struct timespec now;

	dbg_printf("%s\n", __func__);

	output = wl_container_of(listener, output, frame);

	scene = output->server->scene;
	scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
	wlr_scene_output_commit(scene_output, NULL);

	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void
set_layout(struct stage_server *server)
{
	struct xkb_context *context;
	struct xkb_rule_names rules;
	struct xkb_keymap *keymap;
	struct wlr_keyboard *kbd;

	memset(&rules, 0, sizeof(struct xkb_rule_names));
	rules.layout = "us,ru";

	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	kbd = wlr_seat_get_keyboard(server->seat);

	keymap = xkb_keymap_new_from_names(context, &rules,
	    XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(kbd, keymap);
	xkb_context_unref(context);
}

static void
unmaxvert(struct stage_view *view)
{
	view->maxverted = false;

	view->x = view->sx;
	view->y = view->sy;
	view->w = view->sw;
	view->h = view->sh;

	update_borders(view);
}

static void
maxvert(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	unsigned int eexz = enable_exczone;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);
	if (view->maxverted) {
		unmaxvert(view);
		return;
	}

	view->maxverted = true;

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	view->y = eexz * exczone;
	view->h = output->height - (eexz * exczone);

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
maxvert_lr(struct stage_server *server, int factor)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	unsigned int eexz = enable_exczone;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	view->w = (output->width - (3 * gapsize) - ((eexz * exczone) + (gapsize * eexz))) / 2;
	view->h = output->height - (2 * gapsize);
	view->x = (view->w + gapsize) * factor + gapsize + ((eexz * exczone) + (gapsize * eexz));
	view->y = gapsize;

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
maxvert_ub(struct stage_server *server, int factor)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	unsigned int eexz = enable_exczone;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	view->w = (output->width - (2 * gapsize) - (eexz * exczone) - (gapsize * eexz)); 
	view->h = (output->height - (3 * gapsize)) / 2;
	view->x = gapsize + (eexz * exczone) + (gapsize * eexz);
	view->y = (view->h + gapsize) * factor + gapsize;

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
move_resize(struct stage_server *server, int stepx, int stepy, int stepw, int steph)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	view->x = view->sx + stepx;
	view->y = view->sy + stepy;
	view->w = view->sw + stepw;
	view->h = view->sh + steph;

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
toggle_exczone()
{
	enable_exczone = !enable_exczone;
}

static void
unmaximize(struct stage_view *view)
{
	view->maximized = false;

	view->x = view->sx;
	view->y = view->sy;
	view->w = view->sw;
	view->h = view->sh;

	update_borders(view);
}

static void
maximize(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	unsigned int eexz = enable_exczone;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);
	if (view->maximized) {
		unmaximize(view);
		return;
	}

	view->maximized = true;

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	view->x = gapsize + ((eexz * exczone) + (gapsize * eexz));
	view->y = gapsize;
	view->w = output->width - ( 2 * gapsize ) - ((eexz * exczone) + (gapsize * eexz));
	view->h = output->height - ( 2 * gapsize );

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
fullscreen(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);
	if (view->maximized) {
		unmaximize(view);
		return;
	}

	view->maximized = true;

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	app_id = get_app_id(view);
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	view->x = 0;
	view->y = 0;
	view->w = output->width;
	view->h = output->height;

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
move_to_center(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	view->x = (output->width - view->w) / 2;
	view->y = (output->height - view->h) / 2;

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
snap_edge_lr(struct stage_server *server, int factor)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	unsigned int eexz = enable_exczone;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	view->x = gapsize + (eexz * (gapsize + exczone)) + ((output->width - view->sw - (2 * gapsize)) * factor) - ((eexz * (exczone + gapsize)) * factor);

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
snap_edge_tb(struct stage_server *server, int factor)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	view->y = gapsize + ((output->height - view->sh - (2 * gapsize)) * factor);

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
killclient(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct stage_view *view;
	struct wlr_seat *seat;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);
	
	app_id = get_app_id(view);
	if (strcmp(app_id, "yad") == 0)
		return;
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	wlr_xdg_toplevel_send_close(view->xdg_toplevel);

}

static void
send_lower(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct stage_view *view;
	struct wlr_seat *seat;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	update_borders(view);
	wlr_scene_node_lower_to_bottom(&view->scene_tree->node);
	wlr_seat_keyboard_notify_clear_focus(server->seat);

}

static void
send_upper(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct stage_view *view;
	struct wlr_seat *seat;
	const char *app_id;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	update_borders(view);

	app_id = get_app_id(view);
	if (strcmp(app_id, "imv") == 0)
		return;

	wlr_scene_node_raise_to_top(&view->scene_tree->node);

}

static void
show_geom(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct stage_view *view;
	struct wlr_seat *seat;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	printf("%s: view geoms %d %d %d %d\n", __func__, view->x, view->y,
	    view->w, view->h);

	fflush(stdout);
	}

static void
half_width_height(struct stage_server *server)
{
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct stage_output *out;
	struct stage_view *view;
	struct wlr_seat *seat;
	unsigned int eexz = enable_exczone;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);

	view->sx = view->x;
	view->sy = view->y;
	view->sw = view->w;
	view->sh = view->h;

	if (view->x < 0 || view->y < 0)
	{
		view->x = 0;
		view->y = 0;
	}

	out = output_at(server, view->x, view->y);
	output = out->wlr_output;

	if ((view->x + view->w) > output->width || (view->y + view->h) > output->height)
	{
		view->x = 0;
		view->y = 0;
	}

	view->w = (output->width - (3 * gapsize) - ((eexz * exczone) + (gapsize * eexz))) / 2;
	view->h = (output->height - (3 * gapsize)) / 2;

	update_borders(view);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

static void
shot_window(struct stage_server *server)
{
	static char ss_window[] = "ss_window";

	show_geom(server);
	if (fork() == 0)
		execl("/bin/sh", "/bin/sh", "-c", ss_window, NULL);
}

static void
shot_region(struct stage_server *server)
{
	static char ss_window[] = "ss_window";

	show_geom(server);
	send_lower(server);
	if (fork() == 0)
		execl("/bin/sh", "/bin/sh", "-c", ss_window, NULL);
}

static void
shot_screen(struct stage_server *server)
{
	static char ss_screen[] = "grim";

	show_geom(server);
	if (fork() == 0)
		execl("/bin/sh", "/bin/sh", "-c", ss_screen, NULL);
}

static bool
handle_keybinding2(struct stage_server *server,
    struct wlr_keyboard_key_event *event, xkb_keysym_t sym)
{
	switch (sym) {
	case XKB_KEY_Alt_R:
		shot_screen(server);
		break;
	case XKB_KEY_Super_R:
		break;
	default:
		return (false);
	}

	return (true);
}

static void
cycle_window_next(struct stage_server *server)
{
	struct stage_output *out;
	struct stage_workspace *ws;
//	double lx;
//	double ly;
//	out = output_at(server, lx, ly);
	out = cursor_at(server);
	
	ws = &workspaces[out->curws];

	if (wl_list_length(&ws->views) == 0) {
		return;
	}

	struct stage_view *next_view = wl_container_of(
		ws->views.prev, next_view, link);
	focus_view(next_view, next_view->xdg_toplevel->base->surface);
	send_upper(server);
}

static void
sendto_ws(struct stage_server *server, int tows)
{
	struct wlr_surface *surface;
	struct stage_view *view;
	struct wlr_seat *seat;
	const char *app_id;
	struct stage_workspace *ws;
	struct stage_output *out;
	int oldws;

	seat = server->seat;

	surface = seat->keyboard_state.focused_surface;
	if (!surface)
		return;

	view = view_from_surface(server, surface);
	
	app_id = get_app_id(view);
	if (strcmp(app_id, "yad") == 0)
		return;
	if (strcmp(app_id, "lxqt-panel") == 0)
		return;

	out = cursor_at(server);
	oldws = out->curws;

	if (oldws == tows)
		return;

	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	wl_list_remove(&view->link);

	ws = &workspaces[tows];
	wl_list_insert(&ws->views, &view->link);
}

static bool
handle_keybinding(struct stage_server *server,
    struct wlr_keyboard_key_event *event, xkb_keysym_t sym)
{
	switch (sym) {
	case XKB_KEY_0:
		changeworkspace(server, 0);
		break;
	case XKB_KEY_1:
		changeworkspace(server, 1);
		break;
	case XKB_KEY_2:
		changeworkspace(server, 2);
		break;
	case XKB_KEY_3:
		changeworkspace(server, 3);
		break;
	case XKB_KEY_4:
		changeworkspace(server, 4);
		break;
	case XKB_KEY_5:
		changeworkspace(server, 5);
		break;
	case XKB_KEY_6:
		changeworkspace(server, 6);
		break;
	case XKB_KEY_7:
		changeworkspace(server, 7);
		break;
	case XKB_KEY_8:
		changeworkspace(server, 8);
		break;
	case XKB_KEY_9:
		changeworkspace(server, 9);
		break;
	case XKB_KEY_M:
		maxvert(server);
		break;
	case XKB_KEY_h:
		maxvert_lr(server, 0);
		break;
	case XKB_KEY_l:
		maxvert_lr(server, 1);
		break;
	case XKB_KEY_U:
		maxvert_ub(server, 0);
		break;
	case XKB_KEY_D:
		maxvert_ub(server, 1);
		break;
	case XKB_KEY_f:
		maximize(server);
		break;
	case XKB_KEY_F:
		fullscreen(server);
		break;
	case XKB_KEY_Q:
		wl_display_terminate(server->wl_disp);
		break;
	case XKB_KEY_q:
		killclient(server);
		break;
	case XKB_KEY_j:
		send_lower(server);
		break;
	case XKB_KEY_k:
		send_upper(server);
		break;
	case XKB_KEY_Right:
		move_resize(server, 40, 0, 0, 0);
		break;
	case XKB_KEY_Left:
		move_resize(server, -40, 0, 0, 0);
		break;
	case XKB_KEY_Up:
		move_resize(server, 0, -40, 0, 0);
		break;
	case XKB_KEY_Down:
		move_resize(server, 0, 40, 0, 0);
		break;
	case XKB_KEY_L:
		move_resize(server, 0, 0, 40, 0);
		break;
	case XKB_KEY_H:
		move_resize(server, 0, 0, -40, 0);
		break;
	case XKB_KEY_J:
		move_resize(server, 0, 0, 0, 40);
		break;
	case XKB_KEY_K:
		move_resize(server, 0, 0, 0, -40);
		break;
	case XKB_KEY_g:
		show_geom(server);
		break;
	case XKB_KEY_R:
		snap_edge_lr(server, 1);
		break;
	case XKB_KEY_E:
		snap_edge_lr(server, 0);
		break;
	case XKB_KEY_W:
		snap_edge_tb(server, 1);
		break;
	case XKB_KEY_T:
		snap_edge_tb(server, 0);
		break;
	case XKB_KEY_C:
		move_to_center(server);
		break;
	case XKB_KEY_O:
		half_width_height(server);
		break;
	case XKB_KEY_z:
		toggle_exczone();
		break;
	case XKB_KEY_Tab:
		cycle_window_next(server);
		break;
	case XKB_KEY_minus:
	case XKB_KEY_F6:
		break;
	case XKB_KEY_Return:
		if (fork() == 0)
			execl("/bin/sh", "/bin/sh", "-c", terminal, NULL);
		break;
	case XKB_KEY_p:
		if (fork() == 0)
			execl("/bin/sh", "/bin/sh", "-c", launcher, NULL);
		break;
	case XKB_KEY_w:
		shot_window(server);
		break;
	case XKB_KEY_P:
		shot_screen(server);
		break;
	case XKB_KEY_G:
		shot_region(server);
		break;
	case XKB_KEY_exclam:
		sendto_ws(server, 1);
		break;
	case XKB_KEY_at:
		sendto_ws(server, 2);
		break;
	case XKB_KEY_numbersign:
		sendto_ws(server, 3);
		break;
	case XKB_KEY_dollar:
		sendto_ws(server, 4);
		break;
	case XKB_KEY_percent:
		sendto_ws(server, 5);
		break;
	case XKB_KEY_asciicircum:
		sendto_ws(server, 6);
		break;
	case XKB_KEY_ampersand:
		sendto_ws(server, 7);
		break;
	case XKB_KEY_asterisk:
		sendto_ws(server, 8);
		break;
	case XKB_KEY_parenleft:
		sendto_ws(server, 9);
		break;
	case XKB_KEY_parenright:
		sendto_ws(server, 0);
		break;
	default:
		return (false);
	};

	return (true);
}

static void
keyboard_handle_key(struct wl_listener *listener, void *data)
{
	struct stage_server *server;
	struct wlr_keyboard_key_event *event;
	struct stage_keyboard *keyboard;
	struct wlr_keyboard *kb;
	const xkb_keysym_t *syms;
	xkb_keysym_t sym;
	uint32_t keycode;
	uint32_t mods;
	bool handled;
	int nsyms;

	keyboard = wl_container_of(listener, keyboard, key);
	server = keyboard->server;
	event = data;

	kb = wlr_seat_get_keyboard(server->seat);
	if (!kb)
		return;

	keycode = event->keycode + 8;
	nsyms = xkb_state_key_get_syms(kb->xkb_state, keycode, &syms);

	assert(nsyms > 0);

	/* TODO: Handle first sym only. */
	sym = syms[0];

	handled = false;

	mods = wlr_keyboard_get_modifiers(kb);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		dbg_printf("%s: nsyms %d mods %x\n", __func__, nsyms, mods);
		if (mods == WLR_MODIFIER_CTRL) {
			/* Ignore key binding. */
			if (sym == XKB_KEY_Return)
				handled = true;
		} else if ((mods == STAGE_MODIFIER) || (mods == (STAGE_MODIFIER | SHFTKEY)))
			handled = handle_keybinding(server, event, sym);
		else
			handled = handle_keybinding2(server, event, sym);
	}

	if (!handled) {
		wlr_seat_set_keyboard(server->seat, kb);
		wlr_seat_keyboard_notify_key(server->seat, event->time_msec,
		    event->keycode, event->state);
	}
}

static void
keyboard_handle_destroy(struct wl_listener *listener, void *data)
{

}

static void
keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
	struct stage_keyboard *keyboard;

	keyboard = wl_container_of(listener, keyboard, modifiers);
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
	    &keyboard->wlr_keyboard->modifiers);
}

static void
server_new_keyboard(struct stage_server *server,
    struct wlr_input_device *device)
{
	struct stage_keyboard *keyboard;
	struct wlr_keyboard *kb;
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	keyboard = malloc(sizeof(struct stage_keyboard));
	keyboard->server = server;
	keyboard->device = device;
	kb = wlr_keyboard_from_input_device(device);
	keyboard->wlr_keyboard = kb;

	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_rule_names rules;
	memset(&rules, 0, sizeof(struct xkb_rule_names));
	rules.layout = "us,ru";

	keymap = xkb_keymap_new_from_names(context, &rules,
	    XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(kb, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(kb, 25, 600);

	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&kb->events.modifiers, &keyboard->modifiers);

	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&kb->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, kb);

	wl_list_insert(&server->keyboards, &keyboard->link);

	set_layout(server);
}

static void
server_new_pointer(struct stage_server *server,
    struct wlr_input_device *device)
{
  if (wlr_input_device_is_libinput(device)) {
    struct libinput_device *libinput_device =  (struct libinput_device*)
      wlr_libinput_get_device_handle(device);

    if (libinput_device_config_tap_get_finger_count(libinput_device)) {
      libinput_device_config_tap_set_enabled(libinput_device, tap_to_click);
      libinput_device_config_tap_set_drag_enabled(libinput_device, tap_and_drag);
      libinput_device_config_tap_set_drag_lock_enabled(libinput_device, drag_lock);
    }

    if (libinput_device_config_scroll_has_natural_scroll(libinput_device))
      libinput_device_config_scroll_set_natural_scroll_enabled(libinput_device, natural_scrolling);

    if (libinput_device_config_dwt_is_available(libinput_device))
      libinput_device_config_dwt_set_enabled(libinput_device, disable_while_typing);

    if (libinput_device_config_left_handed_is_available(libinput_device))
      libinput_device_config_left_handed_set(libinput_device, left_handed);

    if (libinput_device_config_middle_emulation_is_available(libinput_device))
      libinput_device_config_middle_emulation_set_enabled(libinput_device, middle_button_emulation);

    if (libinput_device_config_scroll_get_methods(libinput_device) != LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
      libinput_device_config_scroll_set_method (libinput_device, scroll_method);
    
     if (libinput_device_config_click_get_methods(libinput_device) != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
                        libinput_device_config_click_set_method (libinput_device, click_method);

    if (libinput_device_config_send_events_get_modes(libinput_device))
      libinput_device_config_send_events_set_mode(libinput_device, send_events_mode);

    if (libinput_device_config_accel_is_available(libinput_device)) {
      libinput_device_config_accel_set_profile(libinput_device, accel_profile);
      libinput_device_config_accel_set_speed(libinput_device, accel_speed);
    }
  }

	wlr_cursor_attach_input_device(server->cursor, device);
	server->device = device;
}

void server_new_virtual_pointer(struct wl_listener *listener, void*data){
	struct stage_server *server =
		wl_container_of(listener, server, new_virtual_pointer);
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;

	struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;

	struct wlr_input_device *device = &pointer->pointer.base;
	server_new_pointer(server, device);
}

static void
server_new_input(struct wl_listener *listener, void *data)
{
	struct wlr_input_device *device;
	struct stage_server *server;
	uint32_t caps;

	server = wl_container_of(listener, server, new_input);

	device = data;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		break;
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		break;
	default:
		break;
	}

	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(server->seat, caps);
}

static void
server_new_output(struct wl_listener *listener, void *data)
{
	struct wlr_output *wlr_output;
	struct wlr_output_layout_output *l_output;
	struct wlr_output_state state;
	struct wlr_output_mode *mode;
	struct wlr_scene_output *scene_output;
	struct stage_output *output;
	struct stage_server *server;

	printf("%s\n", __func__);

	server = wl_container_of(listener, server, new_output);

	wlr_output = data;
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	mode = NULL;
#if 0
	wl_list_for_each(mode, &wlr_output->modes, link) {
		printf("found mode %dx%d\n", mode->width, mode->height);
		if (mode->width == 1920 && mode->height == 1080) {
			wlr_output_set_mode(wlr_output, mode);
			wlr_output_enable(wlr_output, true);
			if (!wlr_output_commit(wlr_output))
				return;
			break;
		}
	}
#endif

	if (!wl_list_empty(&wlr_output->modes) && !mode) {
		mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output))
			return;
	}

	output = malloc(sizeof(struct stage_output));
	output->curws = 0; /* TODO */
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* set background */
	output->background = wlr_scene_rect_create(&server->scene->tree, output->wlr_output->width, output->wlr_output->height, color_background);

	wl_list_insert(&server->outputs, &output->link);

	l_output = wlr_output_layout_add_auto(server->output_layout,
	    wlr_output);
	scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, l_output,
	    scene_output);

	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
}

static void
xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct stage_view *view;

	view = wl_container_of(listener, view, unmap);

	wl_list_remove(&view->link);
}

static void
xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	struct stage_view *view;

	view = wl_container_of(listener, view, request_move);
}

static void
xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct stage_view *view;

	view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_minimize.link);

	free(view);
}

static void
xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
	struct stage_view *view;

	printf("%s\n", __func__);

	view = wl_container_of(listener, view, request_resize);
}

static void xdg_toplevel_request_maximize(
		struct wl_listener *listener, void *data) {
	struct stage_view *view =
		wl_container_of(listener, view, request_maximize);
	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);

	struct stage_server *server = view->server;
	maximize(server);
}

static void xdg_toplevel_request_minimize(
		struct wl_listener *listener, void *data) {
	struct stage_view *view =
		wl_container_of(listener, view, request_minimize);
	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);

	struct stage_server *server = view->server;
	send_lower(server);
}

static void
server_new_xdg_surface(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_surface *xdg_surface;
	struct wlr_xdg_surface *parent;
	struct wlr_xdg_toplevel *toplevel;
	struct stage_server *server;
	struct stage_view *view;
	struct wlr_output *output;
	struct stage_output *out;

	server = wl_container_of(listener, server, new_xdg_surface);

	printf("%s\n", __func__);

	xdg_surface = data;
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_scene_tree *parent_tree;
		parent = wlr_xdg_surface_try_from_wlr_surface(
		    xdg_surface->popup->parent);
		assert(parent != NULL);
		parent_tree = parent->data;
		xdg_surface->data = wlr_scene_xdg_surface_create(
		    parent_tree, xdg_surface);
		// initial simple popup unconstrain, works on maximized window only.
		out = cursor_at(server);
		output = out->wlr_output;
		wlr_xdg_popup_unconstrain_from_box(
			xdg_surface->popup,
        			&(struct wlr_box){.x = 0, .y = 0,
					.width = output->width,
					.height = output->height});
		return;
	}

	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	view = malloc(sizeof(struct stage_view));
	memset(view, 0, sizeof(struct stage_view));
	view->type = VIEW_XDG;
	view->server = server;
	view->xdg_surface = xdg_surface;
	view->xdg_toplevel = xdg_surface->toplevel;
	view->scene_tree = wlr_scene_xdg_surface_create(
	    &view->server->scene->tree, view->xdg_toplevel->base);
	view->scene_tree->node.data = view;
	xdg_surface->data = view->scene_tree;

	view->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->surface->events.map, &view->map);
	view->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->surface->events.destroy, &view->destroy);

	toplevel = xdg_surface->toplevel;
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_minimize.notify = xdg_toplevel_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize,
		&view->request_minimize);
	view->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize,
		&view->request_maximize);

	create_borders(view);
	update_borders(view);
}

static void
server_cursor_axis(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_axis_event *event;
	struct stage_server *server;

	server = wl_container_of(listener, server, cursor_axis);

	event = data;

	dbg_printf("%s\n", __func__);
	printf("%s\n", __func__);

	wlr_seat_pointer_notify_axis(server->seat,
	    event->time_msec, event->orientation, event->delta,
	    event->delta_discrete, event->source);
}

static void
server_cursor_frame(struct wl_listener *listener, void *data)
{
	struct stage_server *server;

	server = wl_container_of(listener, server, cursor_frame);
	dbg_printf("%s\n", __func__);

	wlr_seat_pointer_notify_frame(server->seat);
}

static void
process_cursor_move(struct stage_server *server, uint32_t time)
{
	struct stage_view *view;

	view = server->grabbed_view;
	view->x = server->cursor->x - server->grab_x;
	view->y = server->cursor->y - server->grab_y;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
}

static void
process_cursor_resize(struct stage_server *server, uint32_t time)
{
	uint32_t new_x, new_y;
	struct stage_view *view;

	view = server->grabbed_view;

	new_x = server->cursor->x - view->x;
	new_y = server->cursor->y - view->y;
	view->w = new_x;
	view->h = new_y;

	update_borders(view);
}

static void
process_cursor_motion(struct stage_server *server, uint32_t time)
{
	struct wlr_seat *seat;
	struct wlr_surface *surface;
	struct stage_view *view;
	double sx, sy;

	dbg_printf("%s: mode %d\n", __func__, server->cursor_mode);

	if (server->cursor_mode == STAGE_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == STAGE_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	surface = NULL;

	seat = server->seat;

	if (server->cursor_mode == STAGE_CURSOR_SCROLL) {
		sx = server->cursor->x - server->grabbed_view->x;
		sy = server->cursor->y - server->grabbed_view->y;
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		return;
	}

	view = desktop_view_at(server, server->cursor->x, server->cursor->y,
	    &surface, &sx, &sy);
	if (!view)
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");

	if (surface) {
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		focus_view(view, surface);
	} else
		wlr_seat_pointer_clear_focus(seat);
}

static void
server_cursor_motion(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_motion_event *event;
	struct stage_server *server;

	event = data;

	server = wl_container_of(listener, server, cursor_motion);
	dbg_printf("%s: dx dy %f %f\n", __func__, event->x, event->y);

	wlr_cursor_move(server->cursor, server->device, event->delta_x,
	    event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void
server_cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_motion_absolute_event *event;
	struct stage_server *server;

	event = data;

	server = wl_container_of(listener, server, cursor_motion_absolute);

	dbg_printf("%s: dx dy %f %f\n", __func__, event->x, event->y);

	wlr_cursor_warp_absolute(server->cursor, server->device, event->x,
	    event->y);
	process_cursor_motion(server, event->time_msec);
}

static void
server_cursor_button(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event;
	struct stage_server *server;
	struct wlr_keyboard *keyboard;
	uint32_t mods;

	server = wl_container_of(listener, server, cursor_button);
	keyboard = wlr_seat_get_keyboard(server->seat);
	event = data;

	double sx, sy;
	struct wlr_surface *surface;
	struct stage_view *view;

	view = desktop_view_at(server, server->cursor->x, server->cursor->y,
	    &surface, &sx, &sy);
	if (!view) {
		server->cursor_mode = STAGE_CURSOR_PASSTHROUGH;
		return;
	}

	mods = wlr_keyboard_get_modifiers(keyboard);
	if ((mods & STAGE_MODIFIER) && event->state == WLR_BUTTON_PRESSED) {
		server->grabbed_view = view;
		server->grab_x = server->cursor->x - view->x;
		server->grab_y = server->cursor->y - view->y;
		server->cur_saved_x = server->cursor->x;
		server->cur_saved_y = server->cursor->y;

		if (event->button == BTN_LEFT) {
			server->cursor_mode = STAGE_CURSOR_MOVE;
			wlr_scene_node_raise_to_top(&view->scene_tree->node);
		} else if (event->button == BTN_RIGHT) {
			server->cursor_mode = STAGE_CURSOR_RESIZE;

#if 0
			x = view->xdg_toplevel->base->current.geometry.width +
			    view->x - server->cursor->x;
			y = view->xdg_toplevel->base->current.geometry.height +
			    view->y - server->cursor->y;
			wlr_cursor_move(server->cursor, NULL, x, y);
#endif
		}

		return;
	}

	if ((event->state == WLR_BUTTON_RELEASED) &&
	    ((server->cursor_mode == STAGE_CURSOR_MOVE) ||
	     (server->cursor_mode == STAGE_CURSOR_RESIZE))) {

		server->cursor_mode = STAGE_CURSOR_PASSTHROUGH;

		if (event->button == BTN_LEFT) {
		} else if (event->button == BTN_RIGHT) {
#if 0
			x = view->xdg_toplevel->base->current.geometry.width /
			    2 + view->x - server->cursor->x;
			y = view->xdg_toplevel->base->current.geometry.height /
			    2 + view->y - server->cursor->y;
#if 0
			x = server->cur_saved_x - server->cursor->x;
			y = server->cur_saved_y - server->cursor->y;
#endif
			wlr_cursor_move(server->cursor, NULL, x, y);
#endif
		}

		return;
	}

	if (mods == 0) {
		if (event->button == BTN_LEFT) {
			if (event->state == WLR_BUTTON_PRESSED) {
				server->grabbed_view = view;
				server->grab_x = server->cursor->x - view->x;
				server->grab_y = server->cursor->y - view->y;
				server->cursor_mode = STAGE_CURSOR_SCROLL;
			} else
				server->cursor_mode = STAGE_CURSOR_PASSTHROUGH;
		}
	}

	// example if you want to use alt + left button to resize
	
	if (mods == WLR_MODIFIER_ALT) {
		if (event->button == BTN_LEFT) {
			if (event->state == WLR_BUTTON_PRESSED) {
				server->grabbed_view = view;
				server->grab_x = server->cursor->x - view->x;
				server->grab_y = server->cursor->y - view->y;
				server->cursor_mode = STAGE_CURSOR_RESIZE;
			}				
		}
	}

	wlr_seat_pointer_notify_button(server->seat, event->time_msec,
	    event->button, event->state);
}

static void
seat_request_cursor(struct wl_listener *listener, void *data)
{
	struct stage_server *server;

	printf("%s\n", __func__);

	server = wl_container_of(listener, server, request_cursor);

	wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
}

static void
seat_request_set_selection(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_set_selection_event *event;
	struct stage_server *server;

	printf("%s\n", __func__);

	server = wl_container_of(listener, server, request_set_selection);
	event = data;

	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void
seat_request_set_primary_selection(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_set_primary_selection_event *event;
	struct stage_server *server;

	printf("%s\n", __func__);

	server = wl_container_of(listener, server,
	    request_set_primary_selection);

	event = data;

	wlr_seat_set_primary_selection(server->seat, event->source,
	    event->serial);
}

int
main(int argc, char *argv[])
{
	struct stage_server server;
	const char *socket;
	int error;
	int i;

	wlr_log_init(WLR_DEBUG, NULL);

	memset(&server, 0, sizeof(struct stage_server));

	server.wl_disp = wl_display_create();
	server.backend = wlr_backend_autocreate(server.wl_disp,  0);
	server.renderer = wlr_renderer_autocreate(server.backend);
	wlr_renderer_init_wl_display(server.renderer, server.wl_disp);

	server.allocator = wlr_allocator_autocreate(server.backend,
	    server.renderer);
	server.compositor = wlr_compositor_create(server.wl_disp,
	    5, server.renderer);

	wlr_export_dmabuf_manager_v1_create(server.wl_disp);
	wlr_screencopy_manager_v1_create(server.wl_disp);
	wlr_data_control_manager_v1_create(server.wl_disp);
	wlr_data_device_manager_create(server.wl_disp);
	wlr_gamma_control_manager_v1_create(server.wl_disp);
	wlr_primary_selection_v1_device_manager_create(server.wl_disp);
	wlr_viewporter_create(server.wl_disp);
	wlr_subcompositor_create(server.wl_disp);

	server.activation = wlr_xdg_activation_v1_create(server.wl_disp);
	server.request_activate.notify = request_activate;
	wl_signal_add(&server.activation->events.request_activate,
	    &server.request_activate);

	server.output_layout = wlr_output_layout_create();
	server.layout_change.notify = layout_change;
	wl_signal_add(&server.output_layout->events.change,
	    &server.layout_change);
	wlr_xdg_output_manager_v1_create(server.wl_disp, server.output_layout);

	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene,
	    server.output_layout);
	wlr_scene_attach_output_layout(server.scene, server.output_layout);

	server.xdg_shell = wlr_xdg_shell_create(server.wl_disp, 3);
	server.new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server.xdg_shell->events.new_surface,
	    &server.new_xdg_surface);

	server.virtual_pointer_manager = wlr_virtual_pointer_manager_v1_create(server.wl_disp);
	server.new_virtual_pointer.notify = server_new_virtual_pointer;
	wl_signal_add(&server.virtual_pointer_manager->events.new_virtual_pointer,
	&server.new_virtual_pointer);

	server.output_manager = wlr_output_manager_v1_create(server.wl_disp);
	server.output_manager_apply.notify = output_manager_apply;
	wl_signal_add(&server.output_manager->events.apply,
	    &server.output_manager_apply);
	server.output_manager_test.notify = output_manager_test;
	wl_signal_add(&server.output_manager->events.test,
	    &server.output_manager_test);

	server.presentation = wlr_presentation_create(server.wl_disp,
	    server.backend);
	wlr_scene_set_presentation(server.scene, server.presentation);

	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server.cursor_mgr, 1);

	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute,
	    &server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	for (i = 0; i < N_WORKSPACES; i++)
		wl_list_init(&workspaces[i].views);

	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.seat = wlr_seat_create(server.wl_disp, "seat0");
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
	    &server.request_cursor);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
	    &server.request_set_selection);

	server.request_set_primary_selection.notify =
	    seat_request_set_primary_selection;
	wl_signal_add(&server.seat->events.request_set_primary_selection,
	    &server.request_set_primary_selection);

//	wlr_server_decoration_manager_set_default_mode(
//	    wlr_server_decoration_manager_create(server.wl_disp),
//	    WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
//	wlr_xdg_decoration_manager_v1_create(server.wl_disp);

	socket = wl_display_add_socket_auto(server.wl_disp);
	if (socket == NULL) {
		wlr_backend_destroy(server.backend);
		return (1);
	}

	error = wlr_backend_start(server.backend);
	if (error == 0) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_disp);
		return (2);
	}

	setenv("WAYLAND_DISPLAY", socket, true);
	if (fork() == 0)
		execl("/bin/sh", "/bin/sh", "-c", terminal, NULL);
	wl_display_run(server.wl_disp);

	wl_display_destroy_clients(server.wl_disp);
	wl_display_destroy(server.wl_disp);

	return (0);
}
