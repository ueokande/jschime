#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <linux/joystick.h>

#define NAME_LENGTH 128
#define RING_BUFFER_LENGTH (1 << 6)

int detect_command(
		const __u8 *button_buffer, const __u32 *time_buffer,
		size_t buffer_length, size_t start_point,
    		const __u8 *command, size_t command_length,
    		__u32 time_interval) {

	if (command_length <= start_point + 1) {
		size_t mem = memcmp(command, button_buffer + start_point - command_length + 1, sizeof(__u8) * command_length);
		__u32 time = time_buffer[start_point] - time_buffer[start_point - command_length];
		if (mem == 0 && time <= time_interval) {
			return true;
		}
	} else {
		size_t len1 = command_length - start_point - 1;
		size_t len2 = command_length - len1;
		size_t mem1 = memcmp(command, button_buffer + buffer_length - len1, sizeof(__u8) * len1);
		size_t mem2 = memcmp(command + len1, button_buffer, sizeof(__u8) * len2);
		__u32 time = time_buffer[start_point] - time_buffer[start_point - command_length + buffer_length];
		if (mem1 == 0 && mem2 == 0 && time <= time_interval) {
			return true;
		}
	}
	return false;
}

int main (int argc, char **argv) {
	__u8 conami_command[] = { 10, 10, 11, 11, 8, 9, 8, 9, 2, 3 };
	size_t command_length = sizeof(conami_command) / sizeof(conami_command[0]);
	__u32 command_interval = 500 * command_length;

	int fd;
	int version = 0x000800;
	char name[NAME_LENGTH] = "Unknown";

	if (argc < 2 || !strcmp("--help", argv[1]) || !strcmp("-h", argv[1])) {
		puts("Usage: jschime <device>");
		exit(1);
	}

	if ((fd = open(argv[argc - 1], O_RDONLY)) < 0) {
		perror("jschime");
		exit(1);
	}

	unsigned char axes;
	unsigned char buttons;
	ioctl(fd, JSIOCGVERSION, &version);
	ioctl(fd, JSIOCGAXES, &axes);
	ioctl(fd, JSIOCGBUTTONS, &buttons);
	ioctl(fd, JSIOCGNAME(NAME_LENGTH), name);

	__u8 *axis_keycodes = malloc(sizeof(__u8) * axes * 2);

	for (int i = 0; i < axes * 2; ++i) {
		axis_keycodes[i] = buttons + i;
	}

	printf("Joystick (%s) has %d axes and %d buttons. Driver version is %d.%d.%d.\n",
		name, axes, buttons, version >> 16, (version >> 8) & 0xff, version & 0xff);
	printf("Testing ... (interrupt to exit)\n");

	__u8 button_buffer[RING_BUFFER_LENGTH];
	__u32 time_buffer[RING_BUFFER_LENGTH];
	size_t buffer_index = 0;
	memset(&time_buffer, 0, sizeof(time_buffer));
	while (1) {
		bool pressed = false;
		struct js_event js;
		if (read(fd, &js, sizeof(struct js_event)) != sizeof(struct js_event)) {
			perror("\njschime: error reading");
			exit (1);
		}


		if (js.type == JS_EVENT_BUTTON && js.value > 0) {
			button_buffer[buffer_index] = js.number;
			pressed = true;
		}

		if (js.type == JS_EVENT_AXIS && js.value != 0) {
			size_t i = js.number * 2 + (js.value > 0 ? 1 : 0);
			button_buffer[buffer_index] = axis_keycodes[i];
			pressed = true;
		}

		if (pressed) {
			time_buffer[buffer_index] = js.time;

			bool command_execute = detect_command(
					button_buffer, time_buffer,
					RING_BUFFER_LENGTH, buffer_index,
					conami_command, command_length,
					command_interval);
			if (command_execute) {
				printf("CONAMI\n");
			}


			buffer_index++;
			buffer_index &= (RING_BUFFER_LENGTH - 1);

		}

	}

	free(axis_keycodes);
	return 0;
}
