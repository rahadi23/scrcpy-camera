/*
 * OBS properties for the live camera controls
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

#include <obs-module.h>

#include "camera-protocol.h"

/* Settings keys. They are not session settings: changing them never restarts the stream. */
#define CAMCTL_AE_MODE "cam_ae_mode"
#define CAMCTL_EV "cam_ev"
#define CAMCTL_ISO "cam_iso"
#define CAMCTL_SHUTTER "cam_shutter"
#define CAMCTL_AE_LOCK "cam_ae_lock"
#define CAMCTL_AWB_MODE "cam_awb_mode"
#define CAMCTL_AWB_LOCK "cam_awb_lock"
#define CAMCTL_AF_MODE "cam_af_mode"
#define CAMCTL_FOCUS_DISTANCE "cam_focus_distance"
#define CAMCTL_ZOOM "cam_zoom"
#define CAMCTL_TORCH "cam_torch"

/* Snapshot of what the device reported, for building the properties */
struct camctl_device_info {
	bool has_caps;
	struct scrcpy_camera_caps caps;
	bool has_state;
	struct scrcpy_camera_state state;
};

typedef void (*camctl_af_trigger_fn)(void *data);

void camctl_props_defaults(obs_data_t *settings);

/* Build the snapshot to send from the settings. caps may be NULL (not connected yet): then the exposure compensation
 * is not sent, since its unit (the EV step) is device-specific. */
void camctl_controls_from_settings(obs_data_t *settings, const struct scrcpy_camera_caps *caps,
				   struct scrcpy_camera_controls *controls);

/*
 * Add the "Camera controls" group.
 *
 * enabled: whether the selected server supports camera controls (otherwise only an explanation is shown).
 * info: what the device reported (may be NULL), used for the ranges, the available modes and the status line.
 * data: passed to the callbacks (the source).
 * get_info: called by the modified callbacks to get the latest device info (e.g. to initialize manual exposure from
 * the current auto exposure values).
 */
void camctl_props_add(obs_properties_t *props, bool enabled, const struct camctl_device_info *info, void *data,
		      camctl_af_trigger_fn af_trigger, bool (*get_info)(void *data, struct camctl_device_info *info));

/* Format a one-line status from the device state (for logs and the status line) */
void camctl_format_status(const struct camctl_device_info *info, char *dst, size_t dst_size);
