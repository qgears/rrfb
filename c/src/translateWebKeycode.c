// Translate standard web keycode to X11 keycode
#include "translateWebKeycode.h"
#include <stdio.h>

typedef struct {
	const char * code;
	int sym;
	int linux_keycode;
}entry_t;
// Linux Keycode source: https://manpages.ubuntu.com/manpages/focal/en/man7/virkeycode-linux.7.html
static entry_t entries[]={
		// generated
		{"KeyA", XK_A, 30},
		{"KeyB", XK_B, 48},
		{"KeyC", XK_C, 46},
		{"KeyD", XK_D, 32},
		{"KeyE", XK_E, 18},
		{"KeyF", XK_F, 33},
		{"KeyG", XK_G, 34},
		{"KeyH", XK_H, 35},
		{"KeyI", XK_I, 23},
		{"KeyJ", XK_J, 36},
		{"KeyK", XK_K, 37},
		{"KeyL", XK_L, 38},
		{"KeyM", XK_M, 50},
		{"KeyN", XK_N, 49},
		{"KeyO", XK_O, 24},
		{"KeyP", XK_P, 25},
		{"KeyQ", XK_Q, 16},
		{"KeyR", XK_R, 19},
		{"KeyS", XK_S, 31},
		{"KeyT", XK_T, 20},
		{"KeyU", XK_U, 22},
		{"KeyV", XK_V, 47},
		{"KeyW", XK_W, 17},
		{"KeyX", XK_X, 45},
		{"KeyY", XK_Y, 21},
		{"KeyZ", XK_Z, 44},
		{"Digit0", XK_0, 11},
		{"Digit1", XK_1, 2},
		{"Digit2", XK_2, 3},
		{"Digit3", XK_3, 4},
		{"Digit4", XK_4, 5},
		{"Digit5", XK_5, 6},
		{"Digit6", XK_6, 7},
		{"Digit7", XK_7, 8},
		{"Digit8", XK_8, 9},
		{"Digit9", XK_9, 10},

		{"Semicolon", XK_semicolon, 39},
		{"Quote", XK_semicolon, 40},
		// manual
		{"Enter", XK_Return, 28},
		{"Space", XK_space, 57},
		{"BracketLeft", XK_bracketleft, 26},
		{"BracketRight", XK_bracketright, 27},
		{"Backslash", XK_backslash, 43},
		{"Minus", XK_minus, 12},
		{"Equal", XK_equal, 13},
		{"Backquote", XK_apostrophe, 41 },
		{"ShiftRight", XK_Shift_R, 54 },
		{"ShiftLeft", XK_Shift_L, 42 },
		{"CapsLock", XK_Caps_Lock, 58 },
		{"Backspace", XK_BackSpace, 14 },
		{"Delete", XK_Delete, 111 },
		{"AltRight", XK_Alt_R, 100 },
		{"AltLeft", XK_Alt_L, 56 },
		{"ControlLeft", XK_Control_L, 29 },
		{"ControlRight", XK_Control_R, 97 },
		{"Tab", XK_Tab, 15 },
		{"Period", XK_period, 52 },
		{"Comma", XK_comma, 51 },
		{"Slash", XK_slash, 53 },
		{"Escape", XK_Escape, 1 },
		{"F1", -1, 59 },
		{"F2", -1, 60 },
		{"F3", -1, 61 },
		{"F4", -1, 62 },
		{"F5", -1, 63 },
		{"F6", -1, 64 },
		{"F7", -1, 65 },
		{"F8", -1, 66 },
		{"F9", -1, 67 },
		{"F10", -1, 68 },
		{"F11", -1, 87 },
		{"F12", -1, 88 },
		{"ArrowRight", -1, 106},
		{"ArrowLeft", -1, 105},
		{"ArrowUp", -1, 103},
		{"ArrowDown", -1, 108},

		{"Home", -1, 102},
		{"End", -1, 107},
		{"PageUp", -1, 104},
		{"PageDown", -1, 109},

};

// https://manpages.ubuntu.com/manpages/focal/en/man7/virkeycode-linux.7.html



int translateWebKeyCode(Display * dpy, const char * code)
{
	int sym=-1;
	int keycode=-1;
	for(int i=0;i<sizeof(entries)/sizeof(entry_t);++i)
	{
		if(!strcmp(code, entries[i].code))
		{
			sym=entries[i].sym;
			keycode=entries[i].linux_keycode+8;
			break;
		}
	}
	if(sym!=-1)
	{
//		keycode=XKeysymToKeycode(dpy, sym);

		fprintf(stderr, "Code -> sym -> keycode: %s -> %d -> %d\n", code, sym, keycode);
	}
	return keycode;
}
