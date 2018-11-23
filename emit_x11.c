#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "xcb/xcb.h"

#include "emit.h"

#define INVALID_EVENT_TYPE	-1

static int           motion_type = INVALID_EVENT_TYPE;
static int           button_press_type = INVALID_EVENT_TYPE;
static int           button_release_type = INVALID_EVENT_TYPE;
static int           key_press_type = INVALID_EVENT_TYPE;
static int           key_release_type = INVALID_EVENT_TYPE;


struct ex_data {
    int num;
    Display *display;
    XDeviceInfo *dinfo;
    xcb_connection_t *xcb_connection;
};

int getActiveWindow(struct emit *emit, int time)
{
    xcb_get_input_focus_reply_t* focusReply;
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    xcb_connection_t *c;
    char sanatized_name_in[2048];
    char sanatized_name_out[2048];
    int len;
    char *in, *out;
    xcb_generic_error_t *e;
    struct ex_data *exd;

    exd = (struct ex_data *)emit->ex_data;

    c = exd->xcb_connection;

    focusReply = xcb_get_input_focus_reply(c, xcb_get_input_focus(c), NULL);
    xcb_window_t window = focusReply->focus;

    xcb_atom_t property = XCB_ATOM_WM_NAME;
    xcb_atom_t type = XCB_ATOM_STRING;

    cookie = xcb_get_property(c, 0, window, property, type, 0, 4096);
    if ((reply = xcb_get_property_reply(c, cookie, &e))) {
        len = xcb_get_property_value_length(reply);
        if (len == 0) {
            free(reply);
            return 0;
        }
	printf("%d\n", len);
        snprintf(sanatized_name_in, sizeof(sanatized_name_in), "%.*s\n", len,
               (char*)xcb_get_property_value(reply));
    }
    free(reply);

    in = sanatized_name_in;
    out = sanatized_name_out;
    while (*in) {
	if (*in == '"') {
	    *out++ = '\\';
	}
	if (*in == '\n') {
	    in++;
	    continue;
	}
	*out++ = *in++;
    }
    *out = 0;

    len = snprintf(emit->buffer, sizeof(emit->buffer), "{\"event_type\":\"focus_change\", \"window_name\": \"%s\", \"time\": %i, \"window_id\": %u}\n", sanatized_name_out, time, window);
#ifdef DEBUG
    printf("window focus change %s\n", emit->buffer);
#endif

    return len;
}


static void
print_info(Display *dpy, XDeviceInfo *info)
{
    printf("%s - %ld - %d\n", info->name, info->id, info->use);
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

int get_focus_window(struct emit *emit, Display *dpy, int time) {
    Window w;
    int    status;
    XTextProperty text_prop;
    char **list;
    int    num;
    int revert_to;
    int l;
    char sanatized_name[2048];
    char *in, *out;

    XGetInputFocus(dpy, &w, &revert_to);
    status = XGetWMName(dpy, w, &text_prop);
    if (!status || !text_prop.value || !text_prop.nitems)
    {
	return 0;
    }
    status = XmbTextPropertyToTextList(dpy, &text_prop, &list, &num);
    if (status < Success || !num || !*list)
    {
	return 0;
    }
    XFree(text_prop.value);
    in = *list;
    out = sanatized_name;
    while (*in) {
	if (*in == '"') {
	    *out++ = '\\';
	}
	*out++ = *in++;
    }
    *out = 0;
    l = snprintf(emit->buffer, sizeof(emit->buffer), "{\"event_type\":\"focus_change\", \"window_name\": \"%s\", \"time\": %i, \"window_id\": %lu}\n", sanatized_name, time, w);

#if DEBUG
    printf("active window name: %s\n", *list);
#endif
    XFreeStringList(list);
    return l;
}

void *handle_events(void *ptr)
{
    struct emit *emit;
    struct ex_data *exd;
    XEvent event;
    XDeviceKeyEvent *key;
    XPropertyEvent *pev;
    XDeviceInfo *d, *f;
    Display *dpy;
    char *aname;
    int i, sval;
    int dcount;

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
		 //emit->write_length = get_focus_window(emit,dpy, pev->time);
		 emit->write_length = getActiveWindow(emit, pev->time);
		 if (emit->write_length > 0) {
		     emit->read_state = rs_send;
		 }
	     }
	     XFree(aname);
	}
	if (event.type == 67 || event.type == 68) {
	    key = (XDeviceKeyEvent *)&event;
	    d = NULL;
	    for (i=0; i<dcount; i++){
		f = dinfo + i;
		if (f->id == key->deviceid) {
		    d = f;
		    break;
		}
	    }
	    if (i == dcount || d == NULL) {
		break;
	    }
#if DEBUG
	    printf("key: code(%i) releast(%d) device(%s)\n", key->keycode, event.type == 68 ? 1 : 0, d->name);
#endif
	    emit->write_length = snprintf(emit->buffer, sizeof(emit->buffer), "{\"event_type\":\"key\", \"device_name\": \"%s\", \"device_id\": %lu,\"release\":%i,\"code\":%i,\"time\":%lu}\n", d->name, key->deviceid, event.type == 68? 1 : 0, key->keycode, key->time);
	    emit->read_state = rs_send;
	}
	if (emit->read_state == rs_send) {
	    pthread_mutex_lock(&emit->socket_lock);
	    for (i=0; i<emit->connections_count; i++) {
		if (emit->connections[i].invalid) {
		    continue;
		}
		sval = send(emit->connections[i].socket, emit->buffer, emit->write_length, MSG_DONTWAIT | MSG_NOSIGNAL);
		if (sval != emit->write_length) {
		    printf("connection invalid\n");
		    emit->connections[i].invalid = 1;
		}
	    }
	    pthread_mutex_unlock(&emit->socket_lock);
	    emit->read_state = rs_write;
	}
    }
    return NULL;
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

    exd->xcb_connection = xcb_connect(NULL, NULL);

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
    emit->ex_data = ex_data;
    pthread_create(&emit->x11, NULL, &handle_events, (void *)emit);
    return 0;
}
