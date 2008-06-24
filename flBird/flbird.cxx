#include <stdio.h>
#include "gui.h"
#include <FL/Fl.h>
#include <FL/fl_ask.H>
#include "birdclient.h"
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include "flbird.h"
#include <string.h>

#define DISCONNECTED 0
#define CONNECTED 1
#define STREAMING 2

#define TIMER_TIMEOUT 0.1

#define SDLX 800
#define SDLY 600

#define POS_AMP 120
#define ANG_AMP 120

#define POS_AXIS 150
#define ANG_AXIS 450

int sstate = DISCONNECTED;
int srvfd = 0;
int graph=0;
SDL_Surface* scr=NULL;

pcbird_pos_t data[SDLX];
int data_wr=0;

static inline void addtext(const char* c)
{
    resultbox->buffer()->append(c);
}

void clear_data()
{
    memset(data,0,SDLX*sizeof(pcbird_pos_t));
}

void clear_screen()
{
    SDL_Rect r = {0,0,SDLX,SDLY};
    SDL_FillRect(scr,&r,0);
}

void draw_data()
{
    int i=data_wr;
    int x;
    int y;
    pcbird_pos_t p;
    
    stringColor(scr,5,10,"x - red",  0xFF0000FF);
    stringColor(scr,155,10,"y - green",0x00FF00FF);
    stringColor(scr,305,10,"z - blue",  0x0000FFFF);
    
    stringColor(scr,5,300,"azimuth - red",  0xFF0000FF);
    stringColor(scr,155,300,"elevation - green",0x00FF00FF);
    stringColor(scr,305,300,"roll - blue",  0x0000FFFF);
    
    for (x=0;x<SDLX;x++)
    {
	p = data[i];
	//pierwsza os
	pixelColor(scr,x,POS_AXIS,0xFFFFFFFF);
	//trzy skladowe: x,y,z
	y = (POS_AXIS - (int)(p.x*POS_AMP));
	pixelColor(scr,x,y,0xFF0000FF);
	y = (POS_AXIS - (int)(p.y*POS_AMP));
	pixelColor(scr,x,y,0x00FF00FF);
	y = (POS_AXIS - (int)(p.z*POS_AMP));
	pixelColor(scr,x,y,0x0000FFFF);
	
	//druga os
	pixelColor(scr,x,ANG_AXIS,0xFFFFFFFF);
	//trzy skladowe: a,b,g
	y = (ANG_AXIS - (int)(p.a/2));
	pixelColor(scr,x,y,0xFF0000FF);
	y = (ANG_AXIS - (int)(p.b)/2);
	pixelColor(scr,x,y,0x00FF00FF);
	y = (ANG_AXIS - (int)(p.g)/2);
	pixelColor(scr,x,y,0x0000FFFF);
	i++;
	if (i >= SDLX)
	    i=0;
    }
    SDL_Flip(scr);
}

void connect_cb()
{
    if (!sstate)
    {
	srvfd = pcbird_connect(hostbox->value(),portbox->value());
	if (srvfd > 0)
	{
	    addtext("#Connected to server...\n");
	    sstate = CONNECTED;
	    hostbox->deactivate();
	    portbox->deactivate();
	}
	else
	{
	    addtext("#Failed to connect to server!\n");
	    connectbtn->value(0);
	}    
    }
    else
    {
	Fl::remove_fd(srvfd);
	pcbird_disconnect(srvfd);
	srvfd = 0;
	sstate = DISCONNECTED;
	streambtn->value(0);
	plotbtn->value(0);
	SDL_Quit();
	addtext("#Disconnected from server...\n");
	hostbox->activate();
	portbox->activate();
	
    }
}

void exit_cb()
{
    if (sstate)
	pcbird_disconnect(srvfd);
    if (plotbtn->value())
	SDL_Quit();
    exit(0);
    
}

void print_data(pcbird_pos_t* d)
{
    char w[1024];    
    sprintf(w,"[x, y, z] = [%.3f, %.3f, %.3f]\n",d->x,d->y,d->z);
    addtext(w);
    sprintf(w,"[a, b, g] = [%.3f, %.3f, %.3f]\n",d->a,d->b,d->g);
    addtext(w);
    sprintf(w,"dist = %.3f ts = %d.%d\n",d->distance,d->ts_sec,d->ts_usec);
    addtext(w);
}


void single_cb()
{
    if (sstate==CONNECTED)
    {
	pcbird_pos_t p;
	pcbird_get_single_position(srvfd,&p);
	print_data(&p);
    }
    if (sstate == DISCONNECTED)
	addtext("#Connect first...\n");
}


void incoming_data_cb(int, void*)
{
	pcbird_pos_t p;
	pcbird_get_streaming_position(srvfd,&p);
	print_data(&p);
	data[data_wr] = p;
	data_wr++;
	if (data_wr >= SDLX)
	    data_wr = 0;
}

void streaming_cb()
{
    if (sstate == CONNECTED)
    {
	sstate = STREAMING;
	pcbird_start_streaming(srvfd);
	Fl::add_fd(srvfd,&incoming_data_cb);
	
	return;
    }
    if (sstate == STREAMING)
    {
	pcbird_stop_streaming(srvfd);
	Fl::remove_fd(srvfd);
	sstate = CONNECTED;
    }
    if (sstate == DISCONNECTED)
    {
	addtext("#Connect first...\n");
	streambtn->value(0);
    }
}


void save_cb()
{
    const char* filename;
    filename = fl_input("Saving output, enter filename:");
    if (filename)
	resultbox->buffer()->savefile(filename);
}


void plot_cb()
{
    if (plotbtn->value()==0)
    {
	SDL_Quit();
    }
    else
    {
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
	{
    	    fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
    	    exit(1);
	}
	
	scr = SDL_SetVideoMode(SDLX, SDLY, 32, SDL_SWSURFACE);
	if ( scr == NULL ) {
    	    addtext("Unable to open SDL window!\n");
	    plotbtn->value(0);
	    return;
	}
	SDL_WM_SetCaption( "flBird - plot", NULL );
	clear_data();
	Fl::add_timeout(TIMER_TIMEOUT, timer_cb);
    }
}



void clear_cb()
{
    resultbox->buffer()->remove(0,resultbox->buffer()->length());
}

void timer_cb(void*) 
{
    SDL_Event e;
    if (!plotbtn->value()) return;
    while( SDL_PollEvent(&e)) 
    {
        if( e.type == SDL_QUIT )
        {
    	    plotbtn->value(0);
	    SDL_Quit();
	    return;
	}
    }
    clear_screen();
    draw_data();
    Fl::repeat_timeout(TIMER_TIMEOUT, timer_cb);
}



int main(int argc,char** argv)
{
    
    atexit(SDL_Quit);
    make_window();
    mainwnd->show(argc,argv);

    while (Fl::wait() > 0)
    {	
    }
    return 0;
}

