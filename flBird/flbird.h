#ifndef __FLBIRD_H
#define __FLBIRD_H
#define DEFAULT_PORT 12345

extern int srvfd;
extern int sstate;
extern int graph;
void connect_cb();
void exit_cb();
void single_cb();
void streaming_cb();
void save_cb();
void plot_cb();
void timer_cb(void*);
void clear_cb();
#endif

