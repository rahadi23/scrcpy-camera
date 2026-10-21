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

#include "camera-controls-props.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CAMCTL_GROUP "cam_controls"
#define CAMCTL_NOTE "cam_note"
#define CAMCTL_STATUS "cam_status"
#define CAMCTL_REFRESH "cam_refresh"
#define CAMCTL_FOCUS_ONCE "cam_focus_once"

#define DEFAULT_ISO 400
#define DEFAULT_SHUTTER_NS 16666667LL /* 1/60 s */

struct camctl_props_ctx {
	void *data;
	camctl_af_trigger_fn af_trigger;
	bool (*get_info)(void *data, struct camctl_device_info *info);
};

/* Shutter speeds offered in manual exposure, as the denominator of 1/x s */
static const int SHUTTER_STOPS[] = {15, 24, 25, 30, 48, 50, 60, 90, 100, 120, 125, 180, 250, 320, 500, 1000, 2000, 4000, 8000};

struct mode_name {
	uint8_t mode;
	const char *name;
};

static const struct mode_name AWB_NAMES[] = {
	{SCRCPY_CAMERA_AWB_MODE_AUTO, "Auto"},
	{SCRCPY_CAMERA_AWB_MODE_DAYLIGHT, "Daylight"},
	{SCRCPY_CAMERA_AWB_MODE_CLOUDY_DAYLIGHT, "Cloudy"},
	{SCRCPY_CAMERA_AWB_MODE_SHADE, "Shade"},
	{SCRCPY_CAMERA_AWB_MODE_TWILIGHT, "Twilight"},
	{SCRCPY_CAMERA_AWB_MODE_INCANDESCENT, "Incandescent (tungsten)"},
	{SCRCPY_CAMERA_AWB_MODE_FLUORESCENT, "Fluorescent"},
	{SCRCPY_CAMERA_AWB_MODE_WARM_FLUORESCENT, "Warm fluorescent"},
};

static const struct mode_name AF_NAMES[] = {
	{SCRCPY_CAMERA_AF_MODE_CONTINUOUS_VIDEO, "Continuous (video)"},
	{SCRCPY_CAMERA_AF_MODE_CONTINUOUS_PICTURE, "Continuous (picture, faster)"},
	{SCRCPY_CAMERA_AF_MODE_AUTO, "Single (press \"Focus once\")"},
	{SCRCPY_CAMERA_AF_MODE_MACRO, "Macro (single)"},
	{SCRCPY_CAMERA_AF_MODE_OFF, "Manual"},
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static bool caps_has_mode(const uint8_t *modes, uint8_t count, uint8_t mode)
{
	for (uint8_t i = 0; i < count; ++i) {
		if (modes[i] == mode)
			return true;
	}
	return false;
}

static double ev_step(const struct scrcpy_camera_caps *caps)
{
	if (!caps || caps->ev_step_num <= 0 || caps->ev_step_den <= 0)
		return 0;
	return (double)caps->ev_step_num / caps->ev_step_den;
}

static bool manual_focus_supported(const struct scrcpy_camera_caps *caps)
{
	return (caps->flags & SCRCPY_CAMERA_CAP_AF_OFF_SUPPORTED) && caps->min_focus_distance > 0 &&
	       !(caps->flags & SCRCPY_CAMERA_CAP_HIGH_SPEED_SESSION);
}

void camctl_props_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, CAMCTL_AE_MODE, "auto");
	obs_data_set_default_double(settings, CAMCTL_EV, 0.0);
	obs_data_set_default_int(settings, CAMCTL_ISO, DEFAULT_ISO);
	obs_data_set_default_int(settings, CAMCTL_SHUTTER, DEFAULT_SHUTTER_NS);
	obs_data_set_default_bool(settings, CAMCTL_AE_LOCK, false);
	obs_data_set_default_int(settings, CAMCTL_AWB_MODE, SCRCPY_CAMERA_AWB_MODE_AUTO);
	obs_data_set_default_bool(settings, CAMCTL_AWB_LOCK, false);
	obs_data_set_default_int(settings, CAMCTL_AF_MODE, SCRCPY_CAMERA_AF_MODE_CONTINUOUS_VIDEO);
	obs_data_set_default_double(settings, CAMCTL_FOCUS_DISTANCE, 0.0);
	obs_data_set_default_double(settings, CAMCTL_ZOOM, 1.0);
	obs_data_set_default_bool(settings, CAMCTL_TORCH, false);
}

void camctl_controls_from_settings(obs_data_t *settings, const struct scrcpy_camera_caps *caps,
				   struct scrcpy_camera_controls *c)
{
	memset(c, 0, sizeof(*c));

	const char *ae = obs_data_get_string(settings, CAMCTL_AE_MODE);
	bool manual = ae && strcmp(ae, "manual") == 0;

	c->present_mask = SCRCPY_CAMERA_FIELD_AE_MODE | SCRCPY_CAMERA_FIELD_AWB_MODE | SCRCPY_CAMERA_FIELD_AWB_LOCK |
			  SCRCPY_CAMERA_FIELD_AF_MODE | SCRCPY_CAMERA_FIELD_ZOOM | SCRCPY_CAMERA_FIELD_TORCH;

	c->ae_mode = manual ? SCRCPY_CAMERA_AE_MODE_MANUAL : SCRCPY_CAMERA_AE_MODE_AUTO;
	if (manual) {
		c->present_mask |= SCRCPY_CAMERA_FIELD_ISO | SCRCPY_CAMERA_FIELD_EXPOSURE_TIME;
		c->iso = (int32_t)obs_data_get_int(settings, CAMCTL_ISO);
		c->exposure_time_ns = obs_data_get_int(settings, CAMCTL_SHUTTER);
	} else {
		double step = ev_step(caps);
		if (step > 0) {
			double index = round(obs_data_get_double(settings, CAMCTL_EV) / step);
			if (index < -128)
				index = -128;
			if (index > 127)
				index = 127;
			c->present_mask |= SCRCPY_CAMERA_FIELD_EV;
			c->ev_index = (int8_t)index;
		}
		c->present_mask |= SCRCPY_CAMERA_FIELD_AE_LOCK;
		c->ae_lock = obs_data_get_bool(settings, CAMCTL_AE_LOCK);
	}

	c->awb_mode = (uint8_t)obs_data_get_int(settings, CAMCTL_AWB_MODE);
	c->awb_lock = obs_data_get_bool(settings, CAMCTL_AWB_LOCK);

	c->af_mode = (uint8_t)obs_data_get_int(settings, CAMCTL_AF_MODE);
	if (c->af_mode == SCRCPY_CAMERA_AF_MODE_OFF) {
		c->present_mask |= SCRCPY_CAMERA_FIELD_FOCUS_DISTANCE;
		c->focus_distance = (float)obs_data_get_double(settings, CAMCTL_FOCUS_DISTANCE);
	}

	c->zoom = (float)obs_data_get_double(settings, CAMCTL_ZOOM);
	c->torch = obs_data_get_bool(settings, CAMCTL_TORCH);
}

static const char *ae_state_name(int state)
{
	switch (state) {
	case 0:
		return "inactive";
	case 1:
		return "adjusting";
	case 2:
		return "converged";
	case 3:
		return "locked";
	case 4:
		return "flash required";
	case 5:
		return "precapture";
	default:
		return "?";
	}
}

static const char *af_state_name(int state)
{
	switch (state) {
	case 0:
		return "inactive";
	case 1:
	case 3:
		return "scanning";
	case 2:
		return "focused";
	case 4:
		return "focused (locked)";
	case 5:
		return "not focused (locked)";
	case 6:
		return "unfocused";
	default:
		return "?";
	}
}

void camctl_format_status(const struct camctl_device_info *info, char *dst, size_t dst_size)
{
	if (!info || !info->has_state) {
		snprintf(dst, dst_size, "Not connected (start the source to read the camera state)");
		return;
	}

	const struct scrcpy_camera_state *s = &info->state;
	char shutter[32];
	if (s->exposure_time_ns > 0)
		snprintf(shutter, sizeof(shutter), "1/%.0f s", 1e9 / (double)s->exposure_time_ns);
	else
		snprintf(shutter, sizeof(shutter), "?");

	char rejected[64] = "";
	if (s->rejected_mask)
		snprintf(rejected, sizeof(rejected), " | some values not supported or clamped (0x%x)", s->rejected_mask);

	snprintf(dst, dst_size, "ISO %d, %s, AE %s%s, AWB %s, AF %s, zoom %.1fx%s", (int)s->iso, shutter,
		 s->ae_mode == SCRCPY_CAMERA_AE_MODE_MANUAL ? "manual" : ae_state_name(s->ae_state),
		 s->ae_lock ? " (locked)" : "", s->awb_lock ? "locked" : (s->awb_state == 2 ? "converged" : "adjusting"),
		 af_state_name(s->af_state), s->zoom, rejected);
}

static int64_t nearest_shutter_stop(int64_t exposure_ns)
{
	int64_t best = DEFAULT_SHUTTER_NS;
	double best_distance = INFINITY;
	for (size_t i = 0; i < ARRAY_LEN(SHUTTER_STOPS); ++i) {
		int64_t ns = 1000000000LL / SHUTTER_STOPS[i];
		/* Compare in log space: stops are geometric */
		double distance = fabs(log((double)ns) - log((double)exposure_ns));
		if (distance < best_distance) {
			best_distance = distance;
			best = ns;
		}
	}
	return best;
}

static void update_visibility(obs_properties_t *props, obs_data_t *settings)
{
	const char *ae = obs_data_get_string(settings, CAMCTL_AE_MODE);
	bool manual = ae && strcmp(ae, "manual") == 0;
	int af = (int)obs_data_get_int(settings, CAMCTL_AF_MODE);

	obs_property_t *p;
	if ((p = obs_properties_get(props, CAMCTL_ISO)))
		obs_property_set_visible(p, manual);
	if ((p = obs_properties_get(props, CAMCTL_SHUTTER)))
		obs_property_set_visible(p, manual);
	if ((p = obs_properties_get(props, CAMCTL_EV)))
		obs_property_set_visible(p, !manual);
	if ((p = obs_properties_get(props, CAMCTL_AE_LOCK)))
		obs_property_set_visible(p, !manual);
	if ((p = obs_properties_get(props, CAMCTL_FOCUS_DISTANCE)))
		obs_property_set_visible(p, af == SCRCPY_CAMERA_AF_MODE_OFF);
	if ((p = obs_properties_get(props, CAMCTL_FOCUS_ONCE)))
		obs_property_set_visible(p, af == SCRCPY_CAMERA_AF_MODE_AUTO || af == SCRCPY_CAMERA_AF_MODE_MACRO ||
						    af == SCRCPY_CAMERA_AF_MODE_CONTINUOUS_VIDEO ||
						    af == SCRCPY_CAMERA_AF_MODE_CONTINUOUS_PICTURE);
}

static bool ae_mode_modified(void *priv, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	struct camctl_props_ctx *ctx = priv;

	const char *ae = obs_data_get_string(settings, CAMCTL_AE_MODE);
	bool manual = ae && strcmp(ae, "manual") == 0;

	/* Switching to manual for the first time: start from the current auto exposure values, to avoid a brightness
	 * jump. Modified callbacks also run each time the properties are shown, so never overwrite values chosen by the
	 * user. */
	struct camctl_device_info info;
	if (manual && !obs_data_has_user_value(settings, CAMCTL_ISO) &&
	    !obs_data_has_user_value(settings, CAMCTL_SHUTTER) && ctx && ctx->get_info && ctx->get_info(ctx->data, &info) &&
	    info.has_state &&
	    info.state.ae_mode == SCRCPY_CAMERA_AE_MODE_AUTO && info.state.iso > 0 && info.state.exposure_time_ns > 0) {
		obs_data_set_int(settings, CAMCTL_ISO, info.state.iso);
		obs_data_set_int(settings, CAMCTL_SHUTTER, nearest_shutter_stop(info.state.exposure_time_ns));
	}

	update_visibility(props, settings);
	return true;
}

static bool af_mode_modified(void *priv, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	struct camctl_props_ctx *ctx = priv;

	int af = (int)obs_data_get_int(settings, CAMCTL_AF_MODE);
	/* Switching to manual focus for the first time: start from the current lens position (see ae_mode_modified()) */
	struct camctl_device_info info;
	if (af == SCRCPY_CAMERA_AF_MODE_OFF && !obs_data_has_user_value(settings, CAMCTL_FOCUS_DISTANCE) && ctx &&
	    ctx->get_info && ctx->get_info(ctx->data, &info) &&
	    info.has_state && info.state.af_mode != SCRCPY_CAMERA_AF_MODE_OFF)
		obs_data_set_double(settings, CAMCTL_FOCUS_DISTANCE, info.state.focus_distance);

	update_visibility(props, settings);
	return true;
}

static bool focus_once_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct camctl_props_ctx *ctx = data;
	if (ctx && ctx->af_trigger)
		ctx->af_trigger(ctx->data);
	return false;
}

static bool refresh_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(property);
	struct camctl_props_ctx *ctx = data;

	struct camctl_device_info info;
	if (ctx && ctx->get_info && ctx->get_info(ctx->data, &info)) {
		char status[256];
		camctl_format_status(&info, status, sizeof(status));
		obs_property_t *p = obs_properties_get(props, CAMCTL_STATUS);
		if (p)
			obs_property_set_description(p, status);
	}
	return true; /* refresh the properties view */
}

static void add_mode_list(obs_property_t *list, const struct mode_name *names, size_t count,
			  const struct scrcpy_camera_caps *caps, bool awb)
{
	for (size_t i = 0; i < count; ++i) {
		if (caps) {
			bool supported = awb ? caps_has_mode(caps->awb_modes, caps->awb_mode_count, names[i].mode)
					     : caps_has_mode(caps->af_modes, caps->af_mode_count, names[i].mode);
			if (!awb && names[i].mode == SCRCPY_CAMERA_AF_MODE_OFF)
				supported = manual_focus_supported(caps);
			if (!supported)
				continue;
		}
		obs_property_list_add_int(list, names[i].name, names[i].mode);
	}
}

void camctl_props_add(obs_properties_t *props, bool enabled, const struct camctl_device_info *info, void *data,
		      camctl_af_trigger_fn af_trigger, bool (*get_info)(void *data, struct camctl_device_info *info))
{
	obs_properties_t *group = obs_properties_create();
	struct camctl_props_ctx *ctx = bzalloc(sizeof(*ctx));
	ctx->data = data;
	ctx->af_trigger = af_trigger;
	ctx->get_info = get_info;
	obs_properties_set_param(group, ctx, bfree);

	obs_properties_add_group(props, CAMCTL_GROUP, "Camera controls", OBS_GROUP_NORMAL, group);

	if (!enabled) {
		obs_properties_add_text(group, CAMCTL_NOTE,
					"Live camera controls need the bundled camera controls server: select the scrcpy "
					"protocol version \"" SCRCPY_CAMCTL_SERVER_VERSION
					"\" and the Camera video source.",
					OBS_TEXT_INFO);
		return;
	}

	const struct scrcpy_camera_caps *caps = info && info->has_caps ? &info->caps : NULL;

	char status[256];
	camctl_format_status(info, status, sizeof(status));
	obs_properties_add_text(group, CAMCTL_STATUS, status, OBS_TEXT_INFO);
	obs_properties_add_button2(group, CAMCTL_REFRESH, "Refresh camera status", refresh_clicked, ctx);
	if (!caps) {
		obs_properties_add_text(group, CAMCTL_NOTE,
					"The ranges below are generic until the camera is connected: they are adjusted "
					"to what the camera supports once its capabilities are received.",
					OBS_TEXT_INFO);
	}

	/* Exposure */
	bool manual_sensor = !caps || ((caps->flags & SCRCPY_CAMERA_CAP_MANUAL_SENSOR) &&
				       !(caps->flags & SCRCPY_CAMERA_CAP_HIGH_SPEED_SESSION));
	obs_property_t *ae = obs_properties_add_list(group, CAMCTL_AE_MODE, "Exposure", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(ae, "Auto", "auto");
	if (manual_sensor)
		obs_property_list_add_string(ae, "Manual (ISO + shutter)", "manual");
	else
		obs_property_set_long_description(ae, "This camera does not support manual ISO and shutter speed.");
	obs_property_set_modified_callback2(ae, ae_mode_modified, ctx);

	double step = ev_step(caps);
	double ev_min = caps && step > 0 ? caps->ev_min * step : -2.0;
	double ev_max = caps && step > 0 ? caps->ev_max * step : 2.0;
	obs_property_t *ev = obs_properties_add_float_slider(group, CAMCTL_EV, "Exposure compensation (EV)", ev_min,
							     ev_max, step > 0 ? step : 0.1);
	obs_property_float_set_suffix(ev, " EV");

	int iso_min = caps && caps->iso_max > 0 ? caps->iso_min : 50;
	int iso_max = caps && caps->iso_max > 0 ? caps->iso_max : 3200;
	obs_properties_add_int_slider(group, CAMCTL_ISO, "ISO", iso_min, iso_max, 10);

	obs_property_t *shutter = obs_properties_add_list(group, CAMCTL_SHUTTER, "Shutter speed", OBS_COMBO_TYPE_LIST,
							  OBS_COMBO_FORMAT_INT);
	/* The exposure time cannot exceed the frame duration without lowering the frame rate */
	int64_t max_exposure = caps && caps->frame_duration_ns > 0 ? caps->frame_duration_ns : 1000000000LL / 15;
	if (caps && caps->exposure_max_ns > 0 && caps->exposure_max_ns < max_exposure)
		max_exposure = caps->exposure_max_ns;
	int64_t min_exposure = caps ? caps->exposure_min_ns : 0;
	for (size_t i = 0; i < ARRAY_LEN(SHUTTER_STOPS); ++i) {
		int64_t ns = 1000000000LL / SHUTTER_STOPS[i];
		/* allow 2% tolerance: 1/30 s must be offered at 30 fps */
		if (ns > max_exposure + max_exposure / 50 || ns < min_exposure)
			continue;
		char label[32];
		snprintf(label, sizeof(label), "1/%d s", SHUTTER_STOPS[i]);
		obs_property_list_add_int(shutter, label, ns);
	}
	obs_property_set_long_description(shutter, "Shutter speeds longer than one frame are not offered: they would "
						    "lower the frame rate.");

	obs_property_t *ae_lock = obs_properties_add_bool(group, CAMCTL_AE_LOCK, "Lock exposure");
	obs_property_set_long_description(ae_lock, "Locks the auto exposure once it has converged.");
	if (caps && !(caps->flags & SCRCPY_CAMERA_CAP_AE_LOCK))
		obs_property_set_enabled(ae_lock, false);

	/* White balance */
	obs_property_t *awb = obs_properties_add_list(group, CAMCTL_AWB_MODE, "White balance", OBS_COMBO_TYPE_LIST,
						      OBS_COMBO_FORMAT_INT);
	add_mode_list(awb, AWB_NAMES, ARRAY_LEN(AWB_NAMES), caps, true);

	obs_property_t *awb_lock = obs_properties_add_bool(group, CAMCTL_AWB_LOCK, "Lock white balance");
	obs_property_set_long_description(awb_lock, "Locks the auto white balance once it has converged.");
	if (caps && !(caps->flags & SCRCPY_CAMERA_CAP_AWB_LOCK))
		obs_property_set_enabled(awb_lock, false);

	/* Focus */
	obs_property_t *af = obs_properties_add_list(group, CAMCTL_AF_MODE, "Focus", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_INT);
	add_mode_list(af, AF_NAMES, ARRAY_LEN(AF_NAMES), caps, false);
	obs_property_set_modified_callback2(af, af_mode_modified, ctx);

	obs_properties_add_button2(group, CAMCTL_FOCUS_ONCE, "Focus once", focus_once_clicked, ctx);

	double min_focus = caps && caps->min_focus_distance > 0 ? caps->min_focus_distance : 10.0;
	char focus_label[128];
	snprintf(focus_label, sizeof(focus_label), "Focus distance (diopters: 0 = infinity, %.0f = %.0f cm)%s", min_focus,
		 100.0 / min_focus, caps && !(caps->flags & SCRCPY_CAMERA_CAP_FOCUS_CALIBRATED) ? " (uncalibrated)" : "");
	obs_properties_add_float_slider(group, CAMCTL_FOCUS_DISTANCE, focus_label, 0.0, min_focus, 0.05);

	/* Zoom and torch */
	double zoom_min = caps && caps->zoom_max > 0 ? caps->zoom_min : 1.0;
	double zoom_max = caps && caps->zoom_max > 0 ? caps->zoom_max : 8.0;
	obs_property_t *zoom = obs_properties_add_float_slider(group, CAMCTL_ZOOM, "Zoom", zoom_min, zoom_max, 0.1);
	obs_property_float_set_suffix(zoom, "x");

	obs_property_t *torch = obs_properties_add_bool(group, CAMCTL_TORCH, "Torch");
	if (caps && !(caps->flags & SCRCPY_CAMERA_CAP_HAS_FLASH))
		obs_property_set_visible(torch, false);
}
