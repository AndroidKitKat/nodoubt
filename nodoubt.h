/* ------------------------------------------------------------------------- */
/*  $Id: nodoubt.h,v 1.14 2005/07/20 20:00:57 pbui Exp pbui $    */
/* ------------------------------------------------------------------------- */

/*  Copyright (c) Peter Bui. All Rights Reserved.
 *  
 *  For specific licensing information, please consult the COPYING file that
 *  comes with the software distribution.  If it is missing, please contact the
 *  author at peter.j.bui@gmail.com.   */

/* ------------------------------------------------------------------------- */

#ifndef __NODOUBT_H__
#define __NODOUBT_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>

/* ------------------------------------------------------------------------- */
/*  Constants	*/
/* ------------------------------------------------------------------------- */

/*  Program Information	*/

static const char * PROG_NAME    = "No Doubt";
static const char * PROG_BIN     = "nodoubt";
static const char * PROG_VERSION = "Build " _DATE;
static const char * PROG_AUTHOR  = "Peter Bui <peter.j.bui@gmail.com>";
static const char * PROG_DESCRIP = "Lame Web Server For Big Ass Files";

/* ------------------------------------------------------------------------- */

/*  Defaults	*/

static const int PORT_NUMBER	 = 8080;    /*  Port Number	*/
static const int QUEUE_LENGTH	 = 5;	    /*  Queue Length    */
static const int CHUNK_SIZE	 = 16384;   /*  File Chunk Size */
static const int TIMEOUT	 = 10;	    /*  Timeout (Sec)	*/
static const int BACKGROUND	 = 0;	    /*  Background Flag */
static const int VERBOSITY	 = 1;	    /*  Verbosity Level	*/

static const char * LOG_FILE	 = NULL;    /*  Log File    */
static const char * DIRECTORY	 = "./";    /*  Directory   */

/* ------------------------------------------------------------------------- */

/*  Session States  */

enum SSN_STATES {
    SSN_INIT = 0,   /*	Initial state, so must read request */
    SSN_SEND,	    /*	Sending state, pushing out chunks   */
    SSN_FINISHED,   /*	Finished state, so close connection */
    SSN_ERROR,	    /*	Error state, so close connection    */
    SSN_STATES_N
};

/* ------------------------------------------------------------------------- */

/*  Errors  */

enum ERR_ERRORS {
    ERR_SS,	    /*	Server Socket Failure	*/
    ERR_BG,	    /*	Backgrounding Failure	*/
    ERR_LF,	    /*	Log File Failure	*/
    ERR_FL,	    /*	File List Failure	*/
    ERR_SN,	    /*	Session Failure		*/
    ERR_DR,	    /*	Directory Failure	*/
    ERR_SL,	    /*	Select Failure		*/
    ERR_ERRORS_N
};

static const char * ERR_STRS[] = {
    "Server Socket Error",
    "Backgrounding Error",
    "Log File Error",
    "File List Error",
    "Session Error",
    "Directory Error",
    "Select Error",
    NULL
};

/* ------------------------------------------------------------------------- */

/*  MIME Types	*/

static const char * MIME_STRS[] = {
    "ogg",  "application/ogg",
    "ac3",  "audio/ac3",
    "aac",  "audio/aac",
    "mp3",  "audio/mpeg",
    "wav",  "audio/wav",
    "avi",  "video/x-msvideo",
    "wma",  "video/x-ms-asf",
    "wmv",  "video/x-msvideo",
    "divx", "video/x-msvideo",
    "xvid", "video/x-msvideo",
    "mpeg", "video/mpeg",
    "mpg",  "video/mpeg",
    "mp4",  "video/mpeg",
    "htm",  "text/html",
    "html", "text/html",
    "css",  "text/css",
    "pdf",  "application/pdf",
    "ps",   "application/postscript",
    ""
};

/* ------------------------------------------------------------------------- */

/*  Session Types   */

enum SSN_TYPES {
    SSN_FILE = 0,
    SSN_DIR,
    SSN_TYPES_N
};

/* ------------------------------------------------------------------------- */
/*  Data Structures */
/* ------------------------------------------------------------------------- */

/*  Session */

struct _ssn_t {
    FILE    *	ssn_fin;    /*  File Output Stream	*/	
    FILE    *	ssn_sin;    /*  Socket Input Stream	*/
    FILE    *	ssn_sout;   /*  Socket Output Stream	*/
    char    *	ssn_file;   /*  File Name		*/
    int		ssn_socket; /*  Socket File Descriptor	*/
    int		ssn_timeout;/*	Timeout			*/
    size_t	ssn_state;  /*  Session State	*/
    size_t	ssn_findex; /*  File Index  */
    off_t	ssn_fsize;  /*  File Size   */
    size_t	ssn_type;   /*	Session Type	*/
};

typedef struct _ssn_t ssn_t;

/* ------------------------------------------------------------------------- */
/*  Functions	*/
/* ------------------------------------------------------------------------- */

/*  File Descriptor Set	*/

void	    fd_chk_server   ( int ss, fd_set * rs );
void	    fd_chk_sessions ( fd_set * rs, fd_set * ws );

/* ------------------------------------------------------------------------- */

/*  Initialization  */

void	    init	    ( int argc, char * argv[] );

void	    init_arguments  ( int argc, char * argv[] );
void	    init_background ();
void	    init_defaults   ();
void	    init_log_file   ();

/* ------------------------------------------------------------------------- */

/*  Error   */

void	    error	    ( const int err, const int perr );

/* ------------------------------------------------------------------------- */

/*  HTTP    */

char	*   http_get_file   ( char * buffer );
char	*   http_get_url    ( char * buffer );
bool	    http_setup_trans( ssn_t * ssn );
void	    http_strip_crnl ( char * buffer );
bool	    http_trans_dir  ( ssn_t * ssn );
bool	    http_trans_file ( ssn_t * ssn );

/* ------------------------------------------------------------------------- */

/*  Session */

ssn_t	*   ssn_init	    ( const int cs ); 
void	    ssn_end	    ( ssn_t * ssn ); 

void	    ssn_process_init( ssn_t * ssn );
void	    ssn_process_send( ssn_t * ssn );

/* ------------------------------------------------------------------------- */

/*  Signal  */

void	    sig_handler	    ( int sig );

/* ------------------------------------------------------------------------- */

/*  Socket  */

int	    ssocket	    ( const int port, const int qlen ); 

/* ------------------------------------------------------------------------- */

/*  Usage   */

void	    usage	    ();

/* ------------------------------------------------------------------------- */

/*  Linux Sucks	*/

#ifdef __linux__
extern	    
FILE	*   fdopen	    ( int fildes, const char *mode );
#endif

/* ------------------------------------------------------------------------- */

#endif
