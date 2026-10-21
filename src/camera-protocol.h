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

#pragma once

/*
 * Pure serialization code, without OBS or FFmpeg dependencies, so that it
 * can be unit-tested on its own (see tests/test-camera-protocol.c).
 *
 * The wire format is defined by the forked scrcpy server (rahadi23/scrcpy,
 * server version "5.0.1-camctl.1"). All values are big-endian. Fork messages
 * use types 0xC0 and above, and their payload is prefixed by a 16-bit length,
 * so that fields may be appended in later versions.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCRCPY_CAMCTL_SERVER_VERSION "5.0.1-camctl.1"

/* Control messages (client -> server) */
#define SCRCPY_CONTROL_MSG_CAMERA_SET_CONTROLS 0xC0
#define SCRCPY_CONTROL_MSG_CAMERA_AF_TRIGGER 0xC1

/* Device messages (server -> client) */
#define SCRCPY_DEVICE_MSG_CLIPBOARD 0x00
#define SCRCPY_DEVICE_MSG_ACK_CLIPBOARD 0x01
#define SCRCPY_DEVICE_MSG_UHID_OUTPUT 0x02
#define SCRCPY_DEVICE_MSG_CAMERA_CAPABILITIES 0xC0
#define SCRCPY_DEVICE_MSG_CAMERA_STATE 0xC1

/* Bits of scrcpy_camera_controls.present_mask (and of the applied/rejected masks of the state) */
#define SCRCPY_CAMERA_FIELD_AE_MODE (1u << 0)
#define SCRCPY_CAMERA_FIELD_ISO (1u << 1)
#define SCRCPY_CAMERA_FIELD_EXPOSURE_TIME (1u << 2)
#define SCRCPY_CAMERA_FIELD_EV (1u << 3)
#define SCRCPY_CAMERA_FIELD_AE_LOCK (1u << 4)
#define SCRCPY_CAMERA_FIELD_AWB_MODE (1u << 5)
#define SCRCPY_CAMERA_FIELD_AWB_LOCK (1u << 6)
#define SCRCPY_CAMERA_FIELD_AF_MODE (1u << 7)
#define SCRCPY_CAMERA_FIELD_FOCUS_DISTANCE (1u << 8)
#define SCRCPY_CAMERA_FIELD_ZOOM (1u << 9)
#define SCRCPY_CAMERA_FIELD_TORCH (1u << 10)
#define SCRCPY_CAMERA_FIELD_ALL ((1u << 11) - 1)

#define SCRCPY_CAMERA_AE_MODE_AUTO 0
#define SCRCPY_CAMERA_AE_MODE_MANUAL 1

/* Same values as Android CameraMetadata.CONTROL_AWB_MODE_* */
#define SCRCPY_CAMERA_AWB_MODE_OFF 0
#define SCRCPY_CAMERA_AWB_MODE_AUTO 1
#define SCRCPY_CAMERA_AWB_MODE_INCANDESCENT 2
#define SCRCPY_CAMERA_AWB_MODE_FLUORESCENT 3
#define SCRCPY_CAMERA_AWB_MODE_WARM_FLUORESCENT 4
#define SCRCPY_CAMERA_AWB_MODE_DAYLIGHT 5
#define SCRCPY_CAMERA_AWB_MODE_CLOUDY_DAYLIGHT 6
#define SCRCPY_CAMERA_AWB_MODE_TWILIGHT 7
#define SCRCPY_CAMERA_AWB_MODE_SHADE 8

/* Same values as Android CameraMetadata.CONTROL_AF_MODE_* */
#define SCRCPY_CAMERA_AF_MODE_OFF 0
#define SCRCPY_CAMERA_AF_MODE_AUTO 1
#define SCRCPY_CAMERA_AF_MODE_MACRO 2
#define SCRCPY_CAMERA_AF_MODE_CONTINUOUS_VIDEO 3
#define SCRCPY_CAMERA_AF_MODE_CONTINUOUS_PICTURE 4
#define SCRCPY_CAMERA_AF_MODE_EDOF 5

#define SCRCPY_CAMERA_AF_TRIGGER_START 1
#define SCRCPY_CAMERA_AF_TRIGGER_CANCEL 2

/* Same values as Android CaptureResult.CONTROL_AE_STATE_* / CONTROL_AWB_STATE_* (-1 if unknown) */
#define SCRCPY_CAMERA_3A_STATE_INACTIVE 0
#define SCRCPY_CAMERA_3A_STATE_SEARCHING 1
#define SCRCPY_CAMERA_3A_STATE_CONVERGED 2
#define SCRCPY_CAMERA_3A_STATE_LOCKED 3

/* Bits of scrcpy_camera_caps.flags */
#define SCRCPY_CAMERA_CAP_MANUAL_SENSOR (1u << 0)
#define SCRCPY_CAMERA_CAP_FOCUS_CALIBRATED (1u << 1)
#define SCRCPY_CAMERA_CAP_AF_OFF_SUPPORTED (1u << 2)
#define SCRCPY_CAMERA_CAP_AE_LOCK (1u << 3)
#define SCRCPY_CAMERA_CAP_AWB_LOCK (1u << 4)
#define SCRCPY_CAMERA_CAP_HAS_FLASH (1u << 5)
#define SCRCPY_CAMERA_CAP_HIGH_SPEED_SESSION (1u << 6)

#define SCRCPY_CAMERA_CONTROLS_MSG_SIZE (1 + 2 + 33)
#define SCRCPY_CAMERA_AF_TRIGGER_MSG_SIZE (1 + 2 + 1)
#define SCRCPY_CAMERA_MAX_MODES 32

/* Desired camera settings: a full snapshot, a field not present in the mask is left at the device default */
struct scrcpy_camera_controls {
	uint32_t present_mask;
	uint8_t ae_mode;
	int32_t iso;
	int64_t exposure_time_ns;
	int8_t ev_index;
	bool ae_lock;
	uint8_t awb_mode;
	bool awb_lock;
	uint8_t af_mode;
	float focus_distance; /* in diopters, 0 = infinity */
	float zoom;
	bool torch;
};

struct scrcpy_camera_caps {
	char camera_id[256];
	uint8_t facing; /* 0 front, 1 back, 2 external */
	uint8_t hardware_level;
	uint32_t flags;
	int32_t iso_min;
	int32_t iso_max;
	int64_t exposure_min_ns;
	int64_t exposure_max_ns;
	int64_t frame_duration_ns;
	int8_t ev_min;
	int8_t ev_max;
	int32_t ev_step_num;
	int32_t ev_step_den;
	uint8_t awb_mode_count;
	uint8_t awb_modes[SCRCPY_CAMERA_MAX_MODES];
	uint8_t af_mode_count;
	uint8_t af_modes[SCRCPY_CAMERA_MAX_MODES];
	float min_focus_distance; /* in diopters, 0 if fixed focus */
	float hyperfocal_distance;
	float zoom_min;
	float zoom_max;
};

struct scrcpy_camera_state {
	uint32_t applied_mask;
	uint32_t rejected_mask;
	uint8_t ae_mode;
	int ae_state;
	bool ae_lock;
	int awb_mode;
	int awb_state;
	bool awb_lock;
	int af_mode;
	int af_state;
	int32_t iso;
	int64_t exposure_time_ns;
	int64_t frame_duration_ns;
	int8_t ev_index;
	float focus_distance;
	float zoom;
};

enum scrcpy_device_msg_kind {
	SCRCPY_DEVICE_MSG_KIND_IGNORED, /* a valid message not relevant to camera controls (e.g. clipboard) */
	SCRCPY_DEVICE_MSG_KIND_CAMERA_CAPABILITIES,
	SCRCPY_DEVICE_MSG_KIND_CAMERA_STATE,
};

struct scrcpy_device_msg {
	enum scrcpy_device_msg_kind kind;
	uint8_t type;
	union {
		struct scrcpy_camera_caps caps;
		struct scrcpy_camera_state state;
	};
};

/* Serialize a CAMERA_SET_CONTROLS message into buf (which must be at least SCRCPY_CAMERA_CONTROLS_MSG_SIZE bytes).
 * Return the message size. */
size_t scrcpy_camera_controls_serialize(const struct scrcpy_camera_controls *controls, uint8_t *buf);

/* Serialize a CAMERA_AF_TRIGGER message into buf (at least SCRCPY_CAMERA_AF_TRIGGER_MSG_SIZE bytes).
 * Return the message size. */
size_t scrcpy_camera_af_trigger_serialize(uint8_t action, uint8_t *buf);

/* Parse one device message from the beginning of buf.
 * Return the number of bytes consumed (> 0) if a full message was parsed, 0 if more data is needed, or -1 on a protocol
 * error (unknown message type or invalid payload), after which the stream cannot be resynchronized. */
long scrcpy_device_msg_parse(const uint8_t *buf, size_t len, struct scrcpy_device_msg *msg);

/* Whether two snapshots are equal (only the fields present are compared) */
bool scrcpy_camera_controls_equal(const struct scrcpy_camera_controls *a, const struct scrcpy_camera_controls *b);

/* Append the server parameters (" camera_ae=manual camera_iso=400 ...") equivalent to the snapshot to dst
 * (a NUL-terminated string of capacity dst_size). Zoom and torch use the existing camera_zoom and camera_torch
 * parameters. Return false if dst is too small. */
bool scrcpy_camera_controls_append_params(const struct scrcpy_camera_controls *controls, char *dst, size_t dst_size);

const char *scrcpy_camera_awb_mode_param(uint8_t mode);
const char *scrcpy_camera_af_mode_param(uint8_t mode);
