
all:
	gcc -Ofast -o rrfb ../../src/executeCommand.c ../../src/x11.c ../../src/translateWebKeycode.c -lX11 -lXdamage -lXext -lxdo -lXtst -lXfixes `pkgconf --libs --cflags json-c`
	gcc -Ofast -o testkey ../../src/x11testkey.c ../../src/translateWebKeycode.c -lX11 -lXdamage -lXext -lxdo -lXtst `pkgconf --libs --cflags json-c`
	
