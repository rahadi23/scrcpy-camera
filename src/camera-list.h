/*
 * Parser of the scrcpy server camera list ("list_camera_sizes=true")
 * Copyright (C) 2026 rahadi23
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>
 */

#pragma once

/*
 * Pure parsing code, without OBS dependencies (see tests/test-camera-list.c).
 *
 * Input: the server output of "list_camera_sizes=true", for example:
 *
 *     [server] INFO: List of cameras:
 *         --camera-id=0    (back, 4032x3024, fps={15, 20, 24, 30}, zoom-range=[1, 8])
 *             - 4032x3024
 *             - 1920x1080
 *           High speed capture (--camera-high-speed):
 *             - 1280x720 (fps={120, 240})
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAMERA_LIST_MAX_CAMERAS 16
#define CAMERA_LIST_MAX_SIZES 64
#define CAMERA_LIST_MAX_HIGH_SPEED_SIZES 16
#define CAMERA_LIST_MAX_FPS 8

struct camera_list_size {
	uint16_t width;
	uint16_t height;
	uint8_t fps_count; /* high-speed sizes only */
	uint16_t fps[CAMERA_LIST_MAX_FPS];
};

struct camera_list_entry {
	char id[32];
	char facing[16];      /* "back", "front", "external" */
	char description[96]; /* the text between the parentheses, e.g. "back, 4032x3024, fps={15, 20, 24, 30}, ..." */
	size_t size_count;
	struct camera_list_size sizes[CAMERA_LIST_MAX_SIZES];
	size_t high_speed_count;
	struct camera_list_size high_speed_sizes[CAMERA_LIST_MAX_HIGH_SPEED_SIZES];
};

struct camera_list {
	size_t count;
	struct camera_list_entry cameras[CAMERA_LIST_MAX_CAMERAS];
};

/* Parse the server output. Return false if it does not contain a camera list. */
bool camera_list_parse(const char *text, struct camera_list *list);

const struct camera_list_entry *camera_list_find(const struct camera_list *list, const char *id);

/* Whether the high-speed size supports the given frame rate */
bool camera_list_size_has_fps(const struct camera_list_size *size, int fps);
