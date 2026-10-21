/*
 * Unit tests for the camera list parser.
 *
 * Build and run from the repository root (no OBS dependency):
 *     gcc -std=c11 -Wall -Wextra -Isrc tests/test-camera-list.c src/camera-list.c -o test-camera-list
 *     ./test-camera-list tests/camera-list-sm-n975f.txt
 */

#include "camera-list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond)                                                                              \
	do {                                                                                     \
		++checks;                                                                        \
		if (!(cond)) {                                                                   \
			++failures;                                                              \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
		}                                                                                \
	} while (0)

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *data = malloc((size_t)size + 1);
	size_t n = fread(data, 1, (size_t)size, f);
	data[n] = '\0';
	fclose(f);
	return data;
}

static bool has_size(const struct camera_list_entry *c, int w, int h)
{
	for (size_t i = 0; i < c->size_count; ++i) {
		if (c->sizes[i].width == w && c->sizes[i].height == h)
			return true;
	}
	return false;
}

/* Real output of a Galaxy Note10+ (SM-N975F, Android 12), with CRLF line endings */
static void test_parse_device_output(const char *path)
{
	char *text = read_file(path);
	CHECK(text != NULL);
	if (!text)
		return;

	struct camera_list *list = calloc(1, sizeof(*list));
	CHECK(camera_list_parse(text, list));
	CHECK(list->count == 4);

	const struct camera_list_entry *c0 = camera_list_find(list, "0");
	CHECK(c0 != NULL);
	if (c0) {
		CHECK(!strcmp(c0->facing, "back"));
		CHECK(!strncmp(c0->description, "back, 4032x3024, fps={15, 20, 24, 30}", 37));
		CHECK(c0->size_count == 22);
		CHECK(c0->sizes[0].width == 4032 && c0->sizes[0].height == 3024);
		CHECK(has_size(c0, 3840, 2160));
		CHECK(has_size(c0, 1920, 1080));
		CHECK(has_size(c0, 1280, 720));
		CHECK(c0->high_speed_count == 2);
		CHECK(c0->high_speed_sizes[0].width == 1280 && c0->high_speed_sizes[0].height == 720);
		CHECK(c0->high_speed_sizes[0].fps_count == 2);
		CHECK(camera_list_size_has_fps(&c0->high_speed_sizes[0], 120));
		CHECK(camera_list_size_has_fps(&c0->high_speed_sizes[0], 240));
		CHECK(!camera_list_size_has_fps(&c0->high_speed_sizes[0], 60));
	}

	const struct camera_list_entry *c1 = camera_list_find(list, "1");
	CHECK(c1 != NULL && !strcmp(c1->facing, "front"));

	const struct camera_list_entry *c2 = camera_list_find(list, "2");
	CHECK(c2 != NULL);
	if (c2)
		CHECK(c2->high_speed_count == 0); /* the ultra-wide camera has no high-speed sizes */

	CHECK(camera_list_find(list, "4") == NULL); /* depth camera, not listed */

	free(list);
	free(text);
}

static void test_parse_no_list(void)
{
	struct camera_list *list = calloc(1, sizeof(*list));
	CHECK(!camera_list_parse("[server] ERROR: Could not open camera", list));
	CHECK(!camera_list_parse(NULL, list));
	CHECK(list->count == 0);
	free(list);
}

static void test_parse_lf_and_other_lines(void)
{
	const char *text = "[server] INFO: Device: [samsung] samsung SM-N975F (Android 12)\n"
			   "[server] INFO: List of cameras:\n"
			   "    --camera-id=10    (external, 1920x1080, fps={30})\n"
			   "        - 1920x1080\n"
			   "        - garbage\n"
			   "        - 640x480\n"
			   "[server] WARN: something\n";
	struct camera_list *list = calloc(1, sizeof(*list));
	CHECK(camera_list_parse(text, list));
	CHECK(list->count == 1);
	CHECK(!strcmp(list->cameras[0].id, "10"));
	CHECK(!strcmp(list->cameras[0].facing, "external"));
	CHECK(list->cameras[0].size_count == 2);
	CHECK(list->cameras[0].high_speed_count == 0);
	free(list);
}

int main(int argc, char *argv[])
{
	test_parse_device_output(argc > 1 ? argv[1] : "tests/camera-list-sm-n975f.txt");
	test_parse_no_list();
	test_parse_lf_and_other_lines();

	printf("%d/%d checks passed\n", checks - failures, checks);
	return failures ? 1 : 0;
}
