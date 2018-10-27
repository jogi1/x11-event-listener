#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdio.h>

#define INVALID_EVENT_TYPE	-1

static int           motion_type = INVALID_EVENT_TYPE;
static int           button_press_type = INVALID_EVENT_TYPE;
static int           button_release_type = INVALID_EVENT_TYPE;
static int           key_press_type = INVALID_EVENT_TYPE;
static int           key_release_type = INVALID_EVENT_TYPE;
static int           proximity_in_type = INVALID_EVENT_TYPE;
static int           proximity_out_type = INVALID_EVENT_TYPE;

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
    printf("%p\n", root_win);

    XSelectInput(dpy, root_win, PropertyChangeMask);

    info = XListInputDevices(dpy, &num_devices);
    printf("%d\n", num_devices);
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

static void
get_focus_window(Display *dpy, FILE *f, int time) {
    Window w;
    int revert_to;
    char **list_return;
    int count_return;
    XClassHint chint;
    XTextProperty prop;
    char *empty = " ";
    char *name;
    char *class;

    int class_hint_res;
    int text_property_res;
    int input_focus_res;

    input_focus_res = XGetInputFocus(dpy, &w, &revert_to);
    XGetWMName(dpy,w,&prop);
    text_property_res = XTextPropertyToStringList(&prop, &list_return, &count_return);
    class_hint_res = XGetClassHint(dpy, w, &chint);
    printf("-------------\n");
    printf("input_focus_res: %d\n", input_focus_res);
    printf("text_property_res: %d\n", text_property_res);
    printf("class_hint_res: %d\n", class_hint_res);

    if (text_property_res == 1) {
	name = list_return[0];
    } else {
	name = empty;
    }


    if (class_hint_res == 0) {
	class = empty;
    } else {
	class = chint.res_class;
    }

    if (text_property_res == 0 && class_hint_res == 0) {
	fprintf(f, "{\"event_type\":\"focus_change\", \"window_name\": \"\", \"window_class\": \"\", \"time\": %i, \"window_id\": -1}\n", time);
    } else {
	fprintf(f, "{\"event_type\":\"focus_change\", \"window_name\": \"%s\", \"window_class\": \"%s\", \"time\": %i, \"window_id\": %lu}\n", name, class, time, w);
    }

    printf("name: %s - %lu\n", name, w);
    if (class_hint_res != 0) {
	XFree(chint.res_name);
	XFree(chint.res_class);
    }
    printf("-------------\n");


}

static void
handle_events(Display *dpy, XDeviceInfo *dinfo, int dcount)
{
    XEvent event;
    XDeviceKeyEvent *key;
    XPropertyEvent *pev;
    XDeviceInfo *d;
    char *aname;
    int i;
    FILE *f;

    f = fopen("/tmp/key_events", "w");

    if (f == NULL) {
	return ;
    }

    while (1) {
	XNextEvent(dpy, &event);
	if (event.type == 28) {
	     pev = (XPropertyEvent *)&event;
	     aname = XGetAtomName(dpy, pev->atom);
	     if (pev->atom == 340) {
		 printf("aname: %s - %i - %i\n", aname, pev->window, pev->atom);
		 get_focus_window(dpy, f, pev->time);
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
	    fprintf(f, "{\"event_type\":\"key\", \"device_name\": \"%s\", \"device_id\": %i,\"release\":%i,\"code\":%i,\"time\":%i}\n", d->name, key->deviceid, event.type == 68? 1 : 0, key->keycode, key->time);
	    fflush(f);
	}
    }
    return;
}

int
main()
{
    Display *display;
    XDeviceInfo *dinfo;
    int num;
    int s;

    display = XOpenDisplay(NULL);

    if (display == NULL)
    {
	printf("could to open display\n");
	return 1;
    }
    dinfo = register_events(display, &num);
    printf("%d\n", num);

    handle_events(display, dinfo, num);
}
