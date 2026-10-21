/*
 * Unit tests for the camera controls protocol.
 *
 * The expected bytes are the same as in the server tests (ControlMessageReaderTest and DeviceMessageWriterTest in
 * rahadi23/scrcpy), so that both sides agree on the wire format.
 *
 * Build and run (no OBS dependency):
 *     gcc -std=c11 -Wall -Wextra -Isrc tests/test-camera-protocol.c src/camera-protocol.c -o test-camera-protocol
 *     ./test-camera-protocol
 */

#include "camera-protocol.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond)                                                                \
	do {                                                                       \
		++checks;                                                          \
		if (!(cond)) {                                                     \
			++failures;                                                \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
		}                                                                  \
	} while (0)

static void test_serialize_controls(void)
{
	struct scrcpy_camera_controls c = {
		.present_mask = SCRCPY_CAMERA_FIELD_ALL,
		.ae_mode = SCRCPY_CAMERA_AE_MODE_MANUAL,
		.iso = 400,
		.exposure_time_ns = 16666666,
		.ev_index = -7,
		.ae_lock = true,
		.awb_mode = SCRCPY_CAMERA_AWB_MODE_DAYLIGHT,
		.awb_lock = true,
		.af_mode = SCRCPY_CAMERA_AF_MODE_OFF,
		.focus_distance = 2.5f,
		.zoom = 1.75f,
		.torch = true,
	};

	/* Same bytes as writeCameraControlsV1() in ControlMessageReaderTest */
	static const uint8_t expected[] = {
		0xC0, 0x00, 0x21,                               /* type, payload length 33 */
		0x01,                                           /* version */
		0x00, 0x00, 0x07, 0xFF,                         /* present mask */
		0x01,                                           /* ae mode manual */
		0x00, 0x00, 0x01, 0x90,                         /* iso 400 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x50, 0x2A, /* exposure 16666666 */
		0xF9,                                           /* ev -7 */
		0x01,                                           /* ae lock */
		0x05,                                           /* awb daylight */
		0x01,                                           /* awb lock */
		0x00,                                           /* af off */
		0x40, 0x20, 0x00, 0x00,                         /* focus 2.5f */
		0x3F, 0xE0, 0x00, 0x00,                         /* zoom 1.75f */
		0x01,                                           /* torch */
		0x00,                                           /* reserved */
	};

	uint8_t buf[SCRCPY_CAMERA_CONTROLS_MSG_SIZE];
	size_t len = scrcpy_camera_controls_serialize(&c, buf);
	CHECK(len == sizeof(expected));
	CHECK(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_controls_masks_unknown_bits(void)
{
	struct scrcpy_camera_controls c = {.present_mask = 0xFFFFFFFF};
	uint8_t buf[SCRCPY_CAMERA_CONTROLS_MSG_SIZE];
	scrcpy_camera_controls_serialize(&c, buf);
	CHECK(buf[4] == 0x00 && buf[5] == 0x00 && buf[6] == 0x07 && buf[7] == 0xFF);
}

static void test_serialize_af_trigger(void)
{
	uint8_t buf[SCRCPY_CAMERA_AF_TRIGGER_MSG_SIZE];
	size_t len = scrcpy_camera_af_trigger_serialize(SCRCPY_CAMERA_AF_TRIGGER_START, buf);
	static const uint8_t expected[] = {0xC1, 0x00, 0x01, 0x01};
	CHECK(len == sizeof(expected));
	CHECK(!memcmp(buf, expected, sizeof(expected)));
}

/* Same bytes as testSerializeCameraCapabilities() in DeviceMessageWriterTest */
static const uint8_t CAPS_MSG[] = {
	0xC0, 0x00, 0x4F,                               /* type, payload length 79 */
	0x01,                                           /* version */
	0x01, '0',                                      /* camera id "0" */
	0x01,                                           /* facing back */
	0x01,                                           /* hardware level */
	0x00, 0x00, 0x00, 0x0B,                         /* flags: manual sensor, calibrated, ae lock */
	0x00, 0x00, 0x00, 0x32,                         /* iso min 50 */
	0x00, 0x00, 0x0C, 0x80,                         /* iso max 3200 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x4C, 0x08, /* exposure min 85000 */
	0x00, 0x00, 0x00, 0x00, 0x05, 0xF5, 0xE1, 0x00, /* exposure max 100000000 */
	0x00, 0x00, 0x00, 0x00, 0x01, 0xFC, 0xA0, 0x55, /* frame duration 33333333 */
	0xEC, 0x14,                                     /* ev -20..20 */
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0A, /* ev step 1/10 */
	0x06, 0x00, 0x01, 0x02, 0x03, 0x05, 0x06,       /* awb modes */
	0x04, 0x00, 0x01, 0x03, 0x04,                   /* af modes */
	0x41, 0x20, 0x00, 0x00,                         /* min focus 10f */
	0x3E, 0x8E, 0x2E, 0xB2,                         /* hyperfocal 0.2777f */
	0x3F, 0x80, 0x00, 0x00,                         /* zoom min 1f */
	0x41, 0x00, 0x00, 0x00,                         /* zoom max 8f */
};

static void test_parse_caps(void)
{
	struct scrcpy_device_msg msg;
	long r = scrcpy_device_msg_parse(CAPS_MSG, sizeof(CAPS_MSG), &msg);
	CHECK(r == (long)sizeof(CAPS_MSG));
	CHECK(msg.kind == SCRCPY_DEVICE_MSG_KIND_CAMERA_CAPABILITIES);
	struct scrcpy_camera_caps *c = &msg.caps;
	CHECK(!strcmp(c->camera_id, "0"));
	CHECK(c->facing == 1);
	CHECK(c->flags == (SCRCPY_CAMERA_CAP_MANUAL_SENSOR | SCRCPY_CAMERA_CAP_FOCUS_CALIBRATED | SCRCPY_CAMERA_CAP_AE_LOCK));
	CHECK(c->iso_min == 50 && c->iso_max == 3200);
	CHECK(c->exposure_min_ns == 85000 && c->exposure_max_ns == 100000000);
	CHECK(c->frame_duration_ns == 33333333);
	CHECK(c->ev_min == -20 && c->ev_max == 20);
	CHECK(c->ev_step_num == 1 && c->ev_step_den == 10);
	CHECK(c->awb_mode_count == 6 && c->awb_modes[4] == 5 && c->awb_modes[5] == 6);
	CHECK(c->af_mode_count == 4 && c->af_modes[2] == 3);
	CHECK(c->min_focus_distance == 10.0f);
	CHECK(c->hyperfocal_distance == 0.2777f);
	CHECK(c->zoom_min == 1.0f && c->zoom_max == 8.0f);
}

static void test_parse_incomplete(void)
{
	struct scrcpy_device_msg msg;
	/* Every strict prefix is incomplete */
	for (size_t i = 0; i < sizeof(CAPS_MSG); ++i)
		CHECK(scrcpy_device_msg_parse(CAPS_MSG, i, &msg) == 0);
}

/* Same bytes as testSerializeCameraState() in DeviceMessageWriterTest */
static const uint8_t STATE_MSG[] = {
	0xC1, 0x00, 0x2E,                               /* type, payload length 46 */
	0x01,                                           /* version */
	0x00, 0x00, 0x07, 0xFF,                         /* applied */
	0x00, 0x00, 0x01, 0x00,                         /* rejected */
	0x01, 0x02, 0x01, 0x05, 0x03, 0x00, 0x03, 0x02, /* modes, 3A states and locks */
	0x00, 0x00, 0x01, 0x90,                         /* iso 400 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x50, 0x2A, /* exposure 16666666 */
	0x00, 0x00, 0x00, 0x00, 0x01, 0xFC, 0xA0, 0x55, /* frame duration 33333333 */
	0xFD,                                           /* ev -3 */
	0x3F, 0xC0, 0x00, 0x00,                         /* focus 1.5f */
	0x40, 0x00, 0x00, 0x00,                         /* zoom 2f */
};

static void test_parse_state(void)
{
	struct scrcpy_device_msg msg;
	long r = scrcpy_device_msg_parse(STATE_MSG, sizeof(STATE_MSG), &msg);
	CHECK(r == (long)sizeof(STATE_MSG));
	CHECK(msg.kind == SCRCPY_DEVICE_MSG_KIND_CAMERA_STATE);
	struct scrcpy_camera_state *s = &msg.state;
	CHECK(s->applied_mask == 0x7FF && s->rejected_mask == 0x100);
	CHECK(s->ae_mode == 1 && s->ae_state == 2 && s->ae_lock);
	CHECK(s->awb_mode == 5 && s->awb_state == 3 && !s->awb_lock);
	CHECK(s->af_mode == 3 && s->af_state == 2);
	CHECK(s->iso == 400);
	CHECK(s->exposure_time_ns == 16666666 && s->frame_duration_ns == 33333333);
	CHECK(s->ev_index == -3);
	CHECK(s->focus_distance == 1.5f && s->zoom == 2.0f);
}

static void test_parse_state_unknown_values_and_trailing_fields(void)
{
	uint8_t buf[sizeof(STATE_MSG) + 4];
	memcpy(buf, STATE_MSG, sizeof(STATE_MSG));
	buf[2] = 0x2E + 4; /* a later version appended 4 bytes */
	buf[13] = 0xFF;    /* unknown ae state */
	memset(&buf[sizeof(STATE_MSG)], 0x42, 4);

	struct scrcpy_device_msg msg;
	long r = scrcpy_device_msg_parse(buf, sizeof(buf), &msg);
	CHECK(r == (long)sizeof(buf));
	CHECK(msg.kind == SCRCPY_DEVICE_MSG_KIND_CAMERA_STATE);
	CHECK(msg.state.ae_state == -1);
	CHECK(msg.state.zoom == 2.0f);
}

static void test_parse_state_too_short(void)
{
	static const uint8_t msg_bytes[] = {0xC1, 0x00, 0x02, 0x01, 0x00};
	struct scrcpy_device_msg msg;
	CHECK(scrcpy_device_msg_parse(msg_bytes, sizeof(msg_bytes), &msg) == -1);
}

static void test_parse_upstream_messages_are_skipped(void)
{
	/* clipboard "abc", ack clipboard, uhid output with 2 bytes, then a state */
	uint8_t buf[64 + sizeof(STATE_MSG)];
	size_t n = 0;
	static const uint8_t clipboard[] = {0x00, 0x00, 0x00, 0x00, 0x03, 'a', 'b', 'c'};
	static const uint8_t ack[] = {0x01, 1, 2, 3, 4, 5, 6, 7, 8};
	static const uint8_t uhid[] = {0x02, 0x00, 0x2A, 0x00, 0x02, 0x11, 0x22};
	memcpy(buf + n, clipboard, sizeof(clipboard));
	n += sizeof(clipboard);
	memcpy(buf + n, ack, sizeof(ack));
	n += sizeof(ack);
	memcpy(buf + n, uhid, sizeof(uhid));
	n += sizeof(uhid);
	memcpy(buf + n, STATE_MSG, sizeof(STATE_MSG));
	n += sizeof(STATE_MSG);

	struct scrcpy_device_msg msg;
	size_t pos = 0;
	long r = scrcpy_device_msg_parse(buf + pos, n - pos, &msg);
	CHECK(r == (long)sizeof(clipboard) && msg.kind == SCRCPY_DEVICE_MSG_KIND_IGNORED);
	pos += (size_t)r;
	r = scrcpy_device_msg_parse(buf + pos, n - pos, &msg);
	CHECK(r == (long)sizeof(ack) && msg.kind == SCRCPY_DEVICE_MSG_KIND_IGNORED);
	pos += (size_t)r;
	r = scrcpy_device_msg_parse(buf + pos, n - pos, &msg);
	CHECK(r == (long)sizeof(uhid) && msg.kind == SCRCPY_DEVICE_MSG_KIND_IGNORED);
	pos += (size_t)r;
	r = scrcpy_device_msg_parse(buf + pos, n - pos, &msg);
	CHECK(r == (long)sizeof(STATE_MSG) && msg.kind == SCRCPY_DEVICE_MSG_KIND_CAMERA_STATE);
	pos += (size_t)r;
	CHECK(pos == n);
}

static void test_parse_unknown_type(void)
{
	static const uint8_t msg_bytes[] = {0x7F, 0x00};
	struct scrcpy_device_msg msg;
	CHECK(scrcpy_device_msg_parse(msg_bytes, sizeof(msg_bytes), &msg) == -1);
}

static void test_controls_equal(void)
{
	struct scrcpy_camera_controls a = {.present_mask = SCRCPY_CAMERA_FIELD_ISO, .iso = 400, .zoom = 3.0f};
	struct scrcpy_camera_controls b = {.present_mask = SCRCPY_CAMERA_FIELD_ISO, .iso = 400, .zoom = 1.0f};
	/* zoom is not present: not compared */
	CHECK(scrcpy_camera_controls_equal(&a, &b));
	b.iso = 800;
	CHECK(!scrcpy_camera_controls_equal(&a, &b));
	b.iso = 400;
	b.present_mask |= SCRCPY_CAMERA_FIELD_ZOOM;
	CHECK(!scrcpy_camera_controls_equal(&a, &b));
}

static void test_append_params(void)
{
	struct scrcpy_camera_controls c = {
		.present_mask = SCRCPY_CAMERA_FIELD_ALL,
		.ae_mode = SCRCPY_CAMERA_AE_MODE_MANUAL,
		.iso = 400,
		.exposure_time_ns = 16666667,
		.ev_index = -3,
		.ae_lock = false,
		.awb_mode = SCRCPY_CAMERA_AWB_MODE_CLOUDY_DAYLIGHT,
		.awb_lock = true,
		.af_mode = SCRCPY_CAMERA_AF_MODE_OFF,
		.focus_distance = 2.5f,
		.zoom = 1.25f,
		.torch = false,
	};
	char params[512] = "video_source=camera";
	CHECK(scrcpy_camera_controls_append_params(&c, params, sizeof(params)));
	const char *expected = "video_source=camera camera_ae=manual camera_iso=400 camera_exposure_time=16666667 camera_ev=-3"
			       " camera_ae_lock=false camera_awb=cloudy camera_awb_lock=true camera_af=off"
			       " camera_focus_distance=2.500 camera_zoom=1.250 camera_torch=false";
	CHECK(!strcmp(params, expected));
	if (strcmp(params, expected))
		fprintf(stderr, "  got:      %s\n  expected: %s\n", params, expected);

	/* Only present fields are appended */
	struct scrcpy_camera_controls partial = {.present_mask = SCRCPY_CAMERA_FIELD_AF_MODE,
						 .af_mode = SCRCPY_CAMERA_AF_MODE_CONTINUOUS_VIDEO};
	char params2[128] = "";
	CHECK(scrcpy_camera_controls_append_params(&partial, params2, sizeof(params2)));
	CHECK(!strcmp(params2, " camera_af=continuous-video"));

	/* Too small */
	char small[16] = "";
	CHECK(!scrcpy_camera_controls_append_params(&c, small, sizeof(small)));
}

static void test_append_params_ignores_locale(void)
{
	/* A locale with a decimal comma must not break the server parameters */
	if (!setlocale(LC_NUMERIC, "de_DE.UTF-8") && !setlocale(LC_NUMERIC, "German_Germany.1252"))
		return; /* locale not available */
	struct scrcpy_camera_controls c = {.present_mask = SCRCPY_CAMERA_FIELD_ZOOM, .zoom = 2.5f};
	char params[64] = "";
	CHECK(scrcpy_camera_controls_append_params(&c, params, sizeof(params)));
	CHECK(!strcmp(params, " camera_zoom=2.500"));
	setlocale(LC_NUMERIC, "C");
}

int main(void)
{
	test_serialize_controls();
	test_serialize_controls_masks_unknown_bits();
	test_serialize_af_trigger();
	test_parse_caps();
	test_parse_incomplete();
	test_parse_state();
	test_parse_state_unknown_values_and_trailing_fields();
	test_parse_state_too_short();
	test_parse_upstream_messages_are_skipped();
	test_parse_unknown_type();
	test_controls_equal();
	test_append_params();
	test_append_params_ignores_locale();

	printf("%d/%d checks passed\n", checks - failures, checks);
	return failures ? 1 : 0;
}
