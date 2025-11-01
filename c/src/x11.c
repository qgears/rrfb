/* X11 implementation of screen replication to remote listener */


// libx11-dev libxdamage-dev libxext-dev
//  gcc x11.c -lX11 -lXdamage -lXext

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <stdbool.h>
#define QOI_IMPLEMENTATION
#include "qoi.h"
#include <xdo.h>
#include <pthread.h>
#include <json-c/json.h>
#include "translateWebKeycode.h"
#include "executeCommand.h"


#define COMMAND_SIZE 1
#define COMMAND_IMAGEUPDATE 2
#define COMMAND_CLIPBOARDCHANGED 3
#define COMMAND_POINTERUPDATE 4
#define COMMAND_VERSION 5
/// Acknowledge a past query. Used to track timing
#define COMMAND_ACK 6

typedef struct {
	char * display_name;
} args_t;

pthread_mutex_t globalLock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t timestampLock = PTHREAD_MUTEX_INITIALIZER;

static Display *XOpenDisplay_wr(char * display_name);
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

static char defaultDisplayName []= ":0";
void fillargs(args_t * args, int argc, char ** argv)
{
	args->display_name=defaultDisplayName;
	for(int i=1; i<argc; ++i)
	{
		args->display_name=argv[i];
	}
}
#define NBUFFER 2
#define MAX_ACK 128
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
	volatile uint32_t nextFrameIndex;
	volatile uint32_t requestedFrameIndex;
	volatile uint32_t targetmsPeriod;
	uint32_t inactiveCycles;
	uint32_t nAck;
	uint32_t lastMsgReceived[MAX_ACK];
	uint32_t msgTimestamp[MAX_ACK];
	uint32_t msgServerTimestamp[MAX_ACK];
	uint64_t lastMsgReceivedTime[MAX_ACK];
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
int write_u32(int fd, uint32_t value)
{
	return writeAll(fd, (uint8_t *)&value, 4);
}
/**
 * Platform independent write of u64 TODO not platform independent yet
 */
int write_u64(int fd, uint64_t value)
{
	return writeAll(fd, (uint8_t *)&value, 8);
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
static void keyEvent(rrfb_session_t * session, bool down, int keycode)
{
	   /*XKeyEvent xk;
	       // _xdo_init_xkeyevent(xdo, &xk);
	       xk.window = InputFocus;
	       xk.keycode = keycode;
	       xk.state = 0; // mask | (key->group << 13);
	       xk.type = (down ? KeyPress : KeyRelease);
	       XSendEvent(session->xdo->xdpy, xk.window, True, KeyPressMask, (XEvent *)&xk);
	       */
	   XTestFakeKeyEvent(session->xdo->xdpy, keycode, down, 0);
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

		// In case timing of input events becomes jammed then to avoid too fast execution
		// We detect the case and sleep a little.
		// Timing of key+mouse events is not real time but we also do not allow too much jamming
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
		   uint32_t msgIndex=json_object_get_int(json_object_object_get(jobj, "msgIndex"));
		   uint32_t msgTimestamp=json_object_get_int(json_object_object_get(jobj, "msgTimestamp"));
		   uint32_t msgServerTimestamp=json_object_get_int(json_object_object_get(jobj, "msgServerTimestamp"));
		   // rfbLog("msgIndex: %d\n", msgIndex);
		   pthread_mutex_lock(&timestampLock);
		   if(session->nAck<MAX_ACK)
		   {
			   session->lastMsgReceived[session->nAck]=msgIndex;
			   session->msgTimestamp[session->nAck]=msgTimestamp;
			   session->msgServerTimestamp[session->nAck]=msgServerTimestamp;
			   session->lastMsgReceivedTime[session->nAck]=currentTimeMillis();
			   session->nAck++;
		   }
		   pthread_mutex_unlock(&timestampLock);
		   if (strcmp(type, "requestFrame") == 0)
		   {
			   int32_t index=json_object_get_int(json_object_object_get(jobj, "index"));
			   session->requestedFrameIndex = index;
			   int32_t targetmsPeriod=json_object_get_int(json_object_object_get(jobj, "targetmsPeriod"));
			   if(targetmsPeriod<10)
			   {
				   targetmsPeriod=10;
			   }
			   if(targetmsPeriod>5000)
			   {
				   targetmsPeriod=5000;
			   }
			   session->targetmsPeriod = targetmsPeriod;
		   }
		   else if (strcmp(type, "mousemove") == 0)
		   {
			   int x=json_object_get_int(json_object_object_get(jobj, "x"));
			   int y=json_object_get_int(json_object_object_get(jobj, "y"));
			   //rfbLog("Mousemove: %s %d %d\n", type, x, y);
			   xdo_move_mouse(session->xdo, x, y, 0);
		     // do something
		   }
		   else if (strcmp(type, "mousedown") == 0)
		   {
			   int x=json_object_get_int(json_object_object_get(jobj, "x"));
			   int y=json_object_get_int(json_object_object_get(jobj, "y"));
			   int button=json_object_get_int(json_object_object_get(jobj, "button"));
			   //rfbLog("Mousedown: %s %d %d %d\n", type, x, y, button);
			   xdo_move_mouse(session->xdo, x, y, 0);
			   xdo_mouse_down(session->xdo, CURRENTWINDOW, button+1);
		     // do something else
		   }
		   else if (strcmp(type, "mouseup") == 0)
		   {
			   int x=json_object_get_int(json_object_object_get(jobj, "x"));
			   int y=json_object_get_int(json_object_object_get(jobj, "y"));
			   int button=json_object_get_int(json_object_object_get(jobj, "button"));
			   //rfbLog("Mouseup: %s %d %d %d\n", type, x, y, button);
			   xdo_move_mouse(session->xdo, x, y, 0);
			   xdo_mouse_up(session->xdo, CURRENTWINDOW, button+1);
		     // do something else
		   }
		   else if (strcmp(type, "wheel") == 0)
		   {
			   double dx=json_object_get_double(json_object_object_get(jobj, "deltaX"));
			   double dy=json_object_get_double(json_object_object_get(jobj, "deltaY"));
			   //rfbLog("Mouseup: %s %d %d %d\n", type, x, y, button);
			   if(dy<0)
			   {
				   rfbLog("wheel down\n");
				   xdo_mouse_down(session->xdo, CURRENTWINDOW, 4);
				   xdo_mouse_up(session->xdo, CURRENTWINDOW, 4);
			   }
			   if(dy>0)
			   {
				   rfbLog("wheel up\n");
				   xdo_mouse_down(session->xdo, CURRENTWINDOW, 5);
				   xdo_mouse_up(session->xdo, CURRENTWINDOW, 5);
			   }
		     // do something else
		   }
		   else if (strcmp(type, "keyup") == 0)
		   {
			   const char * code=json_object_get_string(json_object_object_get(jobj, "code"));
			   // rfbLog("Keyup: %s\n", code);
			   //int webkeycode=json_object_get_int(json_object_object_get(jobj, "code"));
			   int keycode=translateWebKeyCode(session->xdo->xdpy, code);

			   if(keycode!=-1)
			   {
				   keyEvent(session, false, keycode);
			   }else
			   {
				   rfbLog("Unhandled keyup: %s\n", code);
			   }
			   XSync (session->xdo->xdpy, False);
		     // do something else
		   }
		   else if (strcmp(type, "keydown") == 0)
		   {
			   const char * code=json_object_get_string(json_object_object_get(jobj, "code"));
			   // rfbLog("Keydown: %s\n", code);
			   // int webkeycode=json_object_get_int(json_object_object_get(jobj, "keyCode"));
			   int keycode=translateWebKeyCode(session->xdo->xdpy, code);
			   if(keycode!=-1)
			   {
				   // rfbLog("Keydown: %s %d\n", code, keycode);
				   keyEvent(session, true, keycode);
			   }else
			   {
				   rfbLog("Unhandled keydown: %s\n", code);
			   }
			   XSync (session->xdo->xdpy, False);
		     // do something else
		   }else if(strcmp(type, "clipboard") == 0)
		   {
			   const char * text=json_object_get_string(json_object_object_get(jobj, "text"));
			   if(text!=NULL)
			   {
				   writeClipboard(session->args.display_name, text);
				   rfbLog("clipboard copied from client: %s\n", text);
			   }
		   }
		   /* more else if clauses */
		   else /* default: */
		   {
			   rfbLog("unhandled message type: %s\n", type);
		   }
		}
		json_object_put(jobj);
}

#define INPUTBUFFER_SIZE 1024
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
	XTestGrabControl (session->xdo->xdpy, True);
	uint8_t inputBuffer[INPUTBUFFER_SIZE];
	uint32_t at=0;
	while(!session->exit)
	{
		uint8_t ch;
		errno=0;
		size_t ret=read(session->inputFD, &ch, 1);
		if(ret<=0)
		{
			switch(errno)
			{
			case 0:
				if(ret==0)
				{
					rfbLog("input stream EOF reached\n");
					exit(1);
				}else
				{
					rfbLog("input stream unknown error\n");
					exit(1);
				}
				break;
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
				//rfbLog("command: %s\n", inputBuffer);
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
	XTestGrabControl (session->xdo->xdpy, False);
	// xdo_enter_text_window(session->xdo, CURRENTWINDOW, "Hello", 1000);

	return NULL;
}
Bool PrintSelection(Display *display, Window window, const char *bufname, const char *fmtname)
{
  char *result;
  unsigned long ressize, restail;
  int resbits;
  Atom bufid = XInternAtom(display, bufname, False),
       fmtid = XInternAtom(display, fmtname, False),
       propid = XInternAtom(display, "XSEL_DATA", False),
       incrid = XInternAtom(display, "INCR", False);
  XEvent event;

  XConvertSelection(display, bufid, fmtid, propid, window, CurrentTime);
  do {
    XNextEvent(display, &event);
  } while (event.type != SelectionNotify || event.xselection.selection != bufid);

  if (event.xselection.property)
  {
    XGetWindowProperty(display, window, propid, 0, 1024*1024*5, False, AnyPropertyType,
      &fmtid, &resbits, &ressize, &restail, (unsigned char**)&result);

    if (fmtid == incrid)
      fprintf(stderr,"Buffer is too large and INCR reading is not implemented yet.\n");
    else
      fprintf(stderr, "%.*s", (int)ressize, result);

    XFree(result);
    return True;
  }
  else // request failed, e.g. owner can't convert to the target format
    return False;
}
static void * listenClipboardThread__(void * ptr)
{
	char bufname[]="CLIPBOARD\0";
	rrfb_session_t * session=(rrfb_session_t *)ptr;
	XEvent event;
	int event_base, error_base;
	Display * display=XOpenDisplay_wr(session->args.display_name);
	if(!XFixesQueryExtension(display, &event_base, &error_base))
	{
		rfbLog("error XFixesQueryExtension");
		return NULL;
	}
	Atom bufid = XInternAtom(display, bufname, False);
	 XFixesSelectSelectionInput(display, DefaultRootWindow(display), bufid, XFixesSetSelectionOwnerNotifyMask);

	  while (True)
	  {
	    XNextEvent(display, &event);

	    if (event.type == event_base + XFixesSelectionNotify &&
	        ((XFixesSelectionNotifyEvent*)&event)->selection == bufid)
	    {
	      if (!PrintSelection(display, CURRENTWINDOW, bufname, "UTF8_STRING"))
	        PrintSelection(display, CURRENTWINDOW, bufname, "STRING");

	      fflush(stderr);
	    }
	  }
}
static void * listenClipboardThread(void * ptr)
{
	char * prev=NULL;
	rrfb_session_t * session=(rrfb_session_t *)ptr;
	while(!session->exit)
	{
		char * curr=readClipboard(session->args.display_name);
		if(curr!=NULL)
		{
			if(prev==NULL || strcmp(curr, prev))
			{
				if(prev!=NULL)
				{
					rfbLog("Clipboard content changed: %s -> %s\n", prev, curr);
					free(prev);
					prev=NULL;
				}else
				{
					rfbLog("Clipboard content changed: NULL -> %s\n", curr);
				}
				prev=curr;
				int len=strlen(curr);
				pthread_mutex_lock(&globalLock);
				int err=write_u32(session->outputFD, COMMAND_CLIPBOARDCHANGED);
				err|=write_u32(session->outputFD, len);
				err|=writeAll(session->outputFD, curr, len);
				fsync(session->outputFD);
				pthread_mutex_unlock(&globalLock);
				if(err)
				{
					exit(1);
				}
				// rfbLog("Clipboard content changed: %s\n", curr);
			}
		}
        usleep(250*1000);
	}
	return NULL;
}

void main(int argc, char ** argv)
{
	rrfb_session_t session;
	fillargs(&session.args, argc, argv);
	session.exit=false;
	session.dpy = XOpenDisplay_wr(session.args.display_name);
	session.currentGrabImage=0;
	session.frame=0;
	session.outputFD = 1;
	session.inputFD = 0;
	session.requestedFrameIndex = 0;
	session.nextFrameIndex = 0;
	session.inactiveCycles = 0;
	session.targetmsPeriod = 100;
	session.nAck=0;
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
	int event_base, error_base;
	if(!XFixesQueryExtension(session.dpy, &event_base, &error_base) )
	{
		rfbLog("XFixesQueryExtension error.\n");
		exit(1);
	}
	rfbLog("XFixesQueryExtension OK.\n");


	pthread_t xdoThread;
    int err=pthread_create(& xdoThread,
                       NULL,
                       inputThread,
                       &session);
	if(err!=0)
	{
		rfbLog("can not create thread\n");
		exit(1);
	}

	pthread_t xClipboardThread;
    err=pthread_create(& xClipboardThread,
                       NULL,
                       listenClipboardThread,
                       &session);
	if(err!=0)
	{
		rfbLog("can not create listenClipboardThread\n");
		exit(1);
	}

	Visual * visual = DefaultVisual(session.dpy,0);
	int width=XWidthOfScreen(ScreenOfDisplay(session.dpy, 0));
	int height=XHeightOfScreen(ScreenOfDisplay(session.dpy, 0));
	rfbLog("Wxh: %dx%d\n", width, height);
	rfbLog("Visual info: %d %b %b %b\n", visual->bits_per_rgb, (unsigned int)visual->green_mask, (unsigned int)visual->red_mask, (unsigned int)visual->blue_mask);
	for(int i=0;i<NBUFFER;++i)
	{
		XShmSegmentInfo *shminfo=&session.shminfo[i];
		XImage * image = session.image[i] = XShmCreateImage(session.dpy,
		      visual, // Use a correct visual. Omitted for brevity
		      24,   // Determine correct depth from the visual. Omitted for brevity
		      ZPixmap, NULL, shminfo, width, height);
		  rfbLog("Image bytes per line: %d bits per pixel: %d\n", image->bytes_per_line, image->bits_per_pixel);
		  uint32_t size=alignSize(image->bytes_per_line * image->height);
		  shminfo->shmid = shmget(IPC_PRIVATE,
		      size,
		      IPC_CREAT|0777);

		  shminfo->shmaddr = image->data = shmat(shminfo->shmid, 0, 0);
		  shminfo->readOnly = False;
		  XShmAttach(session.dpy, &session.shminfo[i]);
		  memset(image->data, 0, size);
	}

	{
		pthread_mutex_lock(&globalLock);
		int err=write_u32(session.outputFD, COMMAND_VERSION);
		  err|=write_u32(session.outputFD, 16);
		  err|=writeAll(session.outputFD, "RRFB 0.4.0      ", 16);

		  err|=write_u32(session.outputFD, COMMAND_SIZE);
		  err|=write_u32(session.outputFD, 8);
		  err|=write_u32(session.outputFD, width);
		  err|=write_u32(session.outputFD, height);

		pthread_mutex_unlock(&globalLock);
		if(err)
		{
			exit(1);
		}
	}
	while(1)
	{
		if(session.requestedFrameIndex>=session.nextFrameIndex)
		{
			session.nextFrameIndex++;
			session.inactiveCycles =0;
		XImage * image=session.image[session.currentGrabImage];
	  Bool ret=XShmGetImage(session.dpy,
	      RootWindow(session.dpy,0),
	      image,
	      0,
	      0,
	      AllPlanes);
	  if(!ret)
	  {
		  rfbLog("XShmGetImage returned 0\n");
		  exit(1);
	  }
	  // rfbLog("Ret: %d\n", ret);

	  /* typedef struct {
	  	unsigned int width;
	  	unsigned int height;
	  	unsigned char channels;
	  	unsigned char colorspace; */
	  qoi_desc qoi;
	  qoi.width=image->width;
	  qoi.height=image->height;
	  qoi.channels=4;
	  qoi.colorspace=1;
	  // qoi_write("img.qoi", image->data, &qoi);
	  int out_len;
	  XImage * prev=session.image[(session.currentGrabImage+NBUFFER-1)%NBUFFER];
	  createDiff(prev, image);
	  void * compressed=qoi_encode(prev->data, &qoi, &out_len);
	  if(compressed!=NULL)
	  {
		  int err=0;
		pthread_mutex_lock(&globalLock);
		pthread_mutex_lock(&timestampLock);
		for(uint32_t i=0;i<session.nAck; ++i)
		{
			  err|=write_u32(session.outputFD, COMMAND_ACK);
			  err|=write_u32(session.outputFD, 24);
			  err|=write_u32(session.outputFD, session.lastMsgReceived[i]);
			  err|=write_u32(session.outputFD, (uint32_t)session.msgTimestamp[i]);
			  err|=write_u32(session.outputFD, (uint32_t)session.msgServerTimestamp[i]);
			  err|=write_u32(session.outputFD, (uint32_t)session.lastMsgReceivedTime[i]);
			  err|=write_u32(session.outputFD, (uint32_t)currentTimeMillis());
			  err|=write_u32(session.outputFD, (uint32_t)0); // Server timestamp on the way back to the client - overwritten by the web server component
		}
		session.nAck=0;
		pthread_mutex_unlock(&timestampLock);
		  err|=write_u32(session.outputFD, COMMAND_IMAGEUPDATE);

		  err|=write_u32(session.outputFD, out_len+16);
		  err|=write_u32(session.outputFD, (uint32_t)currentTimeMillis());
		  err|=write_u32(session.outputFD, (uint32_t)0); // Server timestamp
		  err|=writeAll(session.outputFD, compressed, out_len);
		  err|=write_u32(session.outputFD, (uint32_t)currentTimeMillis());
		  err|=write_u32(session.outputFD, (uint32_t)0); // Server timestamp
		  fsync(session.outputFD);
		  free(compressed);
		pthread_mutex_unlock(&globalLock);
		if(err)
		{
			rfbLog("output pipe broken: exit\n");
			exit(1);
		}
	  }else
	  {
			rfbLog("qoi_encode returns error\n");
			exit(1);
	  }
	  {
	   // Get the root window
           Window root_window = DefaultRootWindow(session.dpy);
           Window returned_root, returned_child;
           int root_x, root_y;       // Screen coordinates
           int win_x, win_y;         // Coordinates relative to window
           unsigned int mask;
           // Query pointer position
           if (XQueryPointer(session.dpy, root_window, &returned_root, &returned_child,
                      &root_x, &root_y, &win_x, &win_y, &mask)) {
                      		  int err=0;
		pthread_mutex_lock(&globalLock);
		  err|=write_u32(session.outputFD, COMMAND_POINTERUPDATE);
		  err|=write_u32(session.outputFD, 12);
		  err|=write_u32(session.outputFD, root_x);
		  err|=write_u32(session.outputFD, root_y);
		  err|=write_u32(session.outputFD, mask);
		  fsync(session.outputFD);
		pthread_mutex_unlock(&globalLock);
		if(err)
		{
			rfbLog("output pipe broken: exit\n");
			exit(1);
		}
           } else {
            // fprintf(stderr, "Unable to query pointer location.\n");
           }
          }
	  // rfbLog("Image size: %d\n", out_len);
	  // void *qoi_encode(const void *data, const qoi_desc *desc, int *out_len);
	          session.currentGrabImage++;
	          session.currentGrabImage%=NBUFFER;
	          session.frame++;
	          // rfbLog("Frame grabbed: %d", session.frame);


	         /* if(session.frame>1)
	          {
	        	  break;
	          }*/
		} else
		{
			session.inactiveCycles ++;
			if(session.inactiveCycles>50)
			{
				fprintf(stderr, "client stalled: does not send request frame for 50 cycles\n");
				session.inactiveCycles = 0;
			}
		}
        // Let logs always be readable
        fflush(stderr);
        // Wait a little before the next frame - 100ms sleep means less than 10FPS altogether
        usleep(session.targetmsPeriod*1000);
	}
	session.exit=true;
	exit(0);
}

