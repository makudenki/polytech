/**
 *****************************************************************************

 @file       dfframe.h 

 @brief      DirectFB frame work

 @author     Hiroki Takada

 @date       2016-03-31

 @version    $Id:$

  ----------------------------------------------------------------------------
  RELEASE NOTE :

   DATE          REV    REMARK
  ============= ====== =======================================================
  26th May 2015  0.1   Initial release
  30th Mar 2016  0.2   Add network interfaces

 *****************************************************************************/

#ifndef _DFFRAME_H
#define _DFFRAME_H
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <semaphore.h>

#include <directfb.h>
#include <directfb_util.h>
#include <direct/clock.h>



/* -------------------------- macro  declarations -------------------------- */

// macro function to check the returns from DirectFB API
#define DFBCHECK(x...) { \
    DFBResult err = x; \
    if (err != DFB_OK) { \
        fprintf(stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err ); \
        exit(EXIT_FAILURE); \
    } \
}

/* ------------------------------------------------------------------------- */



/* --------------------------- type  definitions --------------------------- */

typedef struct position {
    int                     x;
    int                     y;
} position_t;

typedef struct scsize {
    int                     w;
    int                     h;
} scsize_t;

typedef struct color {
    unsigned char           r;
    unsigned char           g;
    unsigned char           b;
    unsigned char           a;
} color_t;

typedef DFBRectangle region_t;

typedef enum {
    TOUCHED,
    RELEASED,
    ERROR
} TouchState;

typedef struct server {
    struct sockaddr_in      addr;
    struct sockaddr_in      sender;
    int                     sfd;
    int                     backlog;
} server_t;

typedef struct connection {
    struct sockaddr_in      addr;
    int                     sfd;
} connection_t;

typedef struct connection udpsocket_t;

/* ------------------------------------------------------------------------- */



/* ------------------------------- parameters ------------------------------ */

// position was examined every POSSAMPLES(60) samples by means of average
#define POSSAMPLES 60 

// calibration
static const int            CalX1       =   205;
static const int            CalY1       =  3587;
static const int            CalXR       =  3738;
static const int            CalYR       = -3371;

// maximum lenght of file path string
// used for making command string to play musics
#define MAXPATHSTR 255
#define STRBUFFLEN 512

/* ------------------------------------------------------------------------- */



/* --------------------------- global  variables --------------------------- */

/* ------------------------------------------------------------------------- */



/* ------------------------ prototype  declarations ------------------------ */

void init                    (int *argc, char ***argv);
void initialize              (int *argc, char ***argv);
void createPrimarySurface    (void);
void createEventBuffer       (void);
void initSemaphore           (void);

void flip                    (void);
void clearScreen             (void);
void setColor                (int r, int g, int b, int a);
void fillScreen              (int r, int g, int b, int a);

void release                 (void);
void releaseImage            (int index);

bool getInputEvent           (DFBInputEvent *e);
bool handleButton            (DFBInputEvent *e);
bool handleAxes              (DFBInputEvent *e);
position_t eventLoop         (void);
TouchState getTouchState     (void);

bool readImage               (int index, const char * path);

void renderImage             (int index, bool alpha);
void putImage                (int index, position_t p, bool alpha);
void stretchImage            (int index, region_t from, region_t to, bool alpha);

void rectangle               (region_t r, bool fill);
void line                    (position_t from, position_t to);
void triangle                (position_t p1, position_t p2, position_t p3);

scsize_t getSize             (void);
scsize_t getSurfaceSize      (int index);

bool setFont                 (const char * path, int size);
int  stringWidth             (const char * text);
void putString               (const char * text, position_t p);
void putStringAligned        (const char * text, position_t p, DFBSurfaceTextFlags flg);
void unsetFont               (void);

void messageBox              (const char * message, region_t r, position_t off, color_t fg, color_t bg);

void playMusic               (const char * path, bool back);
int  isPlaying               (void);
void stopMusic               (void);

// TCP server and connection
server_t     * startServer   (int port, int backlog);
connection_t * waitClient    (server_t * server);
connection_t * connectServer (const char * ipaddr, int port);
int  sendData                (connection_t * conn, const char * data,   int size);
int  recvData                (connection_t * conn, char       * buffer, int max );

// UDP server and socket
server_t    *  udpServer     (int port);
udpsocket_t *  udpSocket     (const char   * ipaddr, int port);
int            sendDataTo    (udpsocket_t  * usck, const char * data,   int size);
int            recvDataFrom  (server_t     * serv,       char * buffer, int max );

// cleaning up
void closeServer             (server_t * server);
void closeConnection         (connection_t * conn);

/* ------------------------------------------------------------------------- */

#endif
