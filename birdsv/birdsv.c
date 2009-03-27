#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/types.h>
#include <inttypes.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>

#define MAX_CLIENTS 10

#define SVCMD_POS 1
#define SVCMD_START_STREAMING 2
#define SVCMD_STOP_STREAMING 3
#define SVCMD_END_SESSION 4

#define CLIENT_STATE_IDLE 0
#define CLIENT_STATE_STREAMING 1

//using namespace std;

#define PCB_STR 0x4000
#define PCB_PNT 0x4200
#define PCB_ANG 0x5700
#define PCB_POS 0x5600
#define PCB_POS_ANG 0x5900

struct pos_ang {
    uint16_t x,y,z,a,b,g;
    struct timeval ts;
};

#define POS_QUEUE_DEPTH 256

struct pos_ang pos_queue[POS_QUEUE_DEPTH];
int pos_queue_head = 0, pos_queue_tail = 0;
pthread_mutex_t pos_queue_lock = PTHREAD_MUTEX_INITIALIZER;

//irq15
static uint16_t base_addr = 0x300;
static uint16_t tcp_port = 12345;
static int uio_fd;
static struct pos_ang last_pos;
static int no_daemon = 0;

uint16_t pcb_init()
{
//    printf("Reset in progress...");
    //przyznaj sobie prawa do zabawy portami
    iopl(3);
    ioperm(base_addr, 3, 1);
    //reset ptaka
    outw(0x0, base_addr+2);
    outw(0x1, base_addr+2);
    //poczekaj az ptak gotow bedzie    
    while (!(inw(base_addr+2)&1));
}

void pcb_status()
{
    uint16_t s = ~inw(base_addr+2);
    printf("Status %x\n",s);
}


void pcb_send_cmd(uint16_t c)
{
    while (!(inw(base_addr+2)&1));
    outw(c,base_addr);
}

uint16_t pcb_read_data()
{
    while (!(inw(base_addr+2)&2));
    return inw(base_addr);	
}

int pos_queue_put(struct pos_ang p)
{
    if ((pos_queue_head + 1) % POS_QUEUE_DEPTH == pos_queue_tail) return -1;
    
    pos_queue_head ++; pos_queue_head %= POS_QUEUE_DEPTH;
    pos_queue[pos_queue_head] = p;
    
    return 0;
}

int pos_queue_get(struct pos_ang *p)
{
    int tmptail;     

    if (pos_queue_head == pos_queue_tail) return -1;

    tmptail = ( pos_queue_tail + 1 ) % POS_QUEUE_DEPTH;
    memcpy(p, &pos_queue[tmptail], sizeof(struct pos_ang));

    pos_queue_tail = tmptail;
    
    return 0;
}

void die(const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    vfprintf(stderr, fmt, vargs);
    va_end(vargs);
    
    exit(-1);
}

void *hw_thread_entry(void *dummy)
{
    uint16_t a, b, g;
    int fd,cnt=0,t;
    
    int lastt=0;

    iopl(3);
    ioperm(base_addr, 3, 1);

    pcb_init();
    pcb_send_cmd(PCB_POS_ANG);
    pcb_send_cmd(PCB_PNT);

    for(;;)
    {
	struct pos_ang p;
	struct timezone tz = {0, 0};
	int nr,t;
	//nr=read( uio_fd, &t, 4);
	
	while (!(inw(base_addr+2)&2));
	p.x = inw(base_addr);
	while (!(inw(base_addr+2)&2));
	p.y = inw(base_addr);
	while (!(inw(base_addr+2)&2));
	p.z = inw(base_addr);

	while (!(inw(base_addr+2)&2));
	p.a = inw(base_addr);
	while (!(inw(base_addr+2)&2));
	p.b = inw(base_addr);
	while (!(inw(base_addr+2)&2));
	p.g = inw(base_addr);

	if(!(p.x & 1))
	{
	    fprintf(stderr,"unsync!\n");
	}

	gettimeofday(&p.ts, &tz);

	pthread_mutex_lock(&pos_queue_lock);
	int rv = pos_queue_put(p);
	
	if(rv < 0)
	    die("Error - full queue!\n");

	pthread_mutex_unlock(&pos_queue_lock);

    
//	fprintf(stderr,".");
        pcb_send_cmd(PCB_PNT);
	usleep(20000);    
    }    
    return NULL;
}

static pthread_t hw_thread;


void uio_init()
{
    FILE *f = fopen("/sys/class/uio/uio0/name","rb");
    
    if(!f)
	die("No uio sys class registered. Have you installed pcbird_uio kernel module?\n");

    char name[256];
    fscanf(f,"%s", name);
    if(strcmp(name,"PCBird"))
    {
	fclose(f);
	die("Invalid UIO driver\n");
    }
    fclose(f);

    uio_fd = open("/dev/uio0",O_RDONLY);
    if(!uio_fd) die("Can't open UIO device\n");
}

int sock_fd;

struct client {
    int state;
    int fd;    
} client_tab[MAX_CLIENTS];

void tcpsv_init()
{
    struct sockaddr_in sin;
    
    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock_fd < 0)
    {
	perror("socket()");
	exit(-1);
    }

    int optval = 1;
    
    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0)
    {
	perror("setsockopt()");
	exit(-1);
    }

    if(fcntl(sock_fd, F_SETFL, O_NONBLOCK) < 0)
    {
	perror("fcntl()");
	exit(-1);
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(tcp_port);
    
    if(bind (sock_fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0)
    {
	perror("bind()");
	exit(-1);
    }
    
    if(listen(sock_fd, MAX_CLIENTS) < 0)
    {
	perror("listen()");
	exit(-1);
    }

    memset(client_tab, 0, sizeof(client_tab));
}

static int add_client(int fd)
{
    int i;
    
    for(i=0;i<MAX_CLIENTS;i++)
	if(!client_tab[i].fd)
	{
	    client_tab[i].fd = fd;
	    client_tab[i].state = CLIENT_STATE_IDLE;
	    return i;
	}

    return -1;
}

static int remove_client(int fd)
{
    int i;
    
    for(i=0;i<MAX_CLIENTS;i++)
	if(client_tab[i].fd == fd)
	{
	    client_tab[i].fd = 0;
	    client_tab[i].state = CLIENT_STATE_IDLE;
	}
    return 0;
}

static int find_client(int fd)
{
    int i;
    
    for(i=0;i<MAX_CLIENTS;i++)
	if(client_tab[i].fd == fd)
	    return i;

    return -1;
}

static int send_position(int fd, struct pos_ang p)
{
    uint32_t txbuf[5];
    uint8_t *ptr = (uint8_t*) &txbuf[0];

//    printf("%04x %04x %04x\n", p.x, p.y, p.z);

    *(uint16_t*)(ptr) = htons(p.x);
    *(uint16_t*)(ptr+2) = htons(p.y);
    *(uint16_t*)(ptr+4) = htons(p.z);
    *(uint16_t*)(ptr+6) = htons(p.a);
    *(uint16_t*)(ptr+8) = htons(p.b);
    *(uint16_t*)(ptr+10) = htons(p.g);
    *(uint32_t*)(ptr+12) = htonl(p.ts.tv_sec);
    *(uint32_t*)(ptr+16) = htonl(p.ts.tv_usec);
    
    return send(fd, txbuf, 20, 0) == 20 ? 0 : -1;
}

int handle_client(int id)
{
    uint8_t rx_cmd;
    
    int nrx = recv(client_tab[id].fd, &rx_cmd, 1, 0);
    
    if(nrx<0) return -1;
    if(nrx == 0) return 0;
    
    switch(rx_cmd)
    {
	case SVCMD_POS:
//	    fprintf(stderr,"Send!\n");

	    return send_position(client_tab[id].fd, last_pos);

	case SVCMD_START_STREAMING:
	    client_tab[id].state = CLIENT_STATE_STREAMING;
	    return 0;

	case SVCMD_STOP_STREAMING:
	    client_tab[id].state = CLIENT_STATE_IDLE;
	    return 0;
	    
	case SVCMD_END_SESSION: return -1;
	default: return -1;
    }
    
}

int handle_requests()
{
    struct sockaddr_in remote;
    struct pollfd pfds[MAX_CLIENTS];
    int cli_fd, i, n;
    
    size_t len = sizeof(remote);

// akceptujemy przychodzace polaczenia i dodajemy nowych klientow do tablicy
    while ((cli_fd = accept(sock_fd, (struct sockaddr *)&remote, &len))>0)
	if( add_client(cli_fd) < 0) 
	{
	    close(cli_fd);
	    return 0;
	}


// przygotowujemy liste deskryptorow dla poll()-a
    for(n=i=0;i < MAX_CLIENTS;i++)
	if(client_tab[i].fd)
	{    
	    pfds[n].fd = client_tab[i].fd;
	    pfds[n].events = POLLIN | POLLHUP; // reagujemy na przychodzace dane i zerwanie sesji
	    pfds[n].revents = 0;

	    n++;
	}    

    int ret;

// pollujemy klientow    
    if(( ret = poll(pfds, n, 10)) < 0) die("poll error: %s", strerror(errno));

// brak ruchu - koniec pracy funkcji
    if(!ret)  return 0; 

// sprawdzamy, co sie wydarzylo:
    for(i=0; i<n ;i++)
    {
	if(pfds[i].revents & POLLHUP)					// klient zerwal polaczenie?
	{
	    fprintf(stderr, "[lsrv] client %d closed connection.\n", pfds[i].fd);
	    close(pfds[i].fd);
	    remove_client(pfds[i].fd);
	} else if(pfds[i].revents & POLLIN)				// nadeszly dane od klienta?
	{
	    if(handle_client(find_client(pfds[i].fd)) < 0)
	    {
	        close(pfds[i].fd);
		remove_client(pfds[i].fd);
    	    }
	}  	
    }
    
    return 0;    

}


void handle_streaming()
{
    int i=0, j, npos = 0;
    struct pos_ang pos_buf[POS_QUEUE_DEPTH];

    pthread_mutex_lock(&pos_queue_lock);
    while(!pos_queue_get(&pos_buf[npos])) npos++;
    pthread_mutex_unlock(&pos_queue_lock);

    if(npos)
    {
	memcpy(&last_pos, &pos_buf[npos-1], sizeof(struct pos_ang));

	for(i=0; i<MAX_CLIENTS; i++) 
	{
	    if(client_tab[i].state == CLIENT_STATE_STREAMING && client_tab[i].fd>0)
	    {

	        for(j=0;j<npos;j++)
	    	    send_position(client_tab[i].fd, pos_buf[j]);
	    }
	}
    }
}


void parse_args(int argc, char *argv[])
{
    int i=1;
    
    while(i<argc)
    {
	if(!strcmp(argv[i], "-nd"))
	{
	    no_daemon = 1; i++;
	} else if (!strcmp(argv[i], "-ioport"))
	{
	    sscanf(argv[i+1],"0x%x", &base_addr);
	    if(base_addr<0x10 || base_addr>0x400) die("Invalid base address.\n");
	    i+=2;
	} else if(!strcmp(argv[i], "-tcpport"))
	{
	    sscanf(argv[i+1],"%d", &tcp_port);
	    if(tcp_port==0) die("Invalid TCP listen port.\n");
	    i+=2;
	} else if (!strcmp(argv[i], "-help"))
	{
	    printf("birdsv ver 0.0.2 (c) TW 2008\n\n");
	    printf("Usage: %s [-nd] [-ioport port] [-tcpport port]\n\n",argv[0]);
	    printf("Where: -nd - don't run as daemon\n");
	    printf("       -ioport port - specify PCBird I/O base address in hex (default = 0x%x)\n", base_addr);
	    printf("       -tcpport - specify TCP/IP listen port (default = %d)\n\n", tcp_port);
	    exit(0);
    
	    i++;
	} else {
	    fprintf(stderr,"Invalid argument. Type %s -help for help.\n\n", argv[0]);
	    exit(0);
	}
    
    
    }
}

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

// Funkcja zamienia proces w demona
static void daemonize(void)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
	
        exit(EXIT_SUCCESS);
    }

    /* At this point we are executing as the child process */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
}

void sighandler(int sig)
{
    fprintf(stderr,"Exiting due to signal %d.\n", sig);
    close(uio_fd);
    close(sock_fd);

    
    exit(0);
}

main(int argc, char *argv[])
{

    if(getuid() != 0)
	die("This program requires to be run as root.\n");

    parse_args(argc, argv);

    if(!no_daemon) daemonize();

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGQUIT, sighandler);
    
    uio_init();
    tcpsv_init();

    if(pthread_create(&hw_thread, NULL, hw_thread_entry, NULL) < 0)
	die("HW thread creation error\n");

    for(;;)
    {
	handle_requests();
	handle_streaming();
    }
}
