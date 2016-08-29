/**
 *****************************************************************************

 @file       dfframe.c

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

#include "dfframe.h"


/* --------------------------- global  variables --------------------------- */

// super interface
static IDirectFB *            dfb         = NULL;

// primary surface
static IDirectFBSurface *     primary     = NULL;

// current color
static color_t                ccolor      = {0, 0, 0, 0xff};

// surface for logo
#define NUM_SURFACE 10
static IDirectFBSurface *     logo[NUM_SURFACE];
static DFBSurfaceDescription  ldsc[NUM_SURFACE];

// buffer for the button input events
static IDirectFBEventBuffer * eventbuffer = NULL;

// font
static IDirectFBFont *        font        = NULL;
static DFBFontDescription     fdsc;
static pthread_mutex_t        fontLock    = PTHREAD_MUTEX_INITIALIZER;

// properties of the primary surface
static DFBSurfaceDescription  dsc;

// X cordinate resolution of the primary surface
static int                    xres        = 0;

// y cordinate resolution of the primary surface
static int                    yres        = 0;

// current position
static position_t             curpos      = {0, 0};

// array for position values
static position_t             positions[POSSAMPLES];

// number of position samples
static int                    samples     = 0;

// semaphore for position determinating synchronization
static sem_t                  positiondet;

// touch state
static TouchState             tstate      = RELEASED;
static struct timespec        lastTouch;
static const long             ignoreItrvl = 100000000;

// playing music
static bool                   playNow     = false;
static pthread_mutex_t        playLock    = PTHREAD_MUTEX_INITIALIZER;
static pthread_t              playth;
static time_t                 playtime;
static char *                 aplay       = "aplay  -D hw:0,1 ";
static char *                 mpg123      = "mpg123 -a hw:0,1 ";
static char                   musicCommand[STRBUFFLEN];

/* ------------------------------------------------------------------------- */



/* ---------------------------- implementations ---------------------------- */

static void   _initialize       (void);
static void   _clearLogoSurface (void);
static bool   _checkIndex       (int index);
static bool   _checkSurface     (int index);
static void * _playMusic        (void *data);

/**
 * Internal initializing tasks
 */
static void _initialize (void)
{

    return;

}


/**
 * Clear array of pointer for logo surface
 */
static void _clearLogoSurface (void)
{

    int                     i;

    for (i = 0; i < NUM_SURFACE; i++) {
        logo[i] = NULL;
    }

}


/**
 * Check if the primary surface is available
 * and the index is valid
 * @return true on valid index and primary surface
 */
static bool _checkIndex (int index)
{

    // check if primary surface is available
    if (primary == NULL) {
        return false;
    }

    // check if index is valid
    if (index < 0 || index >= NUM_SURFACE) {
        return false;
    }

    return true;

}


/**
 * Check if the primary and logo surface is available
 * and the index is valid
 * @return true on valid index and the surfaces
 */
static bool _checkSurface (int index)
{

    // check if primary surface is available
    if (primary == NULL) {
        return false;
    }

    // check if index is valid
    if (index < 0 || index >= NUM_SURFACE) {
        return false;
    }

    // check if logo[index] is available
    if (logo[index] == NULL) {
        return false;
    }

    return true;

}


/**
 * Thread function to play music
 * @param data command string
 * @return dummy
 */
static void * _playMusic (void *data)
{

    system((char *)data);
    pthread_mutex_lock(&playLock);
    playNow = false;
    pthread_mutex_unlock(&playLock);
    return (void *)NULL;

}


/**
 * Initializing function
 * Initialize everything at once
 * @param argc number of arguments for program invokation
 * @param argv string array of the arguments
 */
void init (int *argc, char ***argv)
{

    // create DirectFB super interface
    initialize(argc, argv);

    // create primary surface
    createPrimarySurface();

    // create event buffer
    createEventBuffer();

    // initialize semaphore
    initSemaphore();

}


/**
 * Initializing function
 * creates DirectFB super interface, sets the screen properties and creates
 * primary surface.
 * @param argc number of arguments for program invokation
 * @param argv string array of the arguments
 */
void initialize (int *argc, char ***argv)
{

    // create DirectFB super interface
    if (dfb == NULL) {
        // initialize DirectFB
        DFBCHECK(DirectFBInit(argc, argv));
    
        // create a super interface
        DFBCHECK(DirectFBCreate(&dfb));
   
        // set fullscreen mode
        DFBCHECK(dfb->SetCooperativeLevel(dfb, DFSCL_FULLSCREEN));

        // misc initilizing tasks
        _initialize();

        // clear logo surface
        _clearLogoSurface();
    }

}



/**
 * Create a primary surface
 *
 */
void createPrimarySurface (void)
{

    // check if the super interface has already been initialized
    if (dfb == NULL) {
        return;
    }

    // create primary surface
    if (primary == NULL) {

        // set properties of the primary surface
        dsc.flags = DSDESC_CAPS;
        dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

        // create primary surface
        DFBCHECK(dfb->CreateSurface(dfb, &dsc, &primary));

        // check out the screen resolution
        DFBCHECK(primary->GetSize(primary, &xres, &yres));
    }

}



/**
 * Create an input event buffer
 * 
 */
void createEventBuffer (void)
{

    // check if the super interface has already been initialized
    if (dfb == NULL) {
        return;
    }

    // create an button event buffer
    if (eventbuffer == NULL) {
        DFBCHECK(dfb->CreateInputEventBuffer(dfb, DICAPS_ALL, DFB_FALSE, &eventbuffer));
    }

}



/**
 * Initialize the semaphores to syncronize
 *
 */
void initSemaphore (void)
{

    static bool             check = false;

    // this should be processed just onece
    if (check) {
        return;
    } else {
        check = true;
    }

    // initialize semapohores
    sem_init(&positiondet, 0, 0);

}



/**
 * Wait for an input event
 * @param e variable to stor input event
 * @return true on valid event, false if not
 */
bool getInputEvent (DFBInputEvent *e)
{

    // check if event buffer is initialized
    if (eventbuffer == NULL) {
        return false;
    }

    // wait for an input event
    eventbuffer->WaitForEvent(eventbuffer);

    // retrieve the event
    if (eventbuffer->GetEvent(eventbuffer, DFB_EVENT(e)) != DFB_OK) {
        return false;
    }

    return true;

}



/**
 * Handling button event
 * @param e input event
 * @return true on touch on/off event, false if not
 */
bool handleButton (DFBInputEvent *e)
{

    struct timespec         curtime;
    long                    duration;

    // touch on/off event
    switch (e->type) {
        case DIET_BUTTONPRESS:
            clock_gettime(CLOCK_REALTIME, &lastTouch);
            tstate = TOUCHED;
            return true;
        case DIET_BUTTONRELEASE:
            clock_gettime(CLOCK_REALTIME, &curtime);
            duration  = curtime.tv_nsec - lastTouch.tv_nsec;
            duration += (curtime.tv_sec - lastTouch.tv_sec) * 1000000000;
            if (ignoreItrvl > duration) {
                // chuttering
                break;
            }
            tstate = RELEASED;
            return true;
        default:
            break;
    }

    return false;

}



/**
 * Handling current position
 * @param e input event
 * @return true on axis event or false otherwise
 */
bool handleAxes (DFBInputEvent *e)
{

    static unsigned int     st = 0;
    static int              x, y;
    int                     i;

    // touch off syncronization
    if (tstate == RELEASED) {
        samples = 0;
    }

    // not likely but just in case
    if (e->type != DIET_AXISMOTION) {
        return false;
    }

    // check value
    switch (e->axis) {
        case DIAI_X:    // X axis
            x   = e->axisabs;
            st |= 1;
            break;
        case DIAI_Y:    // Y axis
            y   = e->axisabs;
            st |= 2;
            break;
        default:
            return false;
    }

    // put position into the array
    if (st == 3) {

        // put sampled position
        positions[samples].x = x;
        positions[samples].y = y;

        // reset
        st = 0;

        // update number of samples
        samples++;

        // reach max number of samples
        if (samples == POSSAMPLES) {

            // calculate average
            x = 0;
            y = 0;
            for (i = 0; i < POSSAMPLES; i++) {
                x += positions[i].x;
                y += positions[i].y;
            }
            x /= POSSAMPLES;
            y /= POSSAMPLES;

            // update current position
            curpos.x = (int)(((x - CalX1) * (xres - 1)) / CalXR);
            curpos.y = (int)(((y - CalY1) * (yres - 1)) / CalYR);

            // position determinating sync
            sem_post(&positiondet);

            // reset number of samples
            samples = 0;

        }
    }

    return true;

}


/**
 * Input event loop
 * @return axis, negative value on error
 */
position_t eventLoop (void)
{

    DFBInputEvent           e;
    int                     val;

    while (getInputEvent(&e)) {

        // handle key event
        handleButton(&e);

        // handle axis event
        handleAxes(&e);

        // break if released
        if (tstate == RELEASED) {
            break;
        }

        // check if axis is determined
        sem_getvalue(&positiondet, &val);
        if (val > 0) {
            sem_wait(&positiondet);
            break;
        }
    }

    return curpos;

}


/**
 * Return touch state
 * @return the current touch state
 */
TouchState getTouchState (void)
{

    return tstate;

}


/**
 * Flip the buffer
 */
void flip (void)
{

    if (primary == NULL) {
        return;
    }
    DFBCHECK(primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC));

}


/**
 * Clear the screen
 */
void clearScreen (void)
{

    if (primary == NULL) {
        return;
    }

    DFBCHECK(primary->SetColor(primary, 0, 0, 0, 0xff));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, xres, yres));

}


/**
 * Change current color of the primary surface
 * @param c color
 */
void setColor (int r, int g, int b, int a)
{

    color_t                 c = {r, g, b, a};

    if (primary == NULL) {
        return;
    }

    //
    DFBCHECK(primary->SetColor(primary, c.r, c.g, c.b, c.a));

    // save current color
    ccolor = c;

}


/**
 * Fill the screen with a color
 * @param c color
 */
void fillScreen (int r, int g, int b, int a)
{

    color_t                 c = {r, g, b, a};

    if (primary == NULL) {
        return;
    }

    DFBCHECK(primary->SetColor(primary, c.r, c.g, c.b, c.a));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, xres, yres));

}


/**
 * Release all the resource
 */
void release (void)
{

    int                     i;

    // font
    if (font != NULL) {
        font->Release(font);
        font = NULL;
    }

    // event buffer
    if (eventbuffer != NULL) {
        eventbuffer->Release(eventbuffer);
        eventbuffer = NULL;
    }

    // logo buffer
    for (i = 0; i < NUM_SURFACE; i++) {
        if (logo[i] != NULL) {
            logo[i]->Release(logo[i]);
            logo[i] = NULL;
        }
    }

    // primary surface
    if (primary != NULL) {
        primary->Release(primary);
        primary = NULL;
    }

    // super interface
    if (dfb != 0) {
        dfb->Release(dfb);
        dfb = NULL;
    }

}



/**
 * Release the image
 * @param index index of the array for logo surface
 */
void releaseImage (int index)
{

    // check if index is valid
    if (! _checkIndex(index)) {
        return;
    }

    // release the surface if allocated
    if (logo[index] != NULL) {
        logo[index]->Release(logo[index]);
        logo[index] = NULL;
    }

}


/**
 * Read the image
 * @param index index of the array for logo surface
 * @param path file path
 * @return true on success, false otherwise
 */
bool readImage (int index, const char *path)
{

    IDirectFBImageProvider *provider;

    // check primary surface and index
    if (! _checkIndex(index)) {
        return false;
    }

    // check if logo is available
    if (logo[index] != NULL) {
        releaseImage(index);
    }

    // create image provider
    DFBCHECK(dfb->CreateImageProvider(dfb, path, &provider));

    // obtain information of the image
    DFBCHECK(provider->GetSurfaceDescription(provider, &ldsc[index]));

    // create surface using image description
    DFBCHECK(dfb->CreateSurface(dfb, &ldsc[index], &logo[index]));

    // render to the surface
    DFBCHECK(provider->RenderTo(provider, logo[index], NULL));

    // release provider
    provider->Release(provider);

    return true;

}



/**
 * Render the image at top left coner
 * @param index index of the array for logo surface
 * @param alpha enable alpha blending on true
 */
void renderImage (int index, bool alpha)
{

    // check if primary surface is available
    if (! _checkSurface(index)) {
        return;
    }

    // set setting of blending
    if (alpha) {
        DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_BLEND_ALPHACHANNEL));
    }

    DFBCHECK(primary->Blit(primary, logo[index], NULL, 0, 0));

    // restore setting of blending
    DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_NOFX));

}



/**
 * Render the image at the position
 * @param index index of the array for logo surface
 * @param p position to render the image 
 * @param alpha enable alpha blending on true
 */
void putImage (int index, position_t p, bool alpha)
{

    // check if primary surface is available
    if (! _checkSurface(index)) {
        return;
    }

    // set setting of blending
    if (alpha) {
        DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_BLEND_ALPHACHANNEL));
    }

    DFBCHECK(primary->Blit(primary, logo[index], NULL, p.x, p.y));

    // restore setting of blending
    DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_NOFX));

}


/**
 * Render the image with regions
 * @param index index of the array for logo surface
 * @param from region of the source image
 * @param to region of the destination surface
 * @param alpha enable alpha blending on true
 */
void stretchImage (int index, region_t from, region_t to, bool alpha)
{

    // check if primary surface is available
    if (! _checkSurface(index)) {
        return;
    }

    // set setting of blending
    if (alpha) {
        DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_BLEND_ALPHACHANNEL));
    }

    DFBCHECK(primary->StretchBlit(primary, logo[index], &from, &to));

    // restore setting of blending
    DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_NOFX));

}


/**
 * Draw rectangle on primary surface
 * @param r region
 * @param fill fill the rectangle on true
 */
void rectangle (region_t r, bool fill)
{

    // check if the primary surface is available
    if (primary == NULL) {
        return;
    }

    // draw rectangle
    if (fill) {
        DFBCHECK(primary->FillRectangle(primary, r.x, r.y, r.w, r.h));
    } else {
        DFBCHECK(primary->DrawRectangle(primary, r.x, r.y, r.w, r.h));
    }

}


/**
 * Draw line
 * @param from starting point
 * @param to terminating point
 */
void line (position_t from, position_t to)
{

    // check if the primary surface is available
    if (primary == NULL) {
        return;
    }

    DFBCHECK(primary->DrawLine(primary, from.x, from.y, to.x, to.y));

}


/**
 * Draw triangle
 * @param p1
 * @param p2
 * @param p3
 */
void triangle (position_t p1, position_t p2, position_t p3)
{

    // check if the primary surface is available
    if (primary == NULL) {
        return;
    }

    DFBCHECK(primary->FillTriangle(primary, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y));

}


/**
 * Get screen size 
 * @return screen size
 */
scsize_t getSize (void)
{

    scsize_t            res = {-1 , -1};

    // check if the primary surface is available
    if (primary == NULL) {
        return res;
    }

    res.w = xres;
    res.h = yres;

    return res;

}


/**
 * Get surface size 
 * @return surface size 
 */
scsize_t getSurfaceSize (int index)
{

    scsize_t            s = {-1, -1};

    // check if logo surface is available
    if (! _checkSurface(index)) {
        return s;
    }

    DFBCHECK(logo[index]->GetSize(logo[index], &s.w, &s.h));

    return s;

}


/**
 * Set font to the primary surface
 * @param path path to the font file
 * @param size font size
 * @return true on success, false otherwise
 */
bool setFont (const char * path, int size)
{

    // check if the primary surface is available
    if (primary == NULL) {
        return false;
    }

    // check if the font has already set
    if (font != NULL) {
        unsetFont();
    }

    // lock
    pthread_mutex_lock(&fontLock);

    // specify size
    fdsc.flags  = DFDESC_HEIGHT;
    fdsc.height = size;

    // create font
    DFBCHECK(dfb->CreateFont(dfb, path, &fdsc, &font));

    // set font
    DFBCHECK(primary->SetFont(primary, font));

    // unlock
    pthread_mutex_unlock(&fontLock);

    return true;

}


/**
 * Calculate string width
 * @param text text to draw
 * @return string width, -1 on error
 */
int stringWidth (const char * text)
{

    int                 width;

    // check if the font has already set
    if (font == NULL) {
        return -1;
    }

    // lock
    pthread_mutex_lock(&fontLock);

    DFBCHECK(font->GetStringWidth(font, text, -1, &width));

    // unlock
    pthread_mutex_unlock(&fontLock);

    return width;

}


/**
 * Draw left aligned text on the primary surface
 * @param text text to draw
 * @param p position
 */
void putString (const char * text, position_t p)
{

    putStringAligned(text, p, DSTF_LEFT); 

}


/**
 * Draw aligned text on the primary surface
 * @param text text to draw
 * @param p position
 */
void putStringAligned (const char * text, position_t p, DFBSurfaceTextFlags flg)
{

    // check if the font has already set
    if (font == NULL) {
        return;
    }

    // lock
    pthread_mutex_lock(&fontLock);

    // draw string
    DFBCHECK (primary->DrawString(primary, text, -1, p.x, p.y, flg));

    // unlock
    pthread_mutex_unlock(&fontLock);

}


/**
 * Unset font
 */
void unsetFont (void)
{

    // check if the font has already set
    if (font == NULL) {
        return;
    }

    // lock
    pthread_mutex_lock(&fontLock);

    // unset font
    DFBCHECK(primary->SetFont(primary, NULL));

    // release resource
    font->Release(font);
    font = NULL;

    // unlock
    pthread_mutex_unlock(&fontLock);

}


/**
 * Draw message box
 * @param message message to draw
 * @param r       region
 * @param offset  offset
 * @param ft      foreground color
 * @param bg      background color
 */
void messageBox (const char * message, region_t r,
                    position_t off, color_t fg, color_t bg)
{

    // check if the primary surface is available
    if (primary == NULL) {
        return;
    }

    // check if the font is available
    if (font    == NULL) {
        return;
    }
    
    // lock font
    pthread_mutex_lock(&fontLock);

    // once unset font 
    DFBCHECK(primary->SetFont(primary, NULL));

    // create a surface    
    IDirectFBSurface *    s;
    DFBSurfaceDescription d;
    d.flags       = DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT;
    d.pixelformat = DSPF_ARGB;
    d.width       = r.w;
    d.height      = r.h;
    DFBCHECK(dfb->CreateSurface(dfb, &d, &s));
    DFBCHECK(s->SetFont(s, font));
    DFBCHECK(s->SetDrawingFlags(s, DSDRAW_BLEND));

    // fill background
    DFBCHECK(s->Clear(s, bg.r, bg.g, bg.b, bg.a));

    // set foreground color
    DFBCHECK(s->SetColor(s, fg.r, fg.g, fg.b, fg.a));
    
    // render message
    DFBCHECK (s->DrawString(s, message, -1,
                              off.x, d.height + off.y, DSTF_LEFT));

    // blit
    DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_BLEND_ALPHACHANNEL));
    DFBCHECK(primary->Blit(primary, s, NULL, r.x, r.y));
    DFBCHECK(primary->SetBlittingFlags(primary, DSBLIT_NOFX));

    // release
    DFBCHECK(s->SetFont(s, NULL));
    s->Release(s);

    // set font back to the primary surface
    DFBCHECK(primary->SetFont(primary, font));

    // unlock
    pthread_mutex_unlock(&fontLock);

}


/**
 * Play music
 * @param path path to the music file
 * @param back play the music in back ground on true
 */
void playMusic (const char * path, bool back)
{

    // check if the font has already set
    pthread_mutex_lock(&playLock);
    if (playNow) {
        goto out;
    }

    // check file path size
    int pathlen  = strnlen(path, MAXPATHSTR);
    if (pathlen >= MAXPATHSTR) {
        goto out;
    }

    // check file type
    char * command;
    if (strcmp(path + pathlen - 3, "wav") == 0) {
        command = aplay;
    } else
    if (strcmp(path + pathlen - 3, "mp3") == 0) {
        command = mpg123;
    } else {
        goto out;
    }

    // make command string
    sprintf(musicCommand, "%s %s ", command, path);

    // play the music
    playNow = true;        
    int rtn = pthread_create(&playth, NULL, _playMusic, musicCommand);
    if (rtn < 0) {
        fprintf(stderr, "Failed to start the thread to play music.\n");
        playNow = false;
        goto out;
    }

    // record time
    playtime = time(NULL);

    // sync if no background
    if (! back) {
        pthread_join(playth, NULL);
    }

out:
    pthread_mutex_unlock(&playLock);
    return;

}


/**
 * Check if music is still going
 * @return -1 if not, past seconds otherwise
 */
int isPlaying (void)
{

    pthread_mutex_lock(&playLock);
    if (! playNow) {
        pthread_mutex_unlock(&playLock);
        return -1;
    }
    pthread_mutex_unlock(&playLock);

    return (int)((time(NULL) - playtime));

}


/**
 * Stop music
 */
void stopMusic (void)
{

    // check if the music is being played
    pthread_mutex_lock(&playLock);
    if (! playNow) { 
        pthread_mutex_unlock(&playLock);
        return;
    }
    pthread_mutex_unlock(&playLock);

    // stop all the musics
    system("killall -9 aplay  1> /dev/NULL 2> /dev/NULL");
    system("killall -9 mpg123 1> /dev/NULL 2> /dev/NULL");

    //
    pthread_join(playth, NULL);

}


/**
 * Start server connection
 * @param port port number to listen
 * @param backlog number of backlogs
 * @return server on success, NULL on failure
 */
server_t * startServer (int port, int backlog)
{

    int                     rtn;
    int                     sfd;
    struct sockaddr_in      addr;

    // open socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        return NULL;
    }

    // set socket option
    int opt = 1;
    rtn = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (rtn == -1) {
        return NULL;
    }

    // bind
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((short)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    rtn = bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rtn == -1) {
        return NULL;
    }

    // listen
    rtn = listen(sfd, backlog);
    if (rtn == -1) {
        return NULL;
    }

    server_t * serv = malloc(sizeof(server_t));
    if (serv == NULL) {
        return NULL;
    }

    serv->addr    = addr;
    serv->sfd     = sfd;
    serv->backlog = backlog;

    return serv;

}


/**
 * Wait for a request to connect from clients
 * @param serv server
 * @return connection with the client, NULL on failure
 */
connection_t * waitClient (server_t * server)
{

    int                     sfd;
    struct sockaddr_in      paddr;
    socklen_t               len;
    
    memset(&paddr, 0, sizeof(paddr));
    len = sizeof(paddr);

    sfd = accept(server->sfd, (struct sockaddr *) &paddr, &len);
    if (sfd == -1) {
        return NULL;
    }

    connection_t * conn = malloc(sizeof(connection_t));
    if (conn == NULL) {
        return NULL;
    }

    conn->addr  = paddr;
    conn->sfd   = sfd;
 
    return conn;    

}


/**
 * Connect to server
 * @param backlog number of backlogs
 * @param port server port number
 * @return connection on success, NULL on failure
 */
connection_t * connectServer (const char * ipaddr, int port)
{

    int                     rtn;
    int                     sfd;
    struct sockaddr_in      addr;

    // open socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        return NULL;
    }

    // configure server address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((short)port);
    rtn = inet_aton(ipaddr, &addr.sin_addr);
    if (rtn == 0) {
        return NULL;
    }
        
    // connect
    rtn = connect(sfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rtn == -1) {
        return NULL;
    }

    connection_t * conn = malloc(sizeof(connection_t));
    if (conn == NULL) {
        return NULL;
    }

    conn->addr    = addr;
    conn->sfd     = sfd;

    return conn;

}


/**
 * Send data to peer
 * @param conn connection to use
 * @param data data to send
 * @param size sending size
 * @return sent bytes on success, -1 on failure
 */
int sendData (connection_t * conn, const char * data, int size)
{

    return send(conn->sfd, data, size, 0);

}


/**
 * Receive data from peer
 * @param conn connection to use
 * @param buffer buffer to store the received data
 * @param max buffer size
 * @return received bytes on success, -1 on failure
 */
int recvData (connection_t * conn, char * buffer, int max)
{

    return recv(conn->sfd, buffer, max, 0);

}


/**
 * Close server connection
 * @param server server whose connection to close
 */
void closeServer (server_t * server)
{

    close(server->sfd);
    free (server);

}



/**
 * Close connection
 * @param conn connection to close
 */
void closeConnection (connection_t * conn)
{

    close(conn->sfd);
    free (conn);

}



/**
 * Start UDP server connection
 * @param port port number to listen
 * @return server on success, NULL on failure
 */
server_t * udpServer (int port)
{

    int                     rtn;
    int                     sfd;
    struct sockaddr_in      addr;

    // open socket
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) {
        return NULL;
    }

    // set socket option
    int opt = 1;
    rtn = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (rtn == -1) {
        return NULL;
    }
    opt = 1;
    rtn = setsockopt(sfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (rtn == -1) {
        return NULL;
    }

    // bind
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((short)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    rtn = bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rtn == -1) {
        return NULL;
    }

    server_t * serv = malloc(sizeof(server_t));
    if (serv == NULL) {
        return NULL;
    }

    serv->addr    = addr;
    serv->sfd     = sfd;
    serv->backlog = 0;

    return serv;

}



/**
 * Open UDP socket and prepare
 * @param ipaddr ipaddr to send in string
 * @param port   port to send
 * @return UDP socket on success, NULL on failure
 */
udpsocket_t * udpSocket (const char * ipaddr, int port)
{

    int                     rtn;
    int                     sfd;
    struct sockaddr_in      addr;
    udpsocket_t *           usock;

    // open socket
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1) {
        return NULL;
    }

    // set socket option
    int opt = 1;
    rtn = setsockopt(sfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (rtn == -1) {
        return NULL;
    }

    // prepare the destination address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((short)port);
    rtn = inet_aton(ipaddr, &addr.sin_addr);
    if (rtn == 0) {
        return NULL;
    }

    usock = (udpsocket_t *) malloc(sizeof(udpsocket_t));
    if (usock == NULL) {
        return NULL;
    }

    usock->addr   = addr;
    usock->sfd    = sfd;

    return usock;

}



/**
 * Send UDP datagram
 * @param usock UDP socket
 * @param data  data to send
 * @param size  sending size
 * @return sent bytes on success, -1 on failure
 */
int sendDataTo (udpsocket_t * usock, const char * data, int size)
{

    return sendto(usock->sfd, data, size, 0,
                (struct sockaddr *)&usock->addr, sizeof(struct sockaddr_in));

}


/**
 * Receive UDP datagram
 * @param serv   receiving UDP server
 * @param buffer buffer to store the received data
 * @param max    buffer size
 * @return received bytes on success, -1 on failure
 */
int recvDataFrom (server_t * serv, char * buffer, int max)
{

    socklen_t               len = sizeof(struct sockaddr_in);

    memset(&serv->sender, 0, len);
    return recvfrom(serv->sfd, buffer, max, 0,
                (struct sockaddr *)&serv->sender, &len);

}


