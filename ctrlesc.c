#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#define KU 0
#define KD 1
#define KH 2

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define HANDLE_EXIT -1
#define HANDLE_NOOP 0
#define HANDLE_FORWARD 1

#undef DEBUG

struct mod_state {
	int lctrl, rctrl;
};

static inline int ctrl_pressed(struct mod_state *mod)
{
#ifdef DEBUG
	printf("ctrl_pressed=%d, %d != %d, %d != %d\n",
	       mod->lctrl != KU || mod->rctrl != KU,
	       mod->lctrl, KU,
	       mod->rctrl, KU);
#endif
	return mod->lctrl != KU || mod->rctrl != KU;
}

struct key_event {
	unsigned int	code;
	int		value;
};

static void print_event(const char *prefix, struct input_event *ev)
{
#ifdef DEBUG
	/*
	 * !!!           UNCOMMENT ONLY FOR DEBUGGING          !!!
	 * !!! OTHERWISE ALL KEY PRESSES APPEAR IN SYSTEMD LOG !!!
	 */

	printf("%s event: %s %s %d\n",
	       prefix,
	       libevdev_event_type_get_name(ev->type),
	       libevdev_event_code_get_name(ev->type, ev->code),
	       ev->value);
#endif
}

static void send_keys(struct libevdev_uinput *uidev,
		      struct key_event *ke,
		      size_t n)
{
	int err;
	for (size_t i = 0; i < n; i++) {
		err = libevdev_uinput_write_event(uidev, EV_KEY, ke[i].code, ke[i].value);
		if (err != 0) {
			fprintf(stderr, "Failed to write %s %d event (%s)\n",
				libevdev_event_code_get_name(EV_KEY, ke[i].code),
				ke[i].value,
				strerror(-err));
			return;
		}
	}
	err = libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
	if (err != 0) {
		fprintf(stderr, "Failed to write EV_SYN SYN_REPORT event (%s)\n", strerror(-err));
	}
}

static void forward(struct libevdev_uinput *uidev,
		    struct input_event *ev)
{
	struct key_event cp[] = {
		{ ev->code, ev->value },
	};
	send_keys(uidev, cp, ARRAY_SIZE(cp));
	print_event("forward", ev);
}

static int handle_key(struct libevdev_uinput *uidev,
		      struct input_event *ev,
		      struct input_event *prev,
		      struct mod_state *mod)
{
	switch (ev->code) {
	case KEY_F8:
		return HANDLE_EXIT;

	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		if (prev->code == ev->code &&
		    ev->value == KU &&
		    prev->value == KD) {
			struct key_event ke[] = {
				{ KEY_ESC, KD },
				{ KEY_ESC, KU },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
			return HANDLE_NOOP;
		}
		if (ev->value == KU) {
			struct key_event ke[] = {
				{ KEY_LEFTCTRL, KU },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
		}
		return HANDLE_NOOP;
		break;

	case KEY_LEFT:
		if (ctrl_pressed(mod)) {
			struct key_event ke[] = {
				{ KEY_HOME, KD },
				{ KEY_HOME, KU },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
			return HANDLE_NOOP;
		}
		break;
	case KEY_RIGHT:
		if (ctrl_pressed(mod)) {
			struct key_event ke[] = {
				{ KEY_END, KD },
				{ KEY_END, KU },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
			return HANDLE_NOOP;
		}
		break;
	case KEY_UP:
		if (ctrl_pressed(mod)) {
			struct key_event ke[] = {
				{ KEY_PAGEUP, KD },
				{ KEY_PAGEUP, KU },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
			return HANDLE_NOOP;
		}
		break;
	case KEY_DOWN:
		if (ctrl_pressed(mod)) {
			struct key_event ke[] = {
				{ KEY_PAGEDOWN, KD },
				{ KEY_PAGEDOWN, KU },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
			return HANDLE_NOOP;
		}
		break;

	default:
		if (ctrl_pressed(mod)) {
			struct key_event ke[] = {
				{ KEY_LEFTCTRL, KD },
				{ KEY_LEFTCTRL, KH },
			};
			send_keys(uidev, ke, ARRAY_SIZE(ke));
		}
	}

	return HANDLE_FORWARD;
}

static int handle_events(struct libevdev *dev, struct libevdev_uinput *uidev,
			 struct mod_state *mod, struct input_event *prev)
{

	for (;;) {
		struct input_event ev;
		int err;

		err = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		switch (err) {
		case LIBEVDEV_READ_STATUS_SUCCESS:
			break;
		case LIBEVDEV_READ_STATUS_SYNC:
			perror("can't keep up");
			return -1;
		case -EAGAIN:
			return 0;
		default:
			perror("unexpected error");
			return -1;
		}

		if (!libevdev_event_is_type(&ev, EV_KEY))
			continue;

		print_event("receive", &ev);

		switch (handle_key(uidev, &ev, prev, mod)) {
		case HANDLE_EXIT:
			return 0;
		case HANDLE_FORWARD:
			forward(uidev, &ev);
			break;
		}

		switch (ev.code) {
		case KEY_LEFTCTRL:
			mod->lctrl = ev.value;
			break;
		case KEY_RIGHTCTRL:
			mod->rctrl = ev.value;
			break;
		}

		*prev = ev;
	}
}

int main(int argc, char *argv[])
{
	struct libevdev_uinput *uidev;
	struct input_event prev = {};
	struct mod_state mod = {};
	struct libevdev *dev;
	struct pollfd pfd[1];
	int err;
	int fd;

	if (argc <= 1) {
		fprintf(stderr, "Please specify device name!\n");
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDONLY|O_NONBLOCK);
	err = libevdev_new_from_fd(fd, &dev);
	if (err < 0) {
		fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-err));
		return EXIT_FAILURE;
	}
	printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
	printf("Input device ID: bus %#x vendor %#x product %#x\n",
	       libevdev_get_id_bustype(dev),
	       libevdev_get_id_vendor(dev),
	       libevdev_get_id_product(dev));
	if (!libevdev_has_event_code(dev, EV_KEY, KEY_LEFTCTRL)) {
		printf("This device does not look like a keyboard!\n");
		return EXIT_FAILURE;
	}

	fd = open("/dev/uinput", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed to open uinput (%s)\n", strerror(errno));
		return EXIT_FAILURE;
	}

	err = libevdev_uinput_create_from_device(dev, fd, &uidev);
	if (err < 0) {
		fprintf(stderr, "Failed to create uinput (%s)\n", strerror(-err));
		return EXIT_FAILURE;
	}

	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
		perror("mlockall");
		return EXIT_FAILURE;
	}

	err = libevdev_grab(dev, LIBEVDEV_GRAB);
	if (err < 0) {
		fprintf(stderr, "Failed to grab device (%s)\n", strerror(-err));
		return EXIT_FAILURE;
	}

	printf("Ready\n");

	pfd[0].fd = libevdev_get_fd(dev);
	pfd[0].events = POLLIN;

	while (poll(pfd, ARRAY_SIZE(pfd), -1))
		if (handle_events(dev, uidev, &mod, &prev))
			break;

	err = libevdev_grab(dev, LIBEVDEV_UNGRAB);
	if (err < 0) {
		fprintf(stderr, "Failed to grab device (%s)\n", strerror(-err));
		return EXIT_FAILURE;
	}

	libevdev_uinput_destroy(uidev);
	libevdev_free(dev);
	return EXIT_SUCCESS;
}
