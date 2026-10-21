/*
 * scrcpy session bootstrap
 * Copyright (C) 2026 NanKill <nankill@nankill.xyz>
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

#include <stdbool.h>
#include <stdint.h>

#include "camera-protocol.h"

struct scrcpy_session;
struct obs_source_frame;
struct obs_source_audio;

typedef void (*scrcpy_session_frame_callback)(void *opaque, const struct obs_source_frame *frame);
typedef void (*scrcpy_session_audio_callback)(void *opaque, const struct obs_source_audio *audio);
/* Called from the session control thread */
typedef void (*scrcpy_session_camera_caps_callback)(void *opaque, const struct scrcpy_camera_caps *caps);
typedef void (*scrcpy_session_camera_state_callback)(void *opaque, const struct scrcpy_camera_state *state);

struct scrcpy_session_config {
	const char *adb_path;
	const char *device_serial;
	const char *server_jar_path;
	const char *scrcpy_version;
	const char *video_codec;
	const char *video_source;
	const char *camera_id;
	const char *camera_size;
	uint16_t local_port;
	uint32_t video_bit_rate;
	uint16_t max_size;
	uint16_t max_fps;       /* 0 = unset; screen only (encoder cap) */
	uint16_t camera_fps;    /* 0 = device default; camera only */
	bool camera_high_speed; /* camera only; requires camera_fps > 0 */
	bool hw_decoding;
	bool audio_enabled;
	const char *audio_source;
	const char *audio_codec;
	uint32_t audio_bit_rate;
	/* Low-latency level: 0=Off, 1=Low (default), 2=Medium.
	 * Higher levels trade jitter tolerance for lower delay by tuning the
	 * socket receive buffer, decoder flags, and OBS unbuffered async output. */
	uint8_t low_latency_level;
	/* Live camera controls (camera source only). Requires the forked server (SCRCPY_CAMCTL_SERVER_VERSION): opens
	 * the control socket, passes camera_controls as startup parameters, and reports capabilities and state. */
	bool camera_controls_enabled;
	struct scrcpy_camera_controls camera_controls;
	scrcpy_session_frame_callback on_frame;
	void *on_frame_opaque;
	scrcpy_session_audio_callback on_audio;
	void *on_audio_opaque;
	scrcpy_session_camera_caps_callback on_camera_caps;
	scrcpy_session_camera_state_callback on_camera_state;
	void *on_camera_opaque;
};

struct scrcpy_session *scrcpy_session_create(void);
void scrcpy_session_destroy(struct scrcpy_session *session);

int scrcpy_session_start(struct scrcpy_session *session, const struct scrcpy_session_config *config);
void scrcpy_session_stop(struct scrcpy_session *session);
bool scrcpy_session_is_running(const struct scrcpy_session *session);

/* Replace the desired camera controls (thread-safe). They are sent to the device if connected (bursts are coalesced),
 * and used as startup parameters on the next (re)connection. */
void scrcpy_session_set_camera_controls(struct scrcpy_session *session, const struct scrcpy_camera_controls *controls);
/* Trigger (SCRCPY_CAMERA_AF_TRIGGER_START) or cancel a one-shot autofocus (thread-safe, ignored if not connected) */
void scrcpy_session_camera_af_trigger(struct scrcpy_session *session, uint8_t action);
