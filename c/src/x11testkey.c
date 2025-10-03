/* X11 implementation of screen replication to remote listener */


// libx11-dev libxdamage-dev libxext-dev
//  gcc x11.c -lX11 -lXdamage -lXext

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <stdbool.h>
#include <xdo.h>
#include <pthread.h>
#include <json-c/json.h>


#define COMMAND_SIZE 1
#define COMMAND_IMAGEUPDATE 2

typedef struct {
	char * display_name;
} args_t;

static void fillArgs(args_t * args);
static Display *XOpenDisplay_wr(char * display_name);
/// Create correct xauthority file
static int xauth_raw(int on);
static void set_env(char *name, char *value) {
	char *str;
	if (! name) {
		return;
	}
	if (! value) {
		value = "";
	}

	setenv(name, value, 1);
}
#define rfbLog(args...) fprintf(stderr, args)

Display *XOpenDisplay_wr(char * display_name) {
	Display *d = NULL;
	int db = 0;

	if (! xauth_raw(1)) {
		return NULL;
	}
	d = XOpenDisplay(display_name);
	if (db) fprintf(stderr, "XOpenDisplay_wr: %s  %p\n", display_name, (void *)d);

	if (d == NULL && !getenv("NO_XAUTHLOCALHOSTNAME")) {
		char *xalhn = getenv("XAUTHLOCALHOSTNAME");
		if (1 || !xalhn) {
			rfbLog("XOpenDisplay(\"%s\") failed.\n",
			    display_name ? display_name : "");
			rfbLog("Trying again with XAUTHLOCALHOSTNAME=localhost ...\n");
			set_env("XAUTHLOCALHOSTNAME", "localhost");
			d = XOpenDisplay(display_name);
			if (d == NULL && xalhn) {
				char *ptr = getenv("XAUTHLOCALHOSTNAME");
				if (ptr) {
					*(ptr-2) = '_';	/* yow */
					rfbLog("XOpenDisplay(\"%s\") failed.\n",
					    display_name ? display_name : "");
					rfbLog("Trying again with unset XAUTHLOCALHOSTNAME ...\n");
					d = XOpenDisplay(display_name);
				}
			}
		}
	}

	xauth_raw(0);

	return d;
}

int XCloseDisplay_wr(Display *display) {
	int db = 0;
	if (db) fprintf(stderr, "XCloseDisplay_wr: %p\n", (void *)display);
#if NO_X11
	return 0;
#else
	return XCloseDisplay(display);
#endif	/* NO_X11 */
}

static int xauth_raw(int on) {
	char tmp[] = "/tmp/x11vnc-xauth.XXXXXX";
	int tmp_fd = -1;
	static char *old_xauthority = NULL;
	static char *old_tmp = NULL;
	int db = 0;

	if (on) {
		if (old_xauthority) {
			free(old_xauthority);
			old_xauthority = NULL;
		}
		if (old_tmp) {
			free(old_tmp);
			old_tmp = NULL;
		}
/*		if (xauth_raw_data) {
			tmp_fd = mkstemp(tmp);
			if (tmp_fd < 0) {
				rfbLog("could not create tmp xauth file: %s\n", tmp);	
				return 0;
			}
			if (db) fprintf(stderr, "XAUTHORITY tmp: %s\n", tmp);
			write(tmp_fd, xauth_raw_data, xauth_raw_len);
			close(tmp_fd);
			if (getenv("XAUTHORITY")) {
				old_xauthority = strdup(getenv("XAUTHORITY"));
			} else {
				old_xauthority = strdup("");
			}
			set_env("XAUTHORITY", tmp);
			old_tmp = strdup(tmp);
		}*/
		return 1;
	} else {
		if (old_xauthority) {
			if (!strcmp(old_xauthority, "")) {
				char *xauth = getenv("XAUTHORITY");
				if (xauth) {
					*(xauth-2) = '_';	/* yow */
				}
			} else {
				set_env("XAUTHORITY", old_xauthority);
			}
			free(old_xauthority);
			old_xauthority = NULL;
		}
		if (old_tmp) {
			unlink(old_tmp);
			free(old_tmp);
			old_tmp = NULL;
		}
		return 1;
	}
}


void fillargs(args_t * args)
{
	args->display_name=":0";
}
#define NBUFFER 2
typedef struct {
	volatile bool exit;
	args_t args;
	int s;
	int currentGrabImage;
	int frame;
	Display * dpy;
	Display * gdpy_ctrl;
	XImage * image[NBUFFER];
	XShmSegmentInfo shminfo[NBUFFER];
	int outputFD;
	int inputFD;
	xdo_t* xdo;
}rrfb_session_t;

typedef char v16qi __attribute__ ((vector_size (16)));

void createDiff(XImage * prev, XImage * current)
{
	int size1=prev->height*prev->bytes_per_line;
	int size2=current->height*current->bytes_per_line;
	if(size1==size2)
	{
		/*
		for(int i=0;i<size1;++i)
		{
			uint8_t p=prev->data[i];
			uint8_t c=current->data[i];
			uint8_t value=c-p;
			prev->data[i]=value;
		}
		*/

		v16qi * a=(v16qi *)prev->data;
		v16qi * b=(v16qi *)current->data;
		int n=size1/16;
		for(int i=0;i<n;++i)
		{
			* a = __builtin_ia32_psubb128 (*a, *b);
			a++;
			b++;
		}

	}else
	{
		rfbLog("createDiff Size mismatch %d %d\n", size1, size2);
	}
}
/// MAke size a little bigger so that SIMD never overflow
int alignSize(int reqsize)
{
	return ((reqsize+15)/16)*16;
}
/**
 * Write all to FD - do retry in case if EAGAIN
 */
int writeAll(int fd, uint8_t * ptr, size_t size)
{
	size_t at=0;
	while(at<size)
	{
		int ret=write(fd, ptr+at, size);
		if(ret<0)
		{
			switch(errno)
			{
			case EAGAIN:
			//case EWOULDBLOCK:
				// Just retry
				break;
			default:
				// Failed
				return 1;
			}
		}else
		{
			at+=ret;
		}
	}
	return 0;
}

/**
 * Platform independent write of u32 TODO not platform independent yet
 */
void write_u32(int fd, uint32_t value)
{
	writeAll(fd, (uint8_t *)&value, 4);
}
#include <sys/time.h>
/**
* @brief provide same output with the native function in java called
* currentTimeMillis().
*/
int64_t currentTimeMillis() {
  struct timeval time;
  gettimeofday(&time, NULL);
  int64_t s1 = (int64_t)(time.tv_sec) * 1000;
  int64_t s2 = (time.tv_usec / 1000);
  return s1 + s2;
}
int64_t prevT_remote=0;
int64_t prevT_local=0;
static void processCommand(rrfb_session_t * session, const char * command)
{
	struct json_object *jobj;


		//printf("str:\n---\n%s\n---\n\n", str);

		jobj = json_tokener_parse(command);
		// rfbLog("jobj from str:\n---\n%s\n---\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
		json_object *type_json = json_object_object_get(jobj, "type");
		int64_t tremote=json_object_get_int64(json_object_object_get(jobj, "t"));
		int64_t tdiffremote=tremote-prevT_remote;
		if(tdiffremote<0) tdiffremote=0;
		if(tdiffremote>10000) tdiffremote=0;
		int64_t tlocal=currentTimeMillis();
		int64_t tdifflocal=tlocal-prevT_local;
		if(tdifflocal<0) tdifflocal=0;
		if(tdiffremote>tdifflocal*2)
		{
			uint64_t remainingMillis=tdiffremote-tdifflocal;
			usleep(remainingMillis*1000);
		}
		tlocal=currentTimeMillis();
		prevT_local=tlocal;
		prevT_remote=tremote;
		if(type_json!=NULL)
		{
		   const char * type=json_object_get_string(type_json);
		   if (strcmp(type, "mousemove") == 0)
		   {
			   int x=json_object_get_int(json_object_object_get(jobj, "x"));
			   int y=json_object_get_int(json_object_object_get(jobj, "y"));
			   rfbLog("Mousemove: %s %d %d\n", type, x, y);
			   xdo_move_mouse(session->xdo, x, y, 0);
		     // do something
		   }
		   else if (strcmp(type, "mousedown") == 0)
		   {
			   int x=json_object_get_int(json_object_object_get(jobj, "x"));
			   int y=json_object_get_int(json_object_object_get(jobj, "y"));
			   int button=json_object_get_int(json_object_object_get(jobj, "button"));
			   rfbLog("Mousedown: %s %d %d %d\n", type, x, y, button);
			   xdo_move_mouse(session->xdo, x, y, 0);
			   xdo_mouse_down(session->xdo, CURRENTWINDOW, button+1);
		     // do something else
		   }
		   else if (strcmp(type, "mouseup") == 0)
		   {
			   int x=json_object_get_int(json_object_object_get(jobj, "x"));
			   int y=json_object_get_int(json_object_object_get(jobj, "y"));
			   int button=json_object_get_int(json_object_object_get(jobj, "button"));
			   rfbLog("Mouseup: %s %d %d %d\n", type, x, y, button);
			   xdo_move_mouse(session->xdo, x, y, 0);
			   xdo_mouse_up(session->xdo, CURRENTWINDOW, button+1);
		     // do something else
		   }
		   else if (strcmp(type, "keyup") == 0)
		   {
			   const char * code=json_object_get_string(json_object_object_get(jobj, "code"));
			   rfbLog("Keyup: %s\n", code);
			   int keycode=json_object_get_int(json_object_object_get(jobj, "keyCode"));
			   XTestFakeKeyEvent(session->xdo->xdpy, keycode, false, 0);
		     // do something else
		   }
		   else if (strcmp(type, "keydown") == 0)
		   {
			   const char * code=json_object_get_string(json_object_object_get(jobj, "code"));
			   int keycode=json_object_get_int(json_object_object_get(jobj, "keyCode"));
			   rfbLog("Keydown: %s\n", code);
			   XTestFakeKeyEvent(session->xdo->xdpy, keycode, true, 0);
		     // do something else
		   }
		   /* more else if clauses */
		   else /* default: */
		   {
			   rfbLog("unhandled message type: %s\n", type);
		   }
		}
		json_object_put(jobj);
}

#include "translateWebKeycode.h"
#define INPUTBUFFER_SIZE 1024
static void pressKey(rrfb_session_t * session, const char * code)
{
	XTestGrabControl (session->xdo->xdpy, True);
	int keycode=translateWebKeyCode(session->xdo->xdpy, code);
	if(keycode==-1)
	{
		printf("no keycode: %s\n", code);
		return;
	}
	// printf("sym: %d keycode: %x %lld\n", sym, keycode, (long long int)(session->xdo->xdpy));
	usleep(100000);
	XTestFakeKeyEvent(session->xdo->xdpy, keycode, true, 10);
	usleep(200000);
	XTestFakeKeyEvent(session->xdo->xdpy, keycode, false, 10);
	usleep(100000);
	XSync (session->xdo->xdpy, False);
	XTestGrabControl (session->xdo->xdpy, False);
}
static void * inputThread(void * ptr)
{
	rrfb_session_t * session=(rrfb_session_t *)ptr;
	session->xdo=xdo_new(session->args.display_name);
	if(session->xdo==NULL)
	{
		rfbLog("xdo session can't be opened\n");
		exit(1);
	}
	rfbLog("xdo session opened\n");
	uint8_t inputBuffer[INPUTBUFFER_SIZE];
	uint32_t at=0;
	/*
	while(!session->exit)
	{
		uint8_t ch;
		size_t ret=read(session->inputFD, &ch, 1);
		if(ret<0)
		{
			switch(errno)
			{
			case EAGAIN:
				break;
			default:
				rfbLog("Error reading input stream\n");
				exit(1);
				break;
			}
		}else if(ret==1)
		{
			switch(ch)
			{
			case '\r': // ignore
				break;
			case '\n': // execute command
				inputBuffer[at]=0;
				processCommand(session, inputBuffer);
				rfbLog("command: %s\n", inputBuffer);
				at=0;
				break;
			default:
				// rfbLog("read byte: %c\n", ch);
				if(at<sizeof(inputBuffer)-1)
				{
					inputBuffer[at]=ch;
					at++;
				}
				break;
		}
		}
	}
	*/
	// xdo_enter_text_window(session->xdo, CURRENTWINDOW, "Hello", 1000);
	  // keycode=65;
	  pressKey(session, "KeyA");
	  pressKey(session, "KeyL");
	  pressKey(session, "KeyM");
	  pressKey(session, "KeyA");
	  pressKey(session, "KeyW");
	  pressKey(session, "Digit5");
	  pressKey(session, "Enter");
	return NULL;
}
void main(void)
{
	rrfb_session_t session;
	fillargs(&session.args);
	session.exit=false;
	session.dpy = XOpenDisplay_wr(session.args.display_name);
	session.currentGrabImage=0;
	session.frame=0;
	session.outputFD = 1;
	session.inputFD = 0;
	if(session.dpy==NULL)
	{
		rfbLog("XOpenDisplay_wr error\n");
		exit(1);
	}
	session.gdpy_ctrl = XOpenDisplay_wr(DisplayString(session.dpy));
	if(session.gdpy_ctrl==NULL)
	{
		rfbLog("gdpy_ctrl = XOpenDisplay_wr error\n");
		exit(1);
	}
	fprintf(stderr, "X11 display opened: %"PRIu64"\n", (uint64_t)session.gdpy_ctrl);
/*	if (!gdpy_ctrl) {
		fprintf(stderr, "gdpy_ctrl open failed\n");
	}
	XSync(dpy, True);
	XSync(gdpy_ctrl, True);
	gdpy_data = XOpenDisplay_wr(DisplayString(dpy));
	if (!gdpy_data) {
		fprintf(stderr, "gdpy_data open failed\n");
	}
	if (gdpy_ctrl && gdpy_data) {
		disable_grabserver(gdpy_ctrl, 0);
		disable_grabserver(gdpy_data, 0);
		xrecord_grabserver(1);
	}
	*/
	int xdamage_base_event_type;
	int xdamage_present;
	int er;
	if (! XDamageQueryExtension(session.dpy, &xdamage_base_event_type, &er)) {
		rfbLog("Disabling X DAMAGE mode: display does not support it.\n");
		exit(1);
		xdamage_base_event_type = 0;
		xdamage_present = 0;
	} else {
		xdamage_present = 1;
		rfbLog("X DAMAGE present OK.\n");
	}
	if (! XShmQueryExtension((session.dpy))) {
		rfbLog("XShmQueryExtension error.\n");
		exit(1);
	}else
	{
		rfbLog("XShmQueryExtension ok.\n");
	}
	int a, b, c, d;
	if (!XTestQueryExtension(session.dpy, &a, &b, &c, &d)) {
		rfbLog("XTestQueryExtension error.\n");
		exit(1);
	}else
	{
		rfbLog("XTestQueryExtension ok.\n");
	}


	pthread_t xdoThread;
	inputThread(&session);
}

