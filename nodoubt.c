/* ------------------------------------------------------------------------- */
/*  $Id: nodoubt.c,v 1.23 2005/11/26 07:37:09 pbui Exp pbui $    */
/* ------------------------------------------------------------------------- */

/*  Copyright (c) Peter Bui. All Rights Reserved.
 *  
 *  For specific licensing information, please consult the COPYING file that
 *  comes with the software distribution.  If it is missing, please contact the
 *  author at peter.j.bui@gmail.com.   */

/* ------------------------------------------------------------------------- */

#include "nodoubt.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dirent.h>
#include <getopt.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ------------------------------------------------------------------------- */
/*  Globals */
/* ------------------------------------------------------------------------- */

static int g_port_number;	    /*  Port Number */
static int g_queue_length;	    /*  Queue Length	*/
static int g_chunk_size;	    /*  Chunk Size  */
static int g_verbosity;		    /*  Verbosity Level	*/
static int g_background;	    /*  Background Flag	*/
static int g_timeout;		    /*	Timeout (Sec)	*/

static char * g_log_file;	    /*  Log File    */
static char * g_directory;	    /*  Directory   */

static char * g_buffer;		    /*	Buffer	*/

static ssn_t ** g_sessions;	    /*  Sessions    */

/* ------------------------------------------------------------------------- */
/*  Main */
/* ------------------------------------------------------------------------- */

/*  TODO:   Error handling system is very poor, and I really cut corners.   */

/*  TODO:   ACK! We are vulernable to sneaky bastards who request from the
 *  socket, but don't ever read... fixes: select on write fd and session time
 *  outs  */

int 
main		( int argc, char *argv[] )
{
    int n;	/*  Number of Active Sockets	*/
    int oldopts;/*  Old Options			*/
    int ss;	/*  Server Socket		*/
    fd_set rs;  /*  Set of Reading Sockets	*/
    fd_set ws;  /*  Set of Writing Sockets	*/
    struct timeval timeout;	/*  Timeout */

    /*	Initialize Program  */
    init(argc, argv);
    
    /*	Open Server Socket  */
    if ((ss = ssocket(g_port_number, g_queue_length)) < 0)	
	error(ERR_SS, true);

    /*	Make Socket Non-Blocking    */
    oldopts = fcntl(ss, F_GETFL, 0);
    if (fcntl(ss, F_SETFL, oldopts | O_NONBLOCK) < 0)
	error(ERR_SS, true);

    /*	Output Port Number  */
    if (g_verbosity) 
	printf("Opened Server Socket on Port %d\n", g_port_number);

    /*  Clear FDSets	*/
    FD_ZERO(&rs);
    FD_ZERO(&ws);

    /*	Main Loop   */
    while (true) {
	/*  Set Server Socket In Reading FD Sets  */
	FD_SET(ss, &rs);
	
	/*  Set Timeout	*/
	timeout.tv_sec  = g_timeout;
	timeout.tv_usec = 0;

	if (g_verbosity > 1)
	    printf("Waiting on Select\n");
    
	/*  Check Sockets For Messages	*/
	if ((n = select(FD_SETSIZE, &rs, &ws, NULL, &timeout)) == 0)
	    continue;
	else if (n < 0 && errno != EINTR) 
	    error(ERR_SL, true);
	
	/*  Check Server Socket */
	fd_chk_server(ss, &rs);
	
	/*  Check Session Sockets */
	fd_chk_sessions(&rs, &ws);

	/*  Push Out Anything in the I/O Buffers    */
	if (g_verbosity)	
	    fflush(stdout);
    }

    exit(EXIT_SUCCESS);
}

/* ------------------------------------------------------------------------- */
/*  File Descriptor Set	*/
/* ------------------------------------------------------------------------- */
	
void
fd_chk_server	(int ss, fd_set * rs)
{
    int cs;			/*  Client Socket   */
    size_t i;			/*  Index   */
    struct sockaddr_in csa;	/*  Client Socket Address   */
    socklen_t csalen;		/*  Client Socket Address Length    */

    /*	Check If There Is An Incoming Request	*/
    if (FD_ISSET(ss, rs)) {
	
	/*  Initialize Address Structure    */
	csalen = sizeof(csa);
	memset(&csa, 0, csalen); 

	/*  Accept Client Connection    */
	if ((cs = accept(ss, (struct sockaddr *)&csa, &csalen)) > 0) {
	    
	    if (g_verbosity) {
		time_t tm = time(NULL);
		struct hostent* ht = gethostbyaddr((char *)&csa.sin_addr,
						    sizeof(csa.sin_addr),
						    AF_INET);

		printf("- %s", asctime(localtime(&tm)));
		printf("Accepted Connection From %s AKA %s\n", 
			inet_ntoa(csa.sin_addr), ht->h_name );
	    }

	    /*	Search For Free Session	*/
	    for ( i = 0; i < g_queue_length; i++ )
		if ( g_sessions[ i ] == NULL )
		    break;

	    /*	Close Connection If No Free Slots, Otherwise Add to List    */
	    if ( i == g_queue_length ) {
		close( cs );
	    }
	    else {
		g_sessions[ i ] = ssn_init( cs );
		FD_SET( g_sessions[ i ]->ssn_socket, rs );
	    }
	} else {
	    if (g_verbosity > 1)
		printf("Connection Failure: %s\n", strerror(errno));
	}
    }
}

/* ------------------------------------------------------------------------- */

void
fd_chk_sessions	(fd_set * rs, fd_set * ws)
{
    size_t i;	/*  Index   */
	
    /*  Check Client Sockets    */
    for (i = 0; i < g_queue_length; i++) {
	/*  Skip Non-existant Session	*/
	if (g_sessions[ i ] == NULL) 
	    continue;
    
	/*  Read Socket When Appropriate    */
	if (g_sessions[i]->ssn_state == SSN_INIT && 
	    FD_ISSET(g_sessions[i]->ssn_socket, rs)) {
	    if (g_verbosity)
		printf("Reading Socket in Session[ %d ]\n", i);
	    
	    ssn_process_init(g_sessions[i]);

	    if (g_sessions[i]->ssn_state == SSN_SEND) {
		FD_CLR(g_sessions[i]->ssn_socket, rs);
		FD_SET(g_sessions[i]->ssn_socket, ws);
	    }

	    g_sessions[i]->ssn_timeout = false;
	}

	/*  Push Chunks When Appropriate */
	else if (g_sessions[i]->ssn_state == SSN_SEND &&
		 FD_ISSET( g_sessions[i]->ssn_socket, ws)) {
	    if (g_verbosity > 1)
		printf("Pushing Chunks in Session[ %d ]\n", i);
	    
	    ssn_process_send(g_sessions[i]);
	    g_sessions[i]->ssn_timeout = false;
	} else {
	    g_sessions[i]->ssn_timeout = true;
	}

	/*  Make Sure We Write	*/
	if (g_sessions[i]->ssn_state == SSN_SEND)  
	    FD_SET(g_sessions[i]->ssn_socket, ws);

	/*  Remove Appropriate Sessions */
	if (g_sessions[i]->ssn_state == SSN_FINISHED || 
	    g_sessions[i]->ssn_state == SSN_ERROR ||
	    g_sessions[i]->ssn_timeout) {

	    if (g_verbosity) printf("Ending Session[ %d ]\n", i);

	    FD_CLR(g_sessions[i]->ssn_socket, rs);
	    FD_CLR(g_sessions[i]->ssn_socket, ws);

	    ssn_end(g_sessions[i]);
	    g_sessions[i] = NULL;
	}
    }
}

/* ------------------------------------------------------------------------- */
/*  HTTP    */
/* ------------------------------------------------------------------------- */

char *
http_get_file	(char * buffer)
{
    char * sptr;    /*	String Pointer	*/
    char * file;    /*	Filename    */
    
    /*  Remove Protocol Information	*/
    if ( (sptr = strstr( buffer, " " )) == NULL ) 
	return ( NULL );

    *sptr = 0;

    /*	Prevent Sneaky //etc	*/
    for ( sptr = buffer; *sptr == '/'; sptr++ )
	;

    if ( *sptr == 0 )
	sptr--;

    sptr = file = strdup( sptr );

    /*  Convert Spaces */
    while ( (sptr = strstr( sptr, "%20" )) != NULL ) {
	*sptr++ = ' ';
	strcpy( sptr, (sptr + 2) );
    }

    return ( file );
}

/* ------------------------------------------------------------------------- */

char *
http_get_url	( char * buffer )
{
    char * sptr;    /*	String Pointer	*/
    char * url;	    /*	URL */
    size_t bsize;   /*	Buffer Size */

    /*	Check For Spaces    */
    bsize = strlen( buffer );

    if ( strlen( (sptr = strtok( buffer, " " )) ) == bsize )
	return strdup( buffer );

    /*	Allocate Buffer	*/
    if ( (url = malloc( sizeof( char ) * 1024 )) == NULL )
	return ( NULL );

    memset( url, 0, sizeof( char ) * 1024 );

    /*	Convert Spaces	*/
    while ( true ) {
	strcat( url, sptr );

	if ( (sptr = strtok( NULL, " " )) != NULL )
	    strcat( url, "%20" );
	else
	    break;
    }
    
    return ( url );
}

/* ------------------------------------------------------------------------- */

bool
http_setup_trans( ssn_t * ssn )
{
    char * sptr;    /*	String Pointer	*/

    /*  Check For Sneaky ../ File Locations */
    if ( (sptr = strstr( ssn->ssn_file, "../" )) != NULL ) {
	fprintf( ssn->ssn_sout, "HTTP/1.0 403 FORBIDDEN\r\n\r\n" );
	fflush( ssn->ssn_sout );
	return ( false );
    }

    switch ( ssn->ssn_type ) {
	case SSN_FILE:
	    return ( http_trans_file( ssn ) );
	    break;
	case SSN_DIR:
	    return ( http_trans_dir( ssn ) );
	    break;
	default:
	    return ( false );
	    break;
    }
}

/* ------------------------------------------------------------------------- */

void
http_strip_crnl	( char * buffer )
{
    size_t bsize;	/*  Buffer Size	*/

    if ( buffer == NULL )
	return;

    /*	Strip Return and Newline    */
    if ( (bsize = strlen( buffer )) ) {
	if ( buffer[ bsize - 2 ] == '\r' )
	    buffer[ bsize - 2 ] = 0;

	if ( buffer[ bsize - 1 ] == '\n' )
	    buffer[ bsize - 1 ] = 0;
    }
}

/* ------------------------------------------------------------------------- */

bool
http_trans_dir	( ssn_t * ssn )
{
    char * path;		/*  File Path	*/
    char * url;			/*  URL		*/
    size_t bsize;		/*  Buffer Size	*/
    double fsize;		/*  File Size	*/
    char   fsymbol;		/*  File symbol	*/
    char   funits[4] = {'B', 'K', 'M', 'G'};   /*  File Units	*/
    int	   fuindex;		/*  File Unit Index */
    
    int	   dindex;		/*  Directory Index	*/
    int	   dsize;		/*  Directory Size	*/
    struct dirent **dfiles;	/*  Directory Files	*/
    struct stat	fstat;		/*  File Statistics	*/
    
    /*	Check For Root	*/
    if ( strcmp( ssn->ssn_file, "/" ) == 0 )
	path = getcwd( NULL, 0 );
    else
	path = strdup( ssn->ssn_file );

    /*	Scan and Sort Directory	*/
    if ( (dsize = scandir( path, &dfiles, NULL, alphasort )) < 0 ) {
	free( dfiles );
	free( path );
	return ( false );
    }
    
    /*	Correct Directory Name	*/
    bsize = strlen( ssn->ssn_file );
    if ( ssn->ssn_file[ bsize - 1 ] == '/' )
	ssn->ssn_file[ bsize - 1 ] = 0;
    
    /*  Setup Response  */
    fprintf( ssn->ssn_sout, "HTTP/1.0 200 OK\r\n" );
    fprintf( ssn->ssn_sout, "Connection: Closed\r\n" );
    fprintf( ssn->ssn_sout, "Content-Type: text/html\r\n" );
    fprintf( ssn->ssn_sout, "\r\n\r\n" );
    
    /*	Output HTML Banner  */
    fprintf( ssn->ssn_sout, 
	     "<html><title>/%s</title><body><b>/%s</b><br/><hr/>",
	     ssn->ssn_file, ssn->ssn_file );

    /*	Output Table	*/
    fprintf( ssn->ssn_sout, 
	     "<table cellpadding=\"3\"><tr>"
	     "<td colspan=\"2\"><b>Filename</b></td>"
	     "<td>&nbsp;</td>"
	     "<td><b>Size</b></td></tr>");

    /*	Output Each Directory Element	*/
    for ( dindex = 0; dindex < dsize; dindex++ ) {
	/*  Get File Statistics	*/
	if ( dfiles[ dindex ]->d_type == DT_REG ) {
	    sprintf( g_buffer, "%s/%s", path, dfiles[ dindex ]->d_name );
	    stat( g_buffer, &fstat );
	}

	/*  Get URL */
	sprintf( g_buffer, "%s%s/%s",
		 strlen( ssn->ssn_file ) ? "/" : "",
		 ssn->ssn_file,
		 dfiles[ dindex ]->d_name );

	url = http_get_url( g_buffer );

	/*  Output File Information	*/
	switch ( dfiles[ dindex ]->d_type ) {
	    case DT_DIR:    
		fsymbol = '/';
		fsize = 0;
		break;
	    case DT_LNK:    
		fsymbol = '@';
		fsize = 0;
		break;
	    default:	
		fsymbol = 0;
		fsize = (double)(fstat.st_size);
		break;
	}
	
	for (fuindex = 0; fuindex < 4; fuindex++) {
	    if (fsize > 1024.0)
		fsize /= 1024.0;
	    else
		break;
	}
	
	fprintf(ssn->ssn_sout, 
		"<tr><td>%d.</td>" 
		"<td><a href=\"%s\">%s</a>%c</td>"
		"<td></td>" 
		"<td>%0.2lf %c</td></tr>",
		(dindex+1),
		url,
		dfiles[dindex]->d_name,
		fsymbol,
		fsize, funits[fuindex]);


	free( url );
    }

    /*	Output HTML */
    fprintf( ssn->ssn_sout, "</table><hr/><i>%s - %s - %s</i></body></html>",
	     PROG_NAME, PROG_VERSION, PROG_DESCRIP );
  
    /*	Push Out Socket Stream	*/
    fflush( ssn->ssn_sout );
   
    /*	Clean Up    */
    free( path );
    free( dfiles );

    /*	Signal End of Session */
    ssn->ssn_state = SSN_FINISHED;
    return ( false );
}

/* ------------------------------------------------------------------------- */

bool
http_trans_file	( ssn_t * ssn )
{
    char * sptr;    /*	String Pointer	*/
    size_t i;	    /*	Index	*/
    struct stat st; /*	File Statistics	*/

    /*  Open File Input File Stream	*/
    if ( (ssn->ssn_fin = fopen( ssn->ssn_file, "r" )) == NULL ) {
	fprintf( ssn->ssn_sout, "HTTP/1.0 404 NOT FOUND\r\n\r\n" );
	fflush( ssn->ssn_sout );
	return ( false );
    }

    /*  Get File Size   */
    if ( stat( ssn->ssn_file, &st ) < 0 ) {
	fprintf( ssn->ssn_sout, "HTTP/1.0 404 NOT FOUND\r\n\r\n" );
	fflush( ssn->ssn_sout );
	return ( false );
    }

    ssn->ssn_fsize = st.st_size;

    if ( g_verbosity > 1 )
	printf( "Filesize %lld\n", (long long)ssn->ssn_fsize );

    /*  Setup Response  */
    fprintf( ssn->ssn_sout, "HTTP/1.0 200 OK\r\n" );
    fprintf( ssn->ssn_sout, "Connection: Closed\r\n" );

    /*	Find File Extension */
    sptr = strchr( ssn->ssn_file + strlen( ssn->ssn_file ) - 5, '.' );

    if ( sptr ) 
	sptr++;
    else
	goto htf_noext;
    
    /*	Look For Content-Type	*/
    for ( i = 0; strlen( MIME_STRS[ i ] ); i += 2 ) {
	if ( strcmp( sptr, MIME_STRS[ i ] ) == 0 ) {
	    fprintf( ssn->ssn_sout, "Content-Type: %s", MIME_STRS[ i + 1 ] );

	    if ( g_verbosity > 1 )
		printf( "Content-Type %s\n", MIME_STRS[ i + 1 ] );
	   
	    goto htf_success;
	}
    }

    /*	Default MIME-Type   */
htf_noext:
    fprintf( ssn->ssn_sout, "Content-Type: text/plain" );
htf_success: 
    fprintf( ssn->ssn_sout, "\r\n" );
    fprintf( ssn->ssn_sout, "Content-Length: %lld", (long long)ssn->ssn_fsize );
    fprintf( ssn->ssn_sout, "\r\n\r\n" );
    fflush( ssn->ssn_sout );
    return ( true );
}

/* ------------------------------------------------------------------------- */
/*  Initialization  */
/* ------------------------------------------------------------------------- */

void	    
init	    ( int argc, char * argv[] )
{
    /*	Set Defaults	*/
    init_defaults();
    
    /*	Check Arguments	*/
    init_arguments( argc, argv );

    /*	Check Background Flag	*/
    if ( g_background )	    
	init_background();

    /*	Check Log File	*/
    if ( g_log_file )
	init_log_file();
    
    /*	Initialize Sessions */
    if ( (g_sessions = malloc( sizeof( ssn_t * ) * g_queue_length )) == NULL )
	error( ERR_SN, true );
    
    memset( g_sessions, 0, sizeof( ssn_t * ) * g_queue_length );

    /*	Change to Directory */
    if ( chdir( g_directory ) < 0 )
	error( ERR_DR, true );
    
    /*	Setup Signal Handlers	*/
    signal( SIGINT, sig_handler );
    signal( SIGKILL, sig_handler );
    signal( SIGPIPE, SIG_IGN );
    
    /*	Allocate Chunk Buffer	*/
    if (( g_buffer = malloc( sizeof( char ) * g_chunk_size )) == NULL )
	error( ERR_SN, true );

    /*	Output Globals	*/
    if ( g_verbosity ) {
	printf( "Port Number:  %d\n", g_port_number );
	printf( "Queue Length: %d\n", g_queue_length );
	printf( "Chunk Size:   %d\n", g_chunk_size );
	printf( "Timeout:      %d\n", g_timeout );
	printf( "Log File:     %s\n", g_log_file);
	printf( "Directory:    %s\n", g_directory );
	printf( "Verbosity:    %d\n", g_verbosity );
	printf( "Background:   %d\n", g_background );
    }
}

/* ------------------------------------------------------------------------- */

void	    
init_arguments	( int argc, char * argv[] )
{
    int c;  /*  GetOpt Key	*/
    
    /*	Process Command Line Arguments	*/
    while ( (c = getopt( argc, argv, "bhc:d:l:p:q:t:v:" )) > 0 ) {
	switch ( c ) {
	    case 'b':
		g_background = true;
		break;
	    case 'h':
		usage();
		exit( EXIT_SUCCESS );
		break;
	    case 'c':
		g_chunk_size = atoi( optarg );
		break;
	    case 'd':
		g_directory = optarg;
		break;
	    case 'l':
		g_log_file = optarg;
		break;
	    case 'p':
		g_port_number = atoi( optarg );
		break;
	    case 'q':
		g_queue_length = atoi( optarg );
		break;
	    case 't':
		g_timeout = atoi( optarg );
		break;
	    case 'v':
		g_verbosity = atoi( optarg );
		break;
	    default:
		usage();
		exit( EXIT_FAILURE );
		break;
	}
    }
}

/* ------------------------------------------------------------------------- */

void
init_background	()
{
    pid_t pid;	/*  Process Id	*/

    if ( (pid = fork()) > 0 ) 
	exit( EXIT_SUCCESS );
    else if ( pid < 0 ) 
	error( ERR_BG, true );
}

/* ------------------------------------------------------------------------- */

void
init_defaults	()
{
    g_port_number     = PORT_NUMBER;
    g_chunk_size      = CHUNK_SIZE;
    g_queue_length    = QUEUE_LENGTH;
    g_log_file	      = (char *)LOG_FILE;
    g_directory	      = (char *)DIRECTORY;
    
    g_background      = BACKGROUND;
    g_verbosity	      = VERBOSITY;
    
    g_timeout	      = TIMEOUT;
}

/* ------------------------------------------------------------------------- */

void
init_log_file	()
{
    int lfd;	/*  Log File File Descriptor    */
    
    /*	Open Log File	*/
    if ( (lfd = open( g_log_file, O_RDWR | O_CREAT | O_APPEND, 0664 )) < 0 )
        error( ERR_LF, true );
   
    /*	Dup Stout and Stderr to Log File    */
    if ( dup2( lfd, 1 ) < 0 )
        error( ERR_LF, true );

    if ( dup2( lfd, 2 ) < 0 )
        error( ERR_LF, true );
}

/* ------------------------------------------------------------------------- */
/*  Error   */
/* ------------------------------------------------------------------------- */

void	    
error		( const int err, const int perr )
{
    /*	Print Error Message */
    if ( err < ERR_ERRORS_N ) {
	if ( perr )
	    fprintf( stderr, "[ERROR] %s: %s\n", 
		     ERR_STRS[ err ], strerror( errno ) );
	else
	    fprintf( stderr, "[ERROR]%s\n", ERR_STRS[ err ] );
    }

    /*	Quit Program	*/
    exit( EXIT_FAILURE );
}

/* ------------------------------------------------------------------------- */
/*  Session */
/* ------------------------------------------------------------------------- */

ssn_t	*   
ssn_init	( const int cs )
{
    ssn_t * tssn;   /*	Temporary Session   */

    /*	Allocate Memory	*/
    if ( (tssn = malloc( sizeof( ssn_t ))) == NULL )
	return NULL;

    /*	Zero Out Memory	*/
    memset( tssn, 0, sizeof( ssn_t ) );

    /*	Open Socket Input File Stream	*/
    if ( (tssn->ssn_sin = fdopen( cs, "r" )) < 0 ) {
	ssn_end( tssn );
	return NULL;
    }
    
    /*	Open Socket Output File Stream	*/
    if ( (tssn->ssn_sout = fdopen( cs, "w" )) < 0 ) {
	ssn_end( tssn );
	return NULL;
    }

    /*	Set Socket File Descriptor  */
    tssn->ssn_socket = cs;

    return ( tssn );
}

/* ------------------------------------------------------------------------- */

void	    
ssn_end		( ssn_t * ssn )
{
    if ( ssn == NULL )	    
	return;
    
    /*	Close File Input Stream */
    if ( ssn->ssn_fin != NULL )
	fclose( ssn->ssn_fin );

    /*	Close Socket Input Stream */
    if ( ssn->ssn_sin != NULL )
	fclose( ssn->ssn_sin );

    /*	Close Socket Output Stream */
    if ( ssn->ssn_sout != NULL ) {
	fclose( ssn->ssn_sout );
    }

    /*	Free File Name	*/
    if ( ssn->ssn_file != NULL )
	free( ssn->ssn_file );

    /*	Free Session Structure	*/
    free( ssn );
}

/* ------------------------------------------------------------------------- */

void
ssn_process_init    (ssn_t * ssn)
{
    DIR * dir;	    /*	Directory   */

    /*	Read In From Socket Stream  */
    if (fgets(g_buffer, g_chunk_size, ssn->ssn_sin) == NULL )
	goto spi_error;

    /*	Strip Return and Newline    */
    http_strip_crnl(g_buffer);

    /*	Check HTTP GET Request   */
    if (strncmp( g_buffer, "GET ", 4) == 0) {
	/*  Get Filename   */
	if ((ssn->ssn_file = http_get_file(g_buffer + 5)) == NULL)
	    goto spi_error;

	if (g_verbosity)
	    printf("Request: %s\n", ssn->ssn_file);

	/*  Check For Directory Request */
	if ((dir = opendir(ssn->ssn_file))) {
	    if (g_verbosity)
		printf("Directory Request Detected\n");
	   
	    ssn->ssn_type = SSN_DIR;
	    closedir(dir);
	}

	if (g_verbosity > 1)
	    printf("Filename [%s]\n", ssn->ssn_file);

	/*  Read Rest of Request    */
	do {
	    /*	Read In From Socket Stream  */
	    if (fgets(g_buffer, g_chunk_size, ssn->ssn_sin) == NULL)
		goto spi_error;

	    /*	Strip Return and Newline    */
	    http_strip_crnl(g_buffer);
	} while (strlen(g_buffer));

	/*  Setup Transmission	*/
	if (http_setup_trans(ssn))
	    ssn->ssn_state = SSN_SEND;
	else
	    ssn->ssn_state = SSN_ERROR;
	return;
    } 

spi_error:
    ssn->ssn_state = SSN_ERROR;
    return;
}

/* ------------------------------------------------------------------------- */

void
ssn_process_send    ( ssn_t * ssn )
{
    int ar;	    /*	Amount Read */
    int aw;	    /*	Amount Written	*/
    
    /*	Read In From File   */
    if ((ar = fread( g_buffer, 1, g_chunk_size, ssn->ssn_fin)) < 0) 
	goto sps_error;

    /*	Write Out To Socket */
    if ((aw = fwrite( g_buffer, 1, ar, ssn->ssn_sout )) != ar) 
	goto sps_error;

    if (g_verbosity > 2)
	printf("Wrote Out %s\n", g_buffer);

    /*	Check For End Of File	*/
    ssn->ssn_findex += aw;

    if (ssn->ssn_findex == ssn->ssn_fsize)
	ssn->ssn_state = SSN_FINISHED;

    /*	Push Out Data	*/
    fflush(ssn->ssn_sout);
    return;

sps_error:
    ssn->ssn_state = SSN_ERROR;
    return;
}

/* ------------------------------------------------------------------------- */
/*  Signal  */
/* ------------------------------------------------------------------------- */

void
sig_handler	    (int sig)
{
    size_t i;	/*  Index   */

    /*	End Every Session   */
    for (i = 0; i < g_queue_length; i++) {
	if (g_sessions[i] == NULL)
	    continue;

	ssn_end(g_sessions[i]);
    }

    free(g_sessions);

    /*	Free Buffer */
    if (g_buffer)
	free(g_buffer);

    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- */
/*  Socket  */
/* ------------------------------------------------------------------------- */

int	    
ssocket		(const int port, const int qlen)
{
    struct sockaddr_in sa_in;	/*  Socket Address  */
    int ss;			/*  Server Socket   */

    memset(&sa_in, 0, sizeof(sa_in));
   
    /*	Setup Binding Socket Address	*/
    sa_in.sin_family	  = AF_INET;
    sa_in.sin_addr.s_addr = INADDR_ANY;
    sa_in.sin_port        = htons(port);

    /*  Allocate Socket  */
    if ((ss = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        return (-1);

    /*	Bind Socket */
    if (bind(ss, (struct sockaddr *)&sa_in, sizeof(sa_in)) < 0 )
        return (-1);
    
    /*  Setup TCP/IP Queue  */
    if (listen(ss, qlen) < 0)
        return (-1);

    return (ss);
}

/* ------------------------------------------------------------------------- */
/*  Usage   */
/* ------------------------------------------------------------------------- */

void
usage		()
{
    /*	Output Banner	*/
    fprintf( stderr, "%s - %s - %s\n", PROG_NAME, PROG_VERSION, PROG_DESCRIP );
    fprintf( stderr, "Usage: %s [options...]\n\n", PROG_BIN );

    /*	Output Flags	*/
    fprintf( stderr, "Flags:\n" ); 
    fprintf( stderr, "  -b   Put program in the background\n" ); 
    fprintf( stderr, "  -h   This help message\n\n" ); 
    
    fprintf( stderr, "All flags are turned off by default.\n\n" ); 
    
    /*	Output Parameters   */
    fprintf( stderr, "Parameters:\n" ); 
    fprintf( stderr, "  -c   Chunk size     (Default is %d)\n", CHUNK_SIZE ); 
    fprintf( stderr, "  -d   Directory      (Default is %s)\n", DIRECTORY ); 
    fprintf( stderr, "  -l   Log file       (Default is %s)\n", LOG_FILE ); 
    fprintf( stderr, "  -p   Port number    (Default is %d)\n", PORT_NUMBER ); 
    fprintf( stderr, "  -q   Queue length   (Default is %d)\n", QUEUE_LENGTH ); 
    fprintf( stderr, "  -t   Timeout (Sec)  (Default is %d)\n", TIMEOUT ); 
    fprintf( stderr, "  -v   Verbosity      (Default is %d)\n\n", VERBOSITY ); 
    
    /*	Output Contact Information  */
    fprintf( stderr, "Please send any flames to %s\n", PROG_AUTHOR ); 
}

/* ------------------------------------------------------------------------- */
