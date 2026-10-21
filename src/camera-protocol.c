/*
 * scrcpy camera controls protocol (fork extension of the scrcpy server)
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

#include "camera-protocol.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CAMERA_CONTROLS_PAYLOAD_SIZE 33
#define CAMERA_STATE_V1_PAYLOAD_SIZE 46

static const char *const AWB_MODE_PARAMS[] = {
	"off", "auto", "incandescent", "fluorescent", "warm-fluorescent", "daylight", "cloudy", "twilight", "shade",
};

static const char *const AF_MODE_PARAMS[] = {
	"off", "auto", "macro", "continuous-video", "continuous-picture", "edof",
};

static void write_be16(uint8_t *buf, uint16_t value)
{
	buf[0] = (uint8_t)(value >> 8);
	buf[1] = (uint8_t)value;
}

static void write_be32(uint8_t *buf, uint32_t value)
{
	buf[0] = (uint8_t)(value >> 24);
	buf[1] = (uint8_t)(value >> 16);
	buf[2] = (uint8_t)(value >> 8);
	buf[3] = (uint8_t)value;
}

static void write_be64(uint8_t *buf, uint64_t value)
{
	write_be32(buf, (uint32_t)(value >> 32));
	write_be32(buf + 4, (uint32_t)value);
}

static void write_float(uint8_t *buf, float value)
{
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	write_be32(buf, bits);
}

static uint16_t read_be16(const uint8_t *buf)
{
	return (uint16_t)((buf[0] << 8) | buf[1]);
}

static uint32_t read_be32(const uint8_t *buf)
{
	return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

static uint64_t read_be64(const uint8_t *buf)
{
	return ((uint64_t)read_be32(buf) << 32) | read_be32(buf + 4);
}

static float read_float(const uint8_t *buf)
{
	uint32_t bits = read_be32(buf);
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

/* A 3A state or mode byte: 0xFF means unknown (-1) */
static int read_state_byte(uint8_t value)
{
	return value == 0xFF ? -1 : value;
}

size_t scrcpy_camera_controls_serialize(const struct scrcpy_camera_controls *c, uint8_t *buf)
{
	buf[0] = SCRCPY_CONTROL_MSG_CAMERA_SET_CONTROLS;
	write_be16(&buf[1], CAMERA_CONTROLS_PAYLOAD_SIZE);

	uint8_t *p = &buf[3];
	p[0] = 1; /* version */
	write_be32(&p[1], c->present_mask & SCRCPY_CAMERA_FIELD_ALL);
	p[5] = c->ae_mode;
	write_be32(&p[6], (uint32_t)c->iso);
	write_be64(&p[10], (uint64_t)c->exposure_time_ns);
	p[18] = (uint8_t)c->ev_index;
	p[19] = c->ae_lock ? 1 : 0;
	p[20] = c->awb_mode;
	p[21] = c->awb_lock ? 1 : 0;
	p[22] = c->af_mode;
	write_float(&p[23], c->focus_distance);
	write_float(&p[27], c->zoom);
	p[31] = c->torch ? 1 : 0;
	p[32] = 0; /* reserved */

	return SCRCPY_CAMERA_CONTROLS_MSG_SIZE;
}

size_t scrcpy_camera_af_trigger_serialize(uint8_t action, uint8_t *buf)
{
	buf[0] = SCRCPY_CONTROL_MSG_CAMERA_AF_TRIGGER;
	write_be16(&buf[1], 1);
	buf[3] = action;
	return SCRCPY_CAMERA_AF_TRIGGER_MSG_SIZE;
}

/* Bounds-checked sequential reader over a payload */
struct reader {
	const uint8_t *data;
	size_t len;
	size_t pos;
	bool error;
};

static const uint8_t *reader_take(struct reader *r, size_t n)
{
	if (r->error || r->len - r->pos < n) {
		r->error = true;
		return NULL;
	}
	const uint8_t *p = &r->data[r->pos];
	r->pos += n;
	return p;
}

static uint8_t reader_u8(struct reader *r)
{
	const uint8_t *p = reader_take(r, 1);
	return p ? p[0] : 0;
}

static uint32_t reader_u32(struct reader *r)
{
	const uint8_t *p = reader_take(r, 4);
	return p ? read_be32(p) : 0;
}

static uint64_t reader_u64(struct reader *r)
{
	const uint8_t *p = reader_take(r, 8);
	return p ? read_be64(p) : 0;
}

static float reader_float(struct reader *r)
{
	const uint8_t *p = reader_take(r, 4);
	return p ? read_float(p) : 0.0f;
}

static void reader_modes(struct reader *r, uint8_t *count, uint8_t *modes)
{
	uint8_t n = reader_u8(r);
	const uint8_t *p = reader_take(r, n);
	if (!p) {
		*count = 0;
		return;
	}
	*count = n < SCRCPY_CAMERA_MAX_MODES ? n : SCRCPY_CAMERA_MAX_MODES;
	memcpy(modes, p, *count);
}

static bool parse_camera_caps(const uint8_t *payload, size_t len, struct scrcpy_camera_caps *caps)
{
	struct reader r = {payload, len, 0, false};
	memset(caps, 0, sizeof(*caps));

	uint8_t version = reader_u8(&r);
	if (version < 1)
		return false;

	uint8_t id_len = reader_u8(&r);
	const uint8_t *id = reader_take(&r, id_len);
	if (id) {
		memcpy(caps->camera_id, id, id_len); /* id_len <= 255 < sizeof(camera_id) */
		caps->camera_id[id_len] = '\0';
	}
	caps->facing = reader_u8(&r);
	caps->hardware_level = reader_u8(&r);
	caps->flags = reader_u32(&r);
	caps->iso_min = (int32_t)reader_u32(&r);
	caps->iso_max = (int32_t)reader_u32(&r);
	caps->exposure_min_ns = (int64_t)reader_u64(&r);
	caps->exposure_max_ns = (int64_t)reader_u64(&r);
	caps->frame_duration_ns = (int64_t)reader_u64(&r);
	caps->ev_min = (int8_t)reader_u8(&r);
	caps->ev_max = (int8_t)reader_u8(&r);
	caps->ev_step_num = (int32_t)reader_u32(&r);
	caps->ev_step_den = (int32_t)reader_u32(&r);
	reader_modes(&r, &caps->awb_mode_count, caps->awb_modes);
	reader_modes(&r, &caps->af_mode_count, caps->af_modes);
	caps->min_focus_distance = reader_float(&r);
	caps->hyperfocal_distance = reader_float(&r);
	caps->zoom_min = reader_float(&r);
	caps->zoom_max = reader_float(&r);
	/* Fields appended by later versions are ignored */

	return !r.error;
}

static bool parse_camera_state(const uint8_t *payload, size_t len, struct scrcpy_camera_state *state)
{
	if (len < CAMERA_STATE_V1_PAYLOAD_SIZE || payload[0] < 1)
		return false;

	struct reader r = {payload, len, 1, false};
	memset(state, 0, sizeof(*state));
	state->applied_mask = reader_u32(&r);
	state->rejected_mask = reader_u32(&r);
	state->ae_mode = reader_u8(&r);
	state->ae_state = read_state_byte(reader_u8(&r));
	state->ae_lock = reader_u8(&r) != 0;
	state->awb_mode = read_state_byte(reader_u8(&r));
	state->awb_state = read_state_byte(reader_u8(&r));
	state->awb_lock = reader_u8(&r) != 0;
	state->af_mode = read_state_byte(reader_u8(&r));
	state->af_state = read_state_byte(reader_u8(&r));
	state->iso = (int32_t)reader_u32(&r);
	state->exposure_time_ns = (int64_t)reader_u64(&r);
	state->frame_duration_ns = (int64_t)reader_u64(&r);
	state->ev_index = (int8_t)reader_u8(&r);
	state->focus_distance = reader_float(&r);
	state->zoom = reader_float(&r);
	return !r.error;
}

long scrcpy_device_msg_parse(const uint8_t *buf, size_t len, struct scrcpy_device_msg *msg)
{
	if (len < 1)
		return 0;

	uint8_t type = buf[0];
	msg->type = type;
	msg->kind = SCRCPY_DEVICE_MSG_KIND_IGNORED;

	switch (type) {
	case SCRCPY_DEVICE_MSG_CAMERA_CAPABILITIES:
	case SCRCPY_DEVICE_MSG_CAMERA_STATE: {
		if (len < 3)
			return 0;
		size_t payload_len = read_be16(&buf[1]);
		if (len < 3 + payload_len)
			return 0;
		const uint8_t *payload = &buf[3];
		bool ok;
		if (type == SCRCPY_DEVICE_MSG_CAMERA_CAPABILITIES) {
			msg->kind = SCRCPY_DEVICE_MSG_KIND_CAMERA_CAPABILITIES;
			ok = parse_camera_caps(payload, payload_len, &msg->caps);
		} else {
			msg->kind = SCRCPY_DEVICE_MSG_KIND_CAMERA_STATE;
			ok = parse_camera_state(payload, payload_len, &msg->state);
		}
		return ok ? (long)(3 + payload_len) : -1;
	}
	case SCRCPY_DEVICE_MSG_CLIPBOARD: {
		if (len < 5)
			return 0;
		size_t text_len = read_be32(&buf[1]);
		if (text_len > (1 << 18))
			return -1;
		return len < 5 + text_len ? 0 : (long)(5 + text_len);
	}
	case SCRCPY_DEVICE_MSG_ACK_CLIPBOARD:
		return len < 9 ? 0 : 9;
	case SCRCPY_DEVICE_MSG_UHID_OUTPUT: {
		if (len < 5)
			return 0;
		size_t data_len = read_be16(&buf[3]);
		return len < 5 + data_len ? 0 : (long)(5 + data_len);
	}
	default:
		return -1;
	}
}

bool scrcpy_camera_controls_equal(const struct scrcpy_camera_controls *a, const struct scrcpy_camera_controls *b)
{
	uint32_t mask = a->present_mask & SCRCPY_CAMERA_FIELD_ALL;
	if (mask != (b->present_mask & SCRCPY_CAMERA_FIELD_ALL))
		return false;

	return (!(mask & SCRCPY_CAMERA_FIELD_AE_MODE) || a->ae_mode == b->ae_mode) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_ISO) || a->iso == b->iso) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_EXPOSURE_TIME) || a->exposure_time_ns == b->exposure_time_ns) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_EV) || a->ev_index == b->ev_index) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_AE_LOCK) || a->ae_lock == b->ae_lock) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_AWB_MODE) || a->awb_mode == b->awb_mode) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_AWB_LOCK) || a->awb_lock == b->awb_lock) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_AF_MODE) || a->af_mode == b->af_mode) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_FOCUS_DISTANCE) || a->focus_distance == b->focus_distance) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_ZOOM) || a->zoom == b->zoom) &&
	       (!(mask & SCRCPY_CAMERA_FIELD_TORCH) || a->torch == b->torch);
}

const char *scrcpy_camera_awb_mode_param(uint8_t mode)
{
	return mode < sizeof(AWB_MODE_PARAMS) / sizeof(AWB_MODE_PARAMS[0]) ? AWB_MODE_PARAMS[mode] : NULL;
}

const char *scrcpy_camera_af_mode_param(uint8_t mode)
{
	return mode < sizeof(AF_MODE_PARAMS) / sizeof(AF_MODE_PARAMS[0]) ? AF_MODE_PARAMS[mode] : NULL;
}

/* Format a non-negative value with 3 decimals, always with a '.' separator (snprintf("%f") depends on the locale, and
 * the server parses with Float.parseFloat()) */
static void format_fixed3(char *dst, size_t dst_size, float value)
{
	long long milli = (long long)(value * 1000.0f + 0.5f);
	snprintf(dst, dst_size, "%lld.%03lld", milli / 1000, milli % 1000);
}

static bool append(char *dst, size_t dst_size, size_t *len, const char *fmt_result)
{
	size_t n = strlen(fmt_result);
	if (*len + n + 1 > dst_size)
		return false;
	memcpy(dst + *len, fmt_result, n + 1);
	*len += n;
	return true;
}

bool scrcpy_camera_controls_append_params(const struct scrcpy_camera_controls *c, char *dst, size_t dst_size)
{
	size_t len = strlen(dst);
	char tmp[64];
	bool ok = true;
	uint32_t mask = c->present_mask;

	if (mask & SCRCPY_CAMERA_FIELD_AE_MODE) {
		ok &= append(dst, dst_size, &len,
			     c->ae_mode == SCRCPY_CAMERA_AE_MODE_MANUAL ? " camera_ae=manual" : " camera_ae=auto");
	}
	if ((mask & SCRCPY_CAMERA_FIELD_ISO) && c->iso > 0) {
		snprintf(tmp, sizeof(tmp), " camera_iso=%" PRId32, c->iso);
		ok &= append(dst, dst_size, &len, tmp);
	}
	if ((mask & SCRCPY_CAMERA_FIELD_EXPOSURE_TIME) && c->exposure_time_ns > 0) {
		snprintf(tmp, sizeof(tmp), " camera_exposure_time=%" PRId64, c->exposure_time_ns);
		ok &= append(dst, dst_size, &len, tmp);
	}
	if (mask & SCRCPY_CAMERA_FIELD_EV) {
		snprintf(tmp, sizeof(tmp), " camera_ev=%d", (int)c->ev_index);
		ok &= append(dst, dst_size, &len, tmp);
	}
	if (mask & SCRCPY_CAMERA_FIELD_AE_LOCK)
		ok &= append(dst, dst_size, &len, c->ae_lock ? " camera_ae_lock=true" : " camera_ae_lock=false");
	if (mask & SCRCPY_CAMERA_FIELD_AWB_MODE) {
		const char *name = scrcpy_camera_awb_mode_param(c->awb_mode);
		if (name) {
			snprintf(tmp, sizeof(tmp), " camera_awb=%s", name);
			ok &= append(dst, dst_size, &len, tmp);
		}
	}
	if (mask & SCRCPY_CAMERA_FIELD_AWB_LOCK)
		ok &= append(dst, dst_size, &len, c->awb_lock ? " camera_awb_lock=true" : " camera_awb_lock=false");
	if (mask & SCRCPY_CAMERA_FIELD_AF_MODE) {
		const char *name = scrcpy_camera_af_mode_param(c->af_mode);
		if (name) {
			snprintf(tmp, sizeof(tmp), " camera_af=%s", name);
			ok &= append(dst, dst_size, &len, tmp);
		}
	}
	if ((mask & SCRCPY_CAMERA_FIELD_FOCUS_DISTANCE) && c->focus_distance >= 0) {
		char value[32];
		format_fixed3(value, sizeof(value), c->focus_distance);
		snprintf(tmp, sizeof(tmp), " camera_focus_distance=%s", value);
		ok &= append(dst, dst_size, &len, tmp);
	}
	if ((mask & SCRCPY_CAMERA_FIELD_ZOOM) && c->zoom > 0) {
		char value[32];
		format_fixed3(value, sizeof(value), c->zoom);
		snprintf(tmp, sizeof(tmp), " camera_zoom=%s", value);
		ok &= append(dst, dst_size, &len, tmp);
	}
	if (mask & SCRCPY_CAMERA_FIELD_TORCH)
		ok &= append(dst, dst_size, &len, c->torch ? " camera_torch=true" : " camera_torch=false");

	return ok;
}
