CC=`fltk-config --cxx`
CFLAGS=`fltk-config --cflags` `fltk-config --cxxflags` `sdl-config --cflags`
LDFLAGS=`fltk-config  --ldflags` `sdl-config --libs` -lSDL_gfx
OBJS=flbird.o gui.o birdclient.o
TARGET=flbird

all:	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(TARGET)

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o:	%.cxx
	$(CC) $(CFLAGS) -c $< -o $@

