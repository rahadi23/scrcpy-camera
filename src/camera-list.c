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

#include "camera-list.h"

#include <stdlib.h>
#include <string.h>

static void copy_trimmed(char *dst, size_t dst_size, const char *begin, const char *end)
{
	while (begin < end && (*begin == ' ' || *begin == '\t'))
		++begin;
	while (end > begin && (end[-1] == ' ' || end[-1] == '\t'))
		--end;
	size_t len = (size_t)(end - begin);
	if (len >= dst_size)
		len = dst_size - 1;
	memcpy(dst, begin, len);
	dst[len] = '\0';
}

/* Parse "WxH" at p. Return the pointer after the size, or NULL. */
static const char *parse_size(const char *p, const char *end, struct camera_list_size *size)
{
	char *after;
	long w = strtol(p, &after, 10);
	if (after == p || after >= end || *after != 'x' || w <= 0 || w > 65535)
		return NULL;
	p = after + 1;
	long h = strtol(p, &after, 10);
	if (after == p || after > end || h <= 0 || h > 65535)
		return NULL;
	size->width = (uint16_t)w;
	size->height = (uint16_t)h;
	return after;
}

/* Parse "fps={120, 240}" in [p, end) */
static void parse_fps(const char *p, const char *end, struct camera_list_size *size)
{
	const char *brace = memchr(p, '{', (size_t)(end - p));
	if (!brace)
		return;
	p = brace + 1;
	while (p < end && *p != '}' && size->fps_count < CAMERA_LIST_MAX_FPS) {
		char *after;
		long fps = strtol(p, &after, 10);
		if (after == p)
			break;
		if (fps > 0 && fps < 65536)
			size->fps[size->fps_count++] = (uint16_t)fps;
		p = after;
		while (p < end && (*p == ',' || *p == ' '))
			++p;
	}
}

static void parse_camera_line(const char *line, const char *end, struct camera_list_entry *camera)
{
	const char *id = strstr(line, "--camera-id=");
	id += strlen("--camera-id=");
	const char *id_end = id;
	while (id_end < end && *id_end != ' ' && *id_end != '\t')
		++id_end;
	copy_trimmed(camera->id, sizeof(camera->id), id, id_end);

	const char *open = memchr(id_end, '(', (size_t)(end - id_end));
	if (!open)
		return;
	const char *close = end;
	while (close > open && close[-1] != ')')
		--close;
	if (close <= open + 1)
		return;
	copy_trimmed(camera->description, sizeof(camera->description), open + 1, close - 1);

	const char *comma = memchr(open + 1, ',', (size_t)(close - 1 - (open + 1)));
	copy_trimmed(camera->facing, sizeof(camera->facing), open + 1, comma ? comma : close - 1);
}

bool camera_list_parse(const char *text, struct camera_list *list)
{
	memset(list, 0, sizeof(*list));
	if (!text)
		return false;

	const char *p = strstr(text, "List of cameras:");
	if (!p)
		return false;

	struct camera_list_entry *camera = NULL;
	bool high_speed = false;

	while (*p) {
		const char *end = p;
		while (*end && *end != '\n' && *end != '\r')
			++end;

		const char *s = p;
		while (s < end && (*s == ' ' || *s == '\t'))
			++s;

		if (strstr(p, "--camera-id=") && strstr(p, "--camera-id=") < end) {
			camera = list->count < CAMERA_LIST_MAX_CAMERAS ? &list->cameras[list->count++] : NULL;
			high_speed = false;
			if (camera)
				parse_camera_line(p, end, camera);
		} else if (camera && s < end && strncmp(s, "High speed capture", strlen("High speed capture")) == 0) {
			high_speed = true;
		} else if (camera && end - s > 2 && s[0] == '-' && s[1] == ' ') {
			struct camera_list_size size = {0};
			const char *after = parse_size(s + 2, end, &size);
			if (after) {
				if (high_speed) {
					parse_fps(after, end, &size);
					if (camera->high_speed_count < CAMERA_LIST_MAX_HIGH_SPEED_SIZES)
						camera->high_speed_sizes[camera->high_speed_count++] = size;
				} else if (camera->size_count < CAMERA_LIST_MAX_SIZES) {
					camera->sizes[camera->size_count++] = size;
				}
			}
		}
		/* Other lines (other log messages) are ignored */

		p = end;
		while (*p == '\n' || *p == '\r')
			++p;
	}

	return true;
}

const struct camera_list_entry *camera_list_find(const struct camera_list *list, const char *id)
{
	if (!id)
		return NULL;
	for (size_t i = 0; i < list->count; ++i) {
		if (strcmp(list->cameras[i].id, id) == 0)
			return &list->cameras[i];
	}
	return NULL;
}

bool camera_list_size_has_fps(const struct camera_list_size *size, int fps)
{
	for (uint8_t i = 0; i < size->fps_count; ++i) {
		if (size->fps[i] == fps)
			return true;
	}
	return false;
}
