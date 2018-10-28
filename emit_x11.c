#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

#include "emit.h"

#define INVALID_EVENT_TYPE	-1

static int           motion_type = INVALID_EVENT_TYPE;
static int           button_press_type = INVALID_EVENT_TYPE;
static int           button_release_type = INVALID_EVENT_TYPE;
static int           key_press_type = INVALID_EVENT_TYPE;
static int           key_release_type = INVALID_EVENT_TYPE;
static int           proximity_in_type = INVALID_EVENT_TYPE;
static int           proximity_out_type = INVALID_EVENT_TYPE;


struct ex_data {
    int num;
    Display *display;
    XDeviceInfo *dinfo;
};


static void
print_info(Display *dpy, XDeviceInfo *info)
{
    printf("%s - %ld - %ld\n", info->name, info->id, info->use);
}

// staight stolen from xinput
static int
register_device(Display *dpy, Window window, XDeviceInfo *info) {
    XEventClass		event_list[7];
    int			i;
    XDevice		*device;
    XInputClassInfo *ip;
    int number = 0;

    device = XOpenDevice(dpy, info->id);
    
    if (device == NULL) {
	return 0;
    }

    if (device->num_classes > 0) {
	for (ip = device->classes, i=0; i<info->num_classes; ip++, i++) {
	    switch (ip->input_class) {
	    case KeyClass:
		DeviceKeyPress(device, key_press_type, event_list[number]); number++;
		DeviceKeyRelease(device, key_release_type, event_list[number]); number++;
		break;

	    case ButtonClass:
		DeviceButtonPress(device, button_press_type, event_list[number]); number++;
		DeviceButtonRelease(device, button_release_type, event_list[number]); number++;
		break;

	    case ValuatorClass:
		DeviceMotionNotify(device, motion_type, event_list[number]); number++;
		break;

	    default:
		fprintf(stderr, "unknown class\n");
		break;
	    }
	}

	if (XSelectExtensionEvent(dpy, window, event_list, number)) {
	    fprintf(stderr, "error selecting extended events\n");
	    return 0;
	}
    }
    return number;
}

XDeviceInfo *
register_events(Display *dpy, int *num_dev)
{
    unsigned long screen;
    Window root_win;
    XDevice *device;
    XDeviceInfo *info;
    XDeviceInfo *cinfo;
    int num_devices;
    int i;

    screen = DefaultScreen(dpy);
    root_win = RootWindow(dpy, screen);

    XSelectInput(dpy, root_win, PropertyChangeMask);

    info = XListInputDevices(dpy, &num_devices);
    *num_dev = num_devices;
    for (i=0; i<num_devices; i++) {
	cinfo = info+i;
	print_info(dpy, cinfo);
	if (cinfo->use == 3) { // only register keyboard atm
	    register_device(dpy, root_win, cinfo);
	}
    }

    return(info);
}

int 
get_focus_window(struct emit *emit, Display *dpy, int time) {
    Window w;
    int revert_to;
    XClassHint chint;
    char *name;
    char *class;
    int l;
    char empty = "";

    int class_hint_res;
    int text_property_res;
    int input_focus_res;

    input_focus_res = XGetInputFocus(dpy, &w, &revert_to);
    text_property_res = XFetchName(dpy, w, &name);
    class_hint_res = XGetClassHint(dpy, w, &chint);

    if (class_hint_res == 0) {
	class = empty;
    } else {
	class = chint.res_class;
    }

    if (text_property_res == 0 && class_hint_res == 0) {
	l = snprintf(emit->buffer, sizeof(emit->buffer), "{\"event_type\":\"focus_change\", \"window_name\": \"\", \"window_class\": \"\", \"time\": %i, \"window_id\": -1}\n", time);
    } else {
	l = snprintf(emit->buffer, sizeof(emit->buffer), "{\"event_type\":\"focus_change\", \"window_name\": \"%s\", \"window_class\": \"%s\", \"time\": %i, \"window_id\": %lu}\n", name, class, time, w);
    }

#if DEBUG
    printf("name: %s - %lu\n", name, w);
#endif
    if (class_hint_res != 0) {
	XFree(chint.res_name);
	XFree(chint.res_class);
    }
    XFree(name);


    return l;
}

static void
handle_events(void *ptr)
{
    struct emit *emit;
    struct ex_data *exd;
    XEvent event;
    XDeviceKeyEvent *key;
    XPropertyEvent *pev;
    XDeviceInfo *d;
    Display *dpy;
    char *aname;
    int i, sval;
    int dcount;
    struct sigaction new_actn, old_actn;

    XDeviceInfo *dinfo;

    emit = (struct emit *)ptr;
    exd = (struct ex_data *)emit->ex_data;


    dpy = exd->display;
    dcount = exd->num;
    dinfo = exd->dinfo;


    while (emit->run) {
	XNextEvent(dpy, &event);
	if (event.type == 28) {
	     pev = (XPropertyEvent *)&event;
	     aname = XGetAtomName(dpy, pev->atom);
	     if (pev->atom == 340) {
		 emit->write_length = get_focus_window(emit,dpy, pev->time);
		 if (emit->write_length > 0) {
		     emit->read_state = rs_send;
		 }
	     }
	     XFree(aname);
	}
	if (event.type == 67 || event.type == 68) {
	    key = (XDeviceKeyEvent *)&event;
	    for (i=0; i<dcount; i++){
		d = dinfo + i;
		if (d->id == key->deviceid) {
		    break;
		}
	    }
#if DEBUG
	    printf("key: code(%i) releast(%d) device(%s)\n", key->keycode, event.type == 68 ? 1 : 0, d->name);
#endif
	    emit->write_length = snprintf(emit->buffer, sizeof(emit->buffer), "{\"event_type\":\"key\", \"device_name\": \"%s\", \"device_id\": %i,\"release\":%i,\"code\":%i,\"time\":%i}\n", d->name, key->deviceid, event.type == 68? 1 : 0, key->keycode, key->time);
	    emit->read_state = rs_send;
	}
	if (emit->read_state == rs_send) {
	    pthread_mutex_lock(&emit->socket_lock);
	    for (i=0; i<emit->connections_count; i++) {
		if (emit->connections[i].invalid) {
		    continue;
		}
		new_actn.sa_handler = SIG_IGN;
		sigemptyset (&new_actn.sa_mask);
		new_actn.sa_flags = 0;
		sigaction (SIGPIPE, &new_actn, &old_actn);
		sval = send(emit->connections[i].socket, emit->buffer, emit->write_length, 0);
		sigaction (SIGPIPE, &old_actn, NULL);
		if (sval != emit->write_length) {
		    emit->connections[i].invalid = 1;
		}
	    }
	    pthread_mutex_unlock(&emit->socket_lock);
	    emit->read_state = rs_write;
	}
    }
    printf("ending x11 thread\n");
    return;
}

int
E_X11_init(void **ex_data)
{
    struct ex_data *exd;

    exd = calloc(1, sizeof(struct ex_data));
    if (exd == NULL) {
	printf("could not allocate data pointer\n");
	return 1;
    }
    *ex_data = (void *)exd;

    printf("exd: %p\n", exd);

    exd->display = XOpenDisplay(NULL);

    if (exd->display == NULL)
    {
	printf("could not open X display\n");
	return 1;
    }

    exd->dinfo = register_events(exd->display, &exd->num);
    if (exd->dinfo == 0)
    {
	printf("no keyboards to listen to\n");
	return 1;
    }

    return 0;

}

int E_X11_start(struct emit *emit, void *ex_data)
{
    struct ex_data *exd = (struct ex_data *)ex_data;
    int i;

    emit->ex_data = ex_data;

    
    i = pthread_create(&emit->x11, NULL, handle_events, (void *)emit);
    return 0;
}