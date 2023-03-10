#include "FirstBlat.h"

// disabling optimizations here is intentional.  without this, the compiler doesn't seem to correctly write
// the boundary text string to the correct location - it's off by +4 bytes.  This can cause the boundary
// to contain premature NULs or 8-bit characters which really messes up the decode.
#pragma optimize("", off)

// disable VC8 warning "/GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function"
#pragma warning (disable: 4748)


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* generic socket DLL support */
#include "gensock\gensock.h"

#define VERSION "1.9.4"
#ifdef WIN32
    #define __far far
    #define huge far
    #define __near near
#endif

#define MAXOUTLINE 2048

extern BOOL DoCgiWork(int & argc, char**  &argv,LPSTR &lpszMessage,
                      LPSTR& lpszCgiSuccessUrl,LPSTR &lpszCgiFailureUrl,
                      LPSTR& lpszFirstReceivedData,LPSTR& lpszOtherHeader);

#ifdef GENSOCK_STATIC_LINK
HANDLE  dll_module_handle=0;
#else
HMODULE   gensock_lib = 0;
#endif

int (FAR PASCAL *pgensock_connect) (char FAR * hostname, char FAR * service, socktag FAR * pst);
int (FAR PASCAL *pgensock_getchar) (socktag st, int wait, char FAR * ch);
int (FAR PASCAL *pgensock_put_data) (socktag st, char FAR * data, unsigned long length);
int (FAR PASCAL *pgensock_close) (socktag st);
int (FAR PASCAL *pgensock_gethostname) (char FAR * name, int namelen);
int (FAR PASCAL *pgensock_put_data_buffered) (socktag st, char FAR * data, unsigned long length);
int (FAR PASCAL *pgensock_put_data_flush) (socktag st);

void base64_encode(const char *in, const char *endin, char *out);
int GetLengthConvertToQuotedPrintable(char *string, int inHeader);
void ConvertToQuotedPrintable(char ThisChar, int * CurrPos, char * buffer, int inHeader);
void fileGetSubject(char *fn);
void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison

socktag SMTPSock;
#define SERVER_SIZE 256     // #defines (bleah!) from Beverly Brown "beverly@datacube.com"
#define SENDER_SIZE 256
#define TRY_SIZE 20
#define SUBJECT_SIZE 1024
#define DEST_SIZE 1024
#define ORG_SIZE 512
#define TEXTMODE_SIZE   16      // added 15 June 1999 by James Greene "greene@gucc.org"
char SMTPHost[SERVER_SIZE+1];
char SMTPPort[SERVER_SIZE+1];
char AUTHLogin[SERVER_SIZE+1];
char AUTHPassword[SERVER_SIZE+1];
char Try[TRY_SIZE+1];
char Sender[SENDER_SIZE+1];
char Profile[TRY_SIZE+1];
char *Recipients;
char my_hostname[1024];
char *destination = 0;
char *cc_list = 0;
char *bcc_list = 0;
char loginname[SENDER_SIZE+1];                   // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage 
char senderid[SENDER_SIZE+1];                    // Inconsistent use in Blat for some RFC 822 Fielsd definitions
char sendername[SENDER_SIZE+1];                  // RFC 822 Sender: <sendername>
char fromid[SENDER_SIZE+1];                      // RFC 822 From: <fromid>
char replytoid[SENDER_SIZE+1];                   // RFC 822 Reply-To: <replytoid>
char returnpathid[SENDER_SIZE+1];                // RFC 822 Return-Path: <returnpath>
char subject[SUBJECT_SIZE+1];
char organization[ORG_SIZE+1];
char aheaders1[DEST_SIZE+1];
char aheaders2[DEST_SIZE+1];
char xheaders[DEST_SIZE+1];
char textmode[TEXTMODE_SIZE+1];                  // added 15 June 1999 by James Greene "greene@gucc.org"
int returnreceipt=0;
int disposition=0;
int mime=0;
int quiet=0;
int debug=0;
int uuencode=0;
int base64=0;
int attach=0;
int authplain=0;                                 // Added 27 Feb 2001 Craig Morrison AUTH PLAIN authentication
char attachfile[64][MAX_PATH+1];
char attachedfile[MAX_PATH+1];
char *shortname;
char my_hostname_wanted[1024]="";
FILE *logOut = (FILE *)NULL;
char charset[40]; 								// Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

WIN32_FIND_DATA FindFileData;

int attachtype[64];
int noheader=0;

const char *usage[]=
{
    "Blat v" VERSION ": WinNT/95 console utility to mail a file via SMTP",
    "by P.Mendes, M.Neal, G.Vollant, T. Charron",
    "  http://www.blat.net",
    "syntax:",
    "  Blat <filename> -to <recipient> [optional switches (see below)]",
//    "  Blat -install <server addr> <sender's addr> [<try>[<port>[<profile>]]] [-q]",
    "  Blat -profile [-delete | \"<default>\"] [profile1] [profileN] [-q]",
    "  Blat -h [-q]",
    "",
//    "-install <server addr> <sender's addr> [<try n times> [<port> [<profile>]]]",
//    "     : set's SMTP server, sender, number of tries and port for profile",
//    "       (<try n times> and <port> may be replaced by '-').",
//    "",
    "<filename>     : file with the message body ('-' for console input, end with ^Z)",
    "-to <recipient> : recipient list (also -t) (comma separated)",
    "-tf <recipient> : recipient list filename",
    "-subject <subj>: subject line (also -s)",
    "-f <sender>    : overrides the default sender address (must be known to server)",
    "-i <addr>      : a 'From:' address, not necessarily known to the SMTP server.",
    "-cc <recipient>: carbon copy recipient list (also -c) (comma separated)",
    "-cf <file>     : cc recipient list filename",
    "-bcc <recipient>: blind carbon copy recipient list (also -bcc) (comma separated)",
    "-bf <file>     : bcc recipient list filename",
    "-organization <organization>: Organization field (also -o and -org)",
    "-body <text>   : Message body",
    "-x <X-Header: detail>: Custom 'X-' header.  eg: -x \"X-INFO: Blat is Great!\"",
    "-r             : Request return receipt.",
    "-d             : Request disposition notification.",
    "-h             : displays this help.",
    "-q             : supresses *all* output.",
    "-debug         : Echoes server communications to screen (disables '-q').",
    "-noh           : prevent X-Mailer header from showing homepage of blat",
    "-noh2          : prevent X-Mailer header entirely",
    "-p <profile>   : send with SMTP server, user and port defined in <profile>.",
    "-priority <pr> : set message priority 0 for low, 1 for high.",
    "-server <addr> : Specify SMTP server to be used. (optionally, addr:port)",
    "-port <port>   : port to be used on the server, defaults to SMTP (25)",
    "-hostname <hst>: select the hostname used to send the message",
    "-mime          : MIME Quoted-Printable Content-Transfer-Encoding.",
    "-enriched      : Send an enriched text message (Content-Type=text/enriched)",    // 10. June 1999 by JAG
    "-html          : Send an HTML message (Content-Type=text/html)",    // 15. June 1999 by JAG 
    "-uuencode      : Send (binary) file UUEncoded",
    "-base64        : Send (binary) file using base64 (binary Mime)",
    "-try <n times> : how many time blat should try to send. from '1' to 'INFINITE'",
    "-attach <file> : attach binary file to message (may be repeated)",
    "-attacht <file>: attach text file to message (may be repeated)",
    "-ti <n>        : Set timeout to 'n' seconds.",
    "-u <username>  : Username for AUTH LOGIN (use with -pw)",
    "-pw <password> : Password for AUTH LOGIN (use with -u)",
    "-log <filename>: Log everything but usage to <filname>",
    "-plain         : Use AUTH PLAIN to login to SMTP server (use with -u -pw)",
	"-charset <cs>  : User defined charset.  The default is ISO-8859-1",
    "",
    "Note that if the '-i' option is used, <sender> is included in 'Reply-to:'",
    "and 'Sender:' fields in the header of the message.",
    "",
    "Optionally, the following options can be used instead of the -f and -i",
    "options:",
    "",
    "-mailfrom <addr>   The RFC 821 MAIL From: statement",
    "-from <addr>       The RFC 822 From: statement",
    "-replyto <addr>    The RFC 822 Reply-To: statement",
    "-returnpath <addr> The RFC 822 Return-Path: statement",
    "-sender <addr>     The RFC 822 Sender: statement",
    "",
    "For backward consistency, the -f and -i options have precedence over these",
    "RFC 822 defined options. If both -f and -i options are omitted then the ",
    "RFC 821 MAIL FROM statement will be defaulted to use the installation-defined ",
    "default sender address"
};
const size_t NMLINES=(sizeof(usage) / sizeof(char *));

void printUsage() {
	for (unsigned i=0;i<NMLINES;i++)
		printf("%s\n",usage[i]);
}

void
     gensock_error (const char * function, int retval)
{
    if ( ! quiet ) {
        switch ( retval ) {
        case 4001: printMsg("Error: Malloc failed (possibly out of memory)."); break;
        case 4002: printMsg("Error: Error sending data."); break;
        case 4003: printMsg("Error: Error initializing gensock.dll."); break;
        case 4004: printMsg("Error: Version not supported."); break;
        case 4005: printMsg("Error: The winsock version specified by gensock is not supported by this winsock.dll."); break;
        case 4006: printMsg("Error: Network not ready."); break;
        case 4007: printMsg("Error: Can't resolve (mailserver) hostname."); break;
        case 4008: printMsg("Error: Can't create a socket (too many simultaneous links?)"); break;
        case 4009: printMsg("Error: Error reading socket."); break;
        case 4010: printMsg("Error: Not a socket."); break;
        case 4011: printMsg("Error: Busy."); break;
        case 4012: printMsg("Error: Error closing socket."); break;
        case 4013: printMsg("Error: Wait a bit (possible timeout)."); break;
        case 4014: printMsg("Error: Can't resolve service."); break;
        case 4015: printMsg("Error: Can't connect to mailserver (timed out if winsock.dll error 10060)"); break;
        case 4016: printMsg("Error: Connection to mailserver was dropped."); break;
        case 4017: printMsg("Error: Mail server refused connection."); break;
        default:   printMsg("error %d in function '%s'", retval, function);
        }
    }
}

// loads the GENSOCK DLL file
int load_gensock()
{
#ifdef GENSOCK_STATIC_LINK
    pgensock_connect = gensock_connect;
    pgensock_getchar = gensock_getchar ;
    pgensock_put_data = gensock_put_data;
    pgensock_close = gensock_close;
    pgensock_gethostname=gensock_gethostname;
    pgensock_put_data_buffered=gensock_put_data_buffered;
    pgensock_put_data_flush=gensock_put_data_flush;
    dll_module_handle = GetModuleHandle (NULL);
#else
    if ( (gensock_lib = LoadLibrary("gwinsock.dll")) == NULL ) {
        if ( (gensock_lib = LoadLibrary("gensock.dll")) == NULL ) {
            if ( ! quiet )
                printMsg("Couldn't load either 'GWINSOCK.DLL' or 'GENSOCK.DLL'\nInstall one of these in your path.");
            return(-1);
        }
    }

    if ( 
       ( pgensock_connect =
         (  int (FAR PASCAL *)(char FAR *, char FAR *, socktag FAR *) )
         GetProcAddress(gensock_lib, "gensock_connect")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_connect\n");
        return(-1);
    }

    if ( 
       ( pgensock_getchar =
         ( int (FAR PASCAL *) (socktag, int, char FAR *) )
         GetProcAddress(gensock_lib, "gensock_getchar")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_getchar\n");
        return(-1);
    }

    if ( 
       ( pgensock_put_data =
         ( int (FAR PASCAL *) (socktag, char FAR *, unsigned long) )
         GetProcAddress(gensock_lib, "gensock_put_data")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_put_data\n");
        return(-1);
    }

    if ( 
       ( pgensock_close =
         (int (FAR PASCAL *) (socktag) )
         GetProcAddress(gensock_lib, "gensock_close")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_close\n");
        return(-1);
    }

    if ( 
       ( pgensock_gethostname =
         (int (FAR PASCAL *) (char FAR *, int) )
         GetProcAddress(gensock_lib, "gensock_gethostname")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_gethostname\n");
        return(-1);
    }

    if ( 
       ( pgensock_put_data_buffered =
         ( int (FAR PASCAL *) (socktag, char FAR *, unsigned long) )
         GetProcAddress(gensock_lib, "gensock_put_data_buffered")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_put_data_buffered\n");
        return(-1);
    }

    if ( 
       ( pgensock_put_data_flush =
         ( int (FAR PASCAL *) (socktag) )
         GetProcAddress(gensock_lib, "gensock_put_data_flush")
       ) == NULL
       ) {
        if ( ! quiet )
            printMsg("couldn't getprocaddress for gensock_put_data_flush\n");
        return(-1);
    }
#endif
    return(0);
}

int open_smtp_socket( void )
{
    int retval;
    char *ptr;

    /* load the library if it's not loaded */
//  if (!gensock_lib)
    if ( ( retval = load_gensock() ) ) return( retval );

    if ( NULL != (ptr=strchr(SMTPHost, ':')) ) {
        *ptr=0;
        strcpy(SMTPPort, ptr+1);
    }

    if ( (retval = (*pgensock_connect) ((LPSTR) SMTPHost,
                                        (LPSTR)((*SMTPPort == 0) ? "smtp" : SMTPPort),
                                        &SMTPSock)) ) {
        if ( retval == ERR_CANT_RESOLVE_SERVICE ) {
            if ( (retval = (*pgensock_connect) ((LPSTR)SMTPHost,
                                                (LPSTR)((*SMTPPort == 0) ? "25" : SMTPPort),
                                                &SMTPSock)) ) {
                gensock_error ("gensock_connect", retval);
                return(-1);
            }
        }
        // error other than can't resolve service
        else {
            gensock_error ("gensock_connect", retval);
            return(-1);
        }
    }

    // we wait to do this until here because WINSOCK is
    // guaranteed to be already initialized at this point.

    // get the local hostname (needed by SMTP)
    if ( (retval = (*pgensock_gethostname) (my_hostname, sizeof(my_hostname))) ) {
        gensock_error ("gensock_gethostname", retval);
        return(-1);
    }
    return(0);
}


int close_smtp_socket( void )
{
    int retval;

    if ( (retval = (*pgensock_close) (SMTPSock)) ) {
        gensock_error ("gensock_close", retval);
        return(-1);
    }
#ifdef GENSOCK_STATIC_LINK
#else
    FreeLibrary( gensock_lib );
#endif
    return(0);
}

int get_smtp_line( void )
{
    char ch;
    char in_data [MAXOUTLINE];
    char * index;
	char * rowstart;
    int retval = 0;

    index = in_data;

// Minor change in 1.8.1 to allow multiline responses.
// Courtesy of Wolfgang Schwart <schwart@wmd.de>
// (the do{}while(!retval) and the if(!retval) are new...
    do {
		rowstart=index;
		*(rowstart)=0; // Avoid a random "250-" being here and confusing the multiline detection below...
		*(rowstart+1)=0;
		*(rowstart+2)=0;
		*(rowstart+3)=0;
		ch='.'; // Must reinistialize to avoid '\n' being here in the event of a multi-line response
        while ( ch != '\n' ) {
            if ( (retval = (*pgensock_getchar) (SMTPSock, 0, &ch) ) ) {
                gensock_error ("gensock_getchar", retval);
                return(-1);
            } else {
                *index = ch;
                index++;
            }
        }

		/* Added in, version 1.8.8.  Multiline ESMTP responses (Tim Charron) */
		/* Look for 250- at the start of the response*/
		/* Actually look for xxx- at the start.  xxx can be any response code... */
		/* This typically an indicator of a multi-line ESMTP response.*/
		   /* ((*(rowstart)=='2') && (*(rowstart+1)=='5') && (*(rowstart+2)=='0') && (*(rowstart+3)=='-') ) */
		if ( *(rowstart+3)=='-') {
			retval=0; // Go get another string, wait if necessary.
		} else {
			/* append following lines, if there are some waiting to be received... */
			/* This is a polling call, and will return if no data is waiting.*/
			if ( !(retval = (*pgensock_getchar) (SMTPSock, 1, &ch)) ) {
				*index = ch;
				index++;
			}
		}
    } while ( !retval );

// Commented out 1.8.1 Tim Charron...
//   /* this is to support multi-line responses, common with */
//   /* servers that speak ESMTP */
//   /* I know, I know, it's a hack 8^) */
//   if ( in_data[3] == '-' ) return( get_smtp_line() );
//   else return(atoi(in_data));

    *index=0;
    if ( debug ) {
        printMsg("<<<getline<<< %s\n",in_data);  /*fflush(stdout);*/
    }
    return(atoi(in_data));
}

int put_smtp_line( socktag sock, const char far * line, unsigned int nchars )
{
    int retval;

    //lint -e(605)
    if ( (retval = (*pgensock_put_data) (sock, const_cast<char*>(line), (unsigned long) nchars)) ) {
        gensock_error ("gensock_put_data", retval);
        return(-1);
    }

    if ( debug ) {
        char * linecopy;
        linecopy = new char[nchars+1];
        for ( int i=0 ; i<(int)nchars ; i++ ) *(linecopy+i) = *(line+i);
        *(linecopy+nchars) = 0;

        printMsg(">>>putline>>> %s\n",linecopy);
        /*fflush(stdout);*/
        delete[] linecopy;
    }

    return(0);
}

int putline_internal (socktag sock, char * line, unsigned int nchars)
{
    int retval;

    if ( (retval =
          (*pgensock_put_data) (sock,
                                (char FAR *) line,
                                (unsigned long) nchars)) ) {
        switch ( retval ) {
        case ERR_NOT_CONNECTED:
            gensock_error( "SMTP server has closed the connection", retval );
            break;

        default:
            gensock_error ("gensock_put_data", retval);
        }
        return(-1);
    }

    if ( debug ) {
        char * linecopy;
        linecopy = new char[nchars+1];
        for ( int i=0 ; i<(int)nchars ; i++ ) *(linecopy+i) = *(line+i);
        *(linecopy+nchars) = 0;

        printMsg(">>>putline>>> %s\n",linecopy);
        delete[] linecopy;
    }

    return(0);
}

void smtp_error (const char * message)
{
    if ( ! quiet )
        printMsg("%s\n",message);
    put_smtp_line (SMTPSock, "QUIT\r\n", 6);
    // this is to wait for the 221 response from the server
    get_smtp_line();
//   close_smtp_socket();
}


// 'destination' is the address the message is to be sent to
// 'message' is a pointer to a null-terminated 'string' containing the
// entire text of the message.
int prepare_smtp_message(char * dest,
                         const char* wanted_hostname,
                         char* loginAuth, char* pwdAuth)
{
    char out_data[MAXOUTLINE];
    char str[1024];
    char *ptr,*pp;
    int len, startLen,ret_temp;

    if ( wanted_hostname!=NULL )
        if ( (*wanted_hostname)=='\0' )
            wanted_hostname=NULL;

    if ( 0 != open_smtp_socket() ) return(-1);

    if ( get_smtp_line() != 220 ) {
        smtp_error ("SMTP server error");
        return(-1);
    }

    // Changed to EHLO processing 27 Feb 2001 Craig Morrison
    sprintf( out_data, "EHLO %s\r\n",
             (wanted_hostname==NULL) ? my_hostname  : wanted_hostname);
    if ( 0!=put_smtp_line( SMTPSock, out_data, strlen (out_data) ) ) {
        return(-1);
    }

    if ( get_smtp_line() != 250 ) {
        sprintf( out_data, "HELO %s\r\n",
                 (wanted_hostname==NULL) ? my_hostname  : wanted_hostname);
        if ( 0!=put_smtp_line( SMTPSock, out_data, strlen (out_data) ) )
            return(-1);
        if ( get_smtp_line() != 250 ) {
            smtp_error ("SMTP server error");
            return(-1);
        }
    }

    /* NOW: auth */
    if ( (loginAuth!=NULL) && (pwdAuth!=NULL) ) {
        if ( !authplain ) {
            if ( (*loginAuth)!='\0' ) {
                char* out_b64;
                sprintf (out_data, "AUTH LOGIN\r\n");

                if ( 0!=put_smtp_line( SMTPSock, out_data, strlen (out_data) ) ) return(-1);

                if ( get_smtp_line() != 334 ) {
                    smtp_error ("The mail server doesn't require AUTH LOGIN\nAre you sure server supports AUTH?");
                    return(-2);
                }

                base64_encode(loginAuth,loginAuth+lstrlen(loginAuth),out_data);
                lstrcat(out_data,"\r\n");
                out_b64 = out_data;
                while ( ((*out_b64) == '\x0a') || ((*out_b64) == '\x0d') )
                    out_b64++;
                if ( 0!=put_smtp_line( SMTPSock, out_b64, strlen (out_b64) ) )  return(-1);

                if ( get_smtp_line() != 334 ) {
                    smtp_error ("The mail server did not accept LOGIN name");
                    return(-2);
                }

                base64_encode(pwdAuth,pwdAuth+lstrlen(pwdAuth),out_data);
                lstrcat(out_data,"\r\n");
                while ( ((*out_b64) == '\x0a') || ((*out_b64) == '\x0d') )
                    out_b64++;
                if ( 0!=put_smtp_line( SMTPSock, out_b64, strlen (out_b64) ) )  return(-1);

                if ( get_smtp_line() != 235 ) {
                    smtp_error ("The mail server did not accept Auth LOGIN PASSWD value");
                    return(-2);
                }
            }
        } else {                                 // AUTH PLAIN login added 27 Feb 2001 Craig Morrison
            char* out;
            out = str;
            *out = 0;
            out++;
            lstrcpy(out, loginAuth);
            while ( *out ) out++;
            out++;
            lstrcpy(out, pwdAuth);
            base64_encode(str, str+(strlen(loginAuth)+strlen(pwdAuth)+2), out_data);

            out=out_data;
            while ( ((*out) == '\x0a') || ((*out) == '\x0d') )
                out++;
            sprintf(str, "AUTH PLAIN %s\r\n", out);

            if ( 0!=put_smtp_line( SMTPSock, str, strlen (str) ) )  return(-1);

            if ( get_smtp_line() != 235 ) {
                smtp_error ("The mail server did not accept Auth PLAIN value");
                return(-2);
            }
        }
    }

    sprintf (out_data, "MAIL From:<%s>\r\n", loginname);
    if ( 0!=put_smtp_line( SMTPSock, out_data, strlen (out_data) ) ) return(-1);

    if ( get_smtp_line() != 250 ) {
        smtp_error ("The mail server doesn't like the sender name,\nhave you set your mail address correctly?");
        return(-2);
    }

    // do a series of RCPT lines for each name in address line
    for ( ptr = dest; *ptr; ptr += len + 1 ) {
        // if there's only one token left, then len will = startLen,
        // and we'll iterate once only
        startLen = strlen (ptr);
        if ( (len = strcspn (ptr, " ,\n\t\r")) != startLen ) {
            ptr[len] = '\0';                     // replace delim with NULL char
            while ( strchr (" ,\n\t\r", ptr[len+1]) )    // eat white space
                ptr[len++] = '\0';
        }

        // 30 Jun 2000 Craig Morrison, parses out just email address in TO:,CC:,BCC:
        // so that all we RCPT To with is the email address.
        pp = strchr(ptr, '<');
        if ( pp ) {
            strcpy(str, pp+1);
            pp=str;
            while ( (*pp != '>') && (*pp) ) pp++;
            *pp = 0;
            sprintf(out_data, "RCPT To: <%s>\r\n", str);
        } else {
            sprintf (out_data, "RCPT To: <%s>\r\n", ptr);
        }

        putline_internal( SMTPSock, out_data, strlen (out_data) );

        ret_temp=get_smtp_line();
        if ( ( ret_temp != 250 ) && ( ret_temp != 251 ) ) {
            sprintf (str, "The mail server doesn't like the name %s.\nHave you set the 'To: ' field correctly?", ptr);
            smtp_error (str);
            return(-1);
        }

        if ( len == startLen )                   // last token, we're done
            break;
    }

    sprintf (out_data, "DATA\r\n");
    if ( 0!=put_smtp_line( SMTPSock, out_data, strlen (out_data) ) ) return(-1);

    if ( get_smtp_line() != 354 ) {
        smtp_error ("Mail server error accepting message data");
        return(-1);
    }

    return(0);

}

int transform_and_send_edit_data( socktag sock, char * editptr )
{
    char *index;
    char *header_end;
    char previous_char = 'x';
    unsigned int send_len;
    int retval;
    BOOL done = 0;

    send_len = lstrlen(editptr);
    index = editptr;

    header_end = strstr (editptr, "\r\n\r\n");
    if ( header_end==NULL ) header_end=editptr+send_len;    // end of buffer

    while ( !done ) {
        // room for extra char for double dot on end case
        while ( (unsigned int) (index - editptr) < send_len ) {
            switch ( *index ) {
            case '.':
                if ( previous_char == '\n' )
                    /* send _two_ dots... */
                    if ( (retval = (*pgensock_put_data_buffered) (sock, index, 1)) ) return(retval);
                if ( (retval = (*pgensock_put_data_buffered) (sock, index, 1)) ) return(retval);
                break;
            case '\r':
                // watch for soft-breaks in the header, and ignore them
                if ( index < header_end && (strncmp (index, "\r\r\n", 3) == 0) )
                    index += 2;
                else
                    if ( previous_char != '\r' )
                    if ( (retval = (*pgensock_put_data_buffered) (sock, index, 1)) )
                        return(retval);
                    // soft line-break (see EM_FMTLINES), skip extra CR */
                break;
            default:
                if ( (retval = (*pgensock_put_data_buffered) (sock, index, 1)) )
                    return(retval);
            }
            previous_char = *index;
            index++;
        }
        if ( (unsigned int) (index - editptr) == send_len ) done = 1;
    }

    // this handles the case where the user doesn't end the last
    // line with a <return>

    if ( editptr[send_len-1] != '\n' ) {
        //lint -e605
        if ( (retval = (*pgensock_put_data_buffered) (sock, "\r\n.\r\n", 5)) )
            return(retval);
    } else
        //lint -e605
        if ( (retval = (*pgensock_put_data_buffered) (sock, ".\r\n", 3)) )
        return(retval);

    /* now make sure it's all sent... */
    if ( (retval = (*pgensock_put_data_flush)(sock)) )  return(retval);
    return(0);
}

int fixup(char * string, char * * tempstring2)
{
    // tempstring2=NULL .. modify in place
    // tempstring2!=NULL...don't modify string.  Put new string in tempstring2.
    int needfix=0;
    int i,stl;
    char * tempstring;
    int Pos=0;

    // First, detect any funny characters.
    stl=strlen(string);
    for ( i=0;i<stl;i++ ) {
        if ( (string[i]<0x20) || (string[i]>0x7D) || (string[i] == 0x3D) || (string[i] == 0x3F) ) needfix=1;    // ..,=,?
    }

    // If needed, fixup the string.
    if ( needfix ) {
        tempstring=new char[1 + 17 + GetLengthConvertToQuotedPrintable(string, 1)];    // null terminator + 17 char ISO prefix/suffix + per character mime encapsulation
		strcpy(tempstring,"=?");
		strcat(tempstring,charset);
		strcat(tempstring,"?Q?");
        for ( i=0;i<stl;i++ ) {
            ConvertToQuotedPrintable((string[i]), &Pos, tempstring + strlen(tempstring), 1);
        }
        strcat(tempstring,"?=");
        if ( tempstring2==NULL ) {
            strcpy(string, tempstring);          // Copy back to source string (calling program must ensure buffer is big enough)
            delete [] tempstring;                // Delete temporary
        } else {
            *tempstring2=tempstring;             // Let the user delete it
        }
        return(1);
    } else {
        // NO fixup was performed.  Make sure that tempstring2 points to the right
        // place if user expects a new string...
        if ( tempstring2!=NULL ) {
            *tempstring2=new char[strlen(string)+1];
            strcpy(*tempstring2,string);
        }
    }
    return(0);
}

int send_smtp_edit_data (char * message)
{
    int retval;

    if ( 0 != (retval=transform_and_send_edit_data( SMTPSock, message )) ) return(retval);

    if ( get_smtp_line() != 250 ) {
        smtp_error ("Message not accepted by server");
        return(-1);
    }
    return(0);
}


int finish_smtp_message( void )
{
    int ret;

    ret = put_smtp_line( SMTPSock, "QUIT\r\n", 6 );
    // wait for a 221 message from the server
    get_smtp_line();
    return(ret);
}

// create a registry entries for this program
int CreateRegEntry( void )
{
    HKEY  hKey1;
    DWORD  dwDisposition;
    LONG   lRetCode;
    char   strRegisterKey[256];

    strcpy(strRegisterKey,"SOFTWARE\\Public Domain\\Blat");
    if ( Profile[0] != '\0' ) {
        strcat(strRegisterKey, "\\");
        strcat(strRegisterKey, Profile);
    }

    /* try to create the .INI file key */
    lRetCode = RegCreateKeyEx ( HKEY_LOCAL_MACHINE,
                                strRegisterKey,
                                0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE,NULL, &hKey1,&dwDisposition
                              );

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printf ("Error in creating blat key in the registry\n");
        return(10);
    }

    /* try to set a section value */
    lRetCode = RegSetValueEx( hKey1,"SMTP server",0,REG_SZ, (BYTE *) &SMTPHost[0], (strlen(SMTPHost)+1));

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printf ( "Error in setting SMTP server value in the registry\n");
        return(11);
    }

    /* try to set another section value */
    lRetCode = RegSetValueEx( hKey1,"Sender",0,REG_SZ, (BYTE *) &Sender[0], (strlen(Sender)+1));

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printf ( "Error in setting sender address value in the registry\n");
        return(11);
    }

    /* try to set another section value */
    lRetCode = RegSetValueEx( hKey1,"SMTP Port",0,REG_SZ, (BYTE *) &SMTPPort[0], (strlen(SMTPPort)+1));

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printf ( "Error in setting port value in the registry\n");
        return(11);
    }

    /* try to set another section value */
    lRetCode = RegSetValueEx( hKey1,"Try",0,REG_SZ, (BYTE *) &Try[0], (strlen(Try)+1));

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printf ( "Error in setting number of try value in the registry\n");
        return(11);
    }

    return(0);
}


#if 0
// Delete a registry entries for this program
int DeleteRegEntry( void )
{
    HKEY  hKey1=NULL;
    LONG   lRetCode;
    char   strRegisterKey[256];

    strcpy(strRegisterKey,"SOFTWARE\\Public Domain\\Blat");
    if ( Profile[0] != '\0' ) {
        strcat(strRegisterKey, "\\");
        strcat(strRegisterKey, Profile);


        /* try to create the .INI file key */
        lRetCode = RegOpenKeyEx ( HKEY_LOCAL_MACHINE,
                                  strRegisterKey,
                                  0, KEY_ALL_ACCESS, &hKey1
                                );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ("Error in finding blat profile %s in the registry\n", Profile);
            return(10);
        }

        /* try to remove a section*/
        lRetCode = RegDeleteKey( hKey1,"");

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in deleting profile %s in the registry\n", Profile);
            return(11);
        }
        if ( hKey1!=NULL )
            RegCloseKey(hKey1);
    }
    return(0);
}

// get the registry entries for this program
int GetRegEntry( void )
{
    HKEY  hKey1=NULL;
    DWORD  dwType;
    DWORD  dwBytesRead;
    LONG   lRetCode;
    char   register_key[256];

    // open the registry key in read mode
    strcpy(register_key, "SOFTWARE\\Public Domain\\Blat");
    if ( Profile[0] != '\0' ) {
        strcat(register_key, "\\");
        strcat(register_key, Profile);
    }
    lRetCode = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                             register_key,
                             0, KEY_READ, &hKey1
                           );
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printMsg( "Failed to open registry key for Blat profile %s, using default.\n", Profile );
        strcpy(register_key, "SOFTWARE\\Public Domain\\Blat");
        lRetCode = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                 register_key,
                                 0, KEY_READ, &hKey1
                               );
        if ( lRetCode != ERROR_SUCCESS ) {
//       if ( ! quiet ) printMsg( "Failed to open registry key for Blat\n" );
            return(12);
        }
    }
    // set the size of the buffer to contain the data returned from the registry
    // thanks to Beverly Brown "beverly@datacube.com" and "chick@cyberspace.com" for spotting it...
    dwBytesRead=SERVER_SIZE;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, "SMTP server", NULL , &dwType, (BYTE *) &SMTPHost, &dwBytesRead);    //lint !e545
    // if we failed, note it, and leave
    if ( lRetCode != ERROR_SUCCESS ) {
//    if ( ! quiet ) printMsg( "Failed to read SMTP server value from the registry\n" );
        return(12);
    }

    dwBytesRead=SENDER_SIZE;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, "Sender", NULL , &dwType, (BYTE *) &Sender, &dwBytesRead);    //lint !e545
    // if we failed, note it, and leave
    if ( lRetCode != ERROR_SUCCESS ) {
//    if ( ! quiet ) printMsg( "Failed to read senders user name from the registry\n" );
        return(12);
    }

    dwBytesRead=TRY_SIZE;
    // read the value of the number of try entry
    lRetCode = RegQueryValueEx( hKey1, "Try", NULL , &dwType, (BYTE *) &Try, &dwBytesRead);    //lint !e545
    // if we failed, assign a default value
    if ( lRetCode != ERROR_SUCCESS ) {
        strcpy(Try,"1");
    }

    dwBytesRead=SERVER_SIZE;
    // read the value of the number of try entry
    lRetCode = RegQueryValueEx( hKey1, "SMTP Port", NULL , &dwType, (BYTE *) &SMTPPort, &dwBytesRead);    //lint !e545
    // if we failed, assign a default value
    if ( lRetCode != ERROR_SUCCESS )    *SMTPPort = 0;
    if ( hKey1!=NULL )
        RegCloseKey(hKey1);

    return(0);
}

// List all profiles
void ListProfiles(char *pstrProfile)
{
    HKEY  hKey1=NULL;
    DWORD  dwBytesRead;
    LONG   lRetCode;
    char   register_key[256];
    DWORD dwIndex;                               // index of subkey to enumerate
    FILETIME lftLastWriteTime;
    // address for time key last written to);


    // open the registry key in read mode
    strcpy(register_key, "SOFTWARE\\Public Domain\\Blat");
    lRetCode = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                             register_key,
                             0, KEY_READ, &hKey1
                           );
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet )  printMsg( "Failed to open registry key for Blat profile %s, using default.\n", Profile );
    } else {

        dwBytesRead=sizeof(Profile);
        if ( ! quiet ) {
            printMsg( "BLAT PROFILE EDITOR\n");
            printMsg( "To modify:    blat -install SMTPHost Sender [Try [Port [Profile]]]\n");
            printMsg( "To delete:    blat -profile -delete Profile\n");
            printMsg( "Profiles are listed as in the -install option:\n");
            printMsg( "SMTPHost Sender Try SMTPPort Profile\n\n");
        }
        quiet=1;
        Profile[0]='\0';
        GetRegEntry();
        if ( (!strcmp(pstrProfile,"<default>"))||(!strcmp(pstrProfile,"<all>")) ) {
            printMsg("%s %s %s %s %s\n",SMTPHost,Sender,Try,SMTPPort,Profile);
        }

        dwIndex=0;
        do {
            dwBytesRead=sizeof(Profile);
            lRetCode = RegEnumKeyEx(  hKey1,     // handle of key to enumerate
                                      dwIndex++, // index of subkey to enumerate
                                      Profile,   // address of buffer for subkey name
                                      &dwBytesRead,    // address for size of subkey buffer
                                      NULL,      // reserved
                                      NULL,      // address of buffer for class string
                                      NULL,      // address for size of class buffer
                                      &lftLastWriteTime
                                      // address for time key last written to);
                                   );
            if ( lRetCode == 0 ) {
                GetRegEntry();
                if ( (!strcmp(pstrProfile,Profile))||(!strcmp(pstrProfile,"<all>")) ) {
                    printMsg("%s %s %s %s %s\n",SMTPHost,Sender,Try,SMTPPort,Profile);
                }
            };
        } while ( lRetCode == 0 );
    }
    if ( hKey1!=NULL )
        RegCloseKey(hKey1);
}
#endif

LPCSTR GetNameWithoutPath(LPCSTR lpFn)
{
    LPCSTR lpRet = lpFn ;
    LPCSTR lpNext ;
    while ( (*lpFn) != '\0' ) {
        if ( ((*lpFn) == ':') || ((*lpFn) == '\\') )    lpRet = lpFn + 1;
        lpNext=CharNext(lpFn);
        lpFn = lpNext;
    }
    return(lpRet);
}

#define UU_Mask(Ch) (char) ((char)(Ch) & 0x3f)
void douuencode(const char * filebuffer,char * out,DWORD filesize,const char* filename)
{
    sprintf(out,"begin 644 %s\x0d\x0a",GetNameWithoutPath(filename));
    out += strlen(out);
    while ( filesize>0 ) {
        int size_line = filesize;
        if ( size_line>45 )
            size_line=45;
        filesize -= size_line;
        *(out++) = ' ' + ((char)size_line);

        while ( size_line > 0 ) {
            *(out) = UU_Mask(*(filebuffer) >> 2) + 0x20;
            if ( (*out)==' ' ) *out='\x60';
            out++;

            *(out) = UU_Mask((*(filebuffer) << 4) & 060 | (*(filebuffer+1) >> 4) & 017) + 0x20;
            if ( (*out)==' ' ) *out='\x60';
            out++;

            *(out) = UU_Mask((*(filebuffer+1) << 2) & 074 | (*(filebuffer+2) >> 6) & 03) + 0x20;
            if ( (*out)==' ' ) *out='\x60';
            out++;

            *(out) = UU_Mask(*(filebuffer+2) & 077) + 0x20;
            if ( (*out)==' ' ) *out='\x60';
            out++;

            filebuffer += 3;
            size_line -=3;
        }
        *(out++) = '\x0d';
        *(out++) = '\x0a';
    }
    strcpy(out,"\x60\x0d\x0a\x65\x6e\x64\x0d\x0a");
}

// MIME Quoted-Printable Content-Transfer-Encoding
#define MimeHexChar "0123456789ABCDEF";
int GetLengthConvertToQuotedPrintable(char *pszStr, int inHeader) {
    /* get the length of a line created that way */
    int ThisValue;
    // char HexTable[17] = MimeHexChar;
    char *pch;
    int i;
    int stl;
    int iLen;

    stl = strlen(pszStr);

    iLen = 0;
    pch = pszStr;
    for ( i = 0; i < stl; i++ ) {
        ThisValue = (256 + ((unsigned int) (*pch)) ) % 256;
        if ( (ThisValue == 13) && (!inHeader) ) {
            iLen += 0;
        } else if ( (ThisValue == 10) && (!inHeader) ) {
            iLen += 2;
        } else if ( (ThisValue  < 33) ||
                    (ThisValue == 61) ||
                    (ThisValue == 63) ||
                    (ThisValue > 126) ) {
            iLen += 3;
        } else {
            iLen += 1;
        }
        pch++;
    }
    return(iLen);
}

void ConvertToQuotedPrintable(char ThisChar, int * CurrPos, char * buffer, int inHeader)
{
    int ThisValue;
    div_t result;
    char HexTable[17] = MimeHexChar;

    ThisValue = (256 + (unsigned int) ThisChar) % 256;    //lint !e571

    if ( (ThisValue == 13) && (!inHeader) ) {
        sprintf( buffer, "%s", "\0" );
        return;
    } else if ( (ThisValue == 10) && (!inHeader) ) {
        sprintf( buffer, "%s", "\r\n" );
        (*CurrPos) = 0;
        return;
    } else if ( (ThisValue  < 33) ||
                (ThisValue == 61) ||
                (ThisValue == 63) ||
                (ThisValue > 126) ) {
        result = div(ThisValue,16);
        buffer[0] = '=';
        (*CurrPos)++;
        buffer[1] = HexTable[result.quot];
        (*CurrPos)++;
        buffer[2] = HexTable[result.rem];
        (*CurrPos)++;
        buffer[3] = '\0';
    } else {
        buffer[0] = ThisChar;
        (*CurrPos)++;
        buffer[1] = '\0';
    }

    if ( ( *CurrPos > 71 ) && !inHeader ) {
        strcat(buffer, "=\r\n");                 /* Add soft line break */
        (*CurrPos) = 0;
    }
}

// MIME base64 Content-Transfer-Encoding
#define B64_ENC(Ch) (char) (base64table[(char)(Ch) & 0x3f])
void base64_encode(const char *in, const char *endin, char *out)
{
    char base64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int length = (int)(endin - in) ;

    char *b = out;
    const char *data = in;
    for ( ; length > 2; length -= 3, data += 3 ) {
        if ( ((data-in)%54) == 0 ) {
            *b++ = '\r'; *b++ = '\n';
        }
        *b++ = B64_ENC(data[0] >> 2);
        *b++ = B64_ENC(((data[0] << 4) & 060) | ((data[1] >> 4) & 017));
        *b++ = B64_ENC(((data[1] << 2) & 074) | ((data[2] >> 6) & 03));
        *b++ = B64_ENC(data[2] & 077);
    }

    if ( length == 1 ) {
        if ( ((data-in)%54) == 0 ) {
            *b++ = '\r'; *b++ = '\n';
        }
        *b++ = B64_ENC(data[0] >> 2);
        *b++ = B64_ENC((data[0] << 4) & 060);
        *b++ = '=';
        *b++ = '=';
    } else if ( length == 2 ) {
        if ( ((data-in)%54) == 0 ) {
            *b++ = '\r'; *b++ = '\n';
        }
        *b++ = B64_ENC(data[0] >> 2);
        *b++ = B64_ENC(((data[0] << 4) & 060) | ((data[1] >> 4) & 017));
        *b++ = B64_ENC((data[1] << 2) & 074);
        *b++ = '=';
    }
    *b = 0;

    return;
}

// Convert the entry "n of try" to a numeric, defaults to 1
int noftry()
{
    int n_of_try;
    int i;
    n_of_try = 0;

    for ( i=0; i<(int)strlen(Try); i++ ) Try[i] = (char) toupper(Try[i]);

    if ( !strcmp(Try, "ONCE") ) n_of_try = 1;
    if ( !strcmp(Try, "TWICE") ) n_of_try = 2;
    if ( !strcmp(Try, "THRICE") )   n_of_try = 3;
    if ( !strcmp(Try, "INFINITE") ) n_of_try =  -1;
    if ( !strcmp(Try, "-1") ) n_of_try =  -1;

    if ( n_of_try == 0 ) {
        for ( i=0; i<(int)strlen(Try); i++ ) {
            if ( Try[i]>=48 && Try[i]<=57 ) n_of_try = n_of_try * 10 + (Try[i] - 48);
        }
    }

    if ( n_of_try == 0 || n_of_try <=-2 ) n_of_try=1;
    return(n_of_try);
}

void SetFileType (char *sDestBuffer, char *sFileName)
{
    // Toby Korn (tkorn@snl.com)
    // SetFileType examines the file name sFileName for known file extensions and sets
    // the content type line appropriately.  This is returned in sDestBuffer.
    // sDestBuffer must have it's own memory (I don't alloc any here).

    char sType [40];
    char sExt [20];
    int iPtr;
    LONG lResult;
    HKEY key=NULL;
    unsigned long lTypeSize;
    unsigned long lStringType = REG_SZ;

    // Find the last '.' in the filename
    for ( iPtr = strlen(sFileName); (iPtr >= 0) && (sFileName[iPtr] != '.'); iPtr-- );
    if ( sFileName[iPtr] == '.' ) {
        // Found the extension type.
        strcpy(sExt, sFileName + iPtr);
        lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, (LPCTSTR) sExt, 0, KEY_READ, &key);
        if ( lResult == ERROR_SUCCESS ) {
            lTypeSize = (long) sizeof(sType);
            lResult = RegQueryValueEx(key, "Content Type", NULL, &lStringType, (unsigned char *) sType,
                                      &lTypeSize); RegCloseKey (key);
        }
        // keep a couple of hard coded ones in case the registry is missing something.
        if ( lResult != ERROR_SUCCESS )
            if ( _stricmp(sExt, ".pdf") == 0 )
                strcpy(sType, "application/pdf");
            else if ( _stricmp(sExt, ".xls") == 0 )
                strcpy(sType, "application/vnd.ms-excel");
            else
                strcpy(sType, "application/octet-stream");
    } else {
        strcpy(sType, "application/octet-stream");
    }
    sprintf( sDestBuffer, "Content-Type: %s; name=%s\r\n", sType, sFileName );
}

int ReadNamesFromFile(char * type, char * namesfilename, char * * listofnames) {
    HANDLE fileh;
    int found;

    if ( (fileh=CreateFile(namesfilename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN,NULL))==INVALID_HANDLE_VALUE ) {
        if ( ! quiet )   printMsg("error reading %s, aborting\n",namesfilename);
        return(3);
    }
    DWORD filesize = GetFileSize( fileh,NULL );

    int nameslen = filesize+1;
    *listofnames = new char[nameslen];           // create room for file contents
    if ( *listofnames == NULL ) {
        CloseHandle(fileh);
        return(4);
    }
    DWORD dummy;
    if ( !ReadFile(fileh,*listofnames,filesize,&dummy,NULL) ) {
        if ( ! quiet )   printMsg("error reading %s, aborting\n",namesfilename);
        CloseHandle(fileh);
        delete [] *listofnames;
        return(5);
    }
    CloseHandle(fileh);
    (*listofnames)[filesize]=0;                  // Terminate the string.
    // Blow away any CRLF or comma if they exist at the very end of the string...
    // The order these are checked matters!
    if ( 0x0A==(*listofnames)[strlen(*listofnames)-1] ) (*listofnames)[strlen(*listofnames)-1]=0;
    if ( 0x0D==(*listofnames)[strlen(*listofnames)-1] ) (*listofnames)[strlen(*listofnames)-1]=0;
    if ( ','==(*listofnames)[strlen(*listofnames)-1] )   (*listofnames)[strlen(*listofnames)-1]=0;

    // Fix any carriage returns/linefeeds.  Just change CR to space, and LF to comma...
    found=1;
    for ( int i=0;i<nameslen;i++ ) {
        if ( (*listofnames)[i]==0x0D ) (*listofnames)[i]=' ';
        if ( (*listofnames)[i]==0x0A ) (*listofnames)[i]=',';
        if ( (*listofnames)[i]== ',' ) found++;
    }

    if ( ! quiet )   printMsg("Read %d %s addresses from %s\n",found,type,namesfilename);

    return(0);                                   // indicates no error.
}

#define MIN_ALLOC_STEP_TEMP_CONSOLE_BUFFER (0x2000)
BOOL InitTempConsoleBuffer(
                          DWORD * lpdwTempConsoleBufferSize,
                          DWORD * lpdwTempConsoleBufferLength,
                          LPSTR * lplpszTempConsoleBuffer)
{
    if ( (*lplpszTempConsoleBuffer)!=NULL ) free(*lplpszTempConsoleBuffer);
    *lplpszTempConsoleBuffer = (LPSTR) malloc(MIN_ALLOC_STEP_TEMP_CONSOLE_BUFFER);
    if ( (*lplpszTempConsoleBuffer)==NULL ) return(FALSE);

    *lpdwTempConsoleBufferSize = MIN_ALLOC_STEP_TEMP_CONSOLE_BUFFER;
    *lpdwTempConsoleBufferLength = 0;
    *(*lplpszTempConsoleBuffer) = '\0';
    return(TRUE);
}

BOOL AddStringORCharToTempConsoleBuffer(
                                       DWORD * lpdwTempConsoleBufferSize,
                                       DWORD * lpdwTempConsoleBufferLength,
                                       LPSTR * lplpszTempConsoleBuffer,
                                       LPCSTR  lpszStringAdd,
                                       char    cAdd)
{
    char cReservStringAdd[2];
    DWORD dwLengthAdd;
    DWORD dwNewSizeMinimal ;
    DWORD dwSizeAllocStep = MIN_ALLOC_STEP_TEMP_CONSOLE_BUFFER;

    if ( lpszStringAdd == NULL ) {
        lpszStringAdd = cReservStringAdd;
        cReservStringAdd[0]=cAdd;
        cReservStringAdd[1]='\0';
        dwLengthAdd = 1;
    } else {
        dwLengthAdd = lstrlen(lpszStringAdd);
    }

    dwNewSizeMinimal = *lpdwTempConsoleBufferLength + dwLengthAdd + 8;
    if ( *lpdwTempConsoleBufferSize < dwNewSizeMinimal ) {
        LPSTR lpszNewTempConsoleBuffer;
        DWORD dwNewSizeBuffer = ((dwNewSizeMinimal / dwSizeAllocStep) + 1 ) * dwSizeAllocStep ;

        if ( *lplpszTempConsoleBuffer == NULL ) {
            lpszNewTempConsoleBuffer = (LPSTR) malloc(dwNewSizeBuffer);
        } else {
            lpszNewTempConsoleBuffer = (LPSTR) realloc(*lplpszTempConsoleBuffer, dwNewSizeBuffer);
            if ( lpszNewTempConsoleBuffer == NULL ) free(*lplpszTempConsoleBuffer);
        }

        *lplpszTempConsoleBuffer = lpszNewTempConsoleBuffer;
        if ( lpszNewTempConsoleBuffer == NULL ) {
            *lpdwTempConsoleBufferLength = *lpdwTempConsoleBufferSize = 0;
            return(FALSE);
        } else {
            *lpdwTempConsoleBufferSize = dwNewSizeBuffer ;
        }
    }
    memmove((*lplpszTempConsoleBuffer)+(*lpdwTempConsoleBufferLength),lpszStringAdd,dwLengthAdd);
    (*lpdwTempConsoleBufferLength) += dwLengthAdd;
    *((*lplpszTempConsoleBuffer)+(*lpdwTempConsoleBufferLength)) = '\0';
    return(TRUE);
}

inline char* new_realloc( char* pSrc, size_t newSize ) {
    char* newBuffer = new char[ newSize ];
    memset( newBuffer, 0, newSize );
    strncpy( newBuffer, pSrc, newSize );
    newBuffer[newSize-1]=0;                      // Terminate the string
    delete[] pSrc;
    return(newBuffer);
}


//wrapper call added by EAS for lib usage
int callBlat(int argc, char **argv, char **envp)
{
    int next_arg=2;
    int impersonating = 0;
    int penguin = 0;
    int i, j, k;
    int ret, retcode=0;//, regerr;
    int n_of_try;
    DWORD dwTempConsoleBufferSize = 0;
    DWORD dwTempConsoleBufferLength = 0;
    LPSTR lpszTempConsoleBuffer = NULL;
    HANDLE fileh = INVALID_HANDLE_VALUE;
    //FILE *tf;
    int hours, minutes;
    OFSTRUCT of;
    char filename[240];
    int bodyparameter=0;
    LPSTR lpszMessageCgi=NULL;
    BOOL fCgiWork=FALSE;
    LPSTR lpszCgiFailureUrl=NULL;
    LPSTR lpszCgiSuccessUrl=NULL;
    LPSTR lpszFirstReceivedData=NULL;
    LPSTR lpszOtherHeader=NULL;
    char * tempstring=NULL;
    char priority [2];

    priority[0] = '\0';

    // by default Blat is very noisy!
    quiet = 0;
    debug = 0;

    if ( argc<=2 ) {
#ifndef DEBUGCGI
        char c;
        if ( (GetEnvironmentVariable("REQUEST_METHOD",&c,1)>0) &&
             (GetEnvironmentVariable("GATEWAY_INTERFACE",&c,1)>0) )
#endif
        {
            if ( DoCgiWork(argc,argv,lpszMessageCgi,lpszCgiSuccessUrl,
                           lpszCgiFailureUrl,lpszFirstReceivedData,
                           lpszOtherHeader) ) {
                quiet = 1;
                fCgiWork=TRUE;
            }
        }
    }

    // by default Blat does not use mime Quoted-Printable Content-Transfer-Encoding!
    mime = 0;
	strcpy(charset,"ISO-8859-1");

    // by default Blat does not use UUEncode
    // Added by Gilles Vollant
    uuencode= 0;

    // base64 encoding -- Added by Tim Charron tcharron@interlog.com / Gilles Vollant
    // by default Blat does not use base64 Quoted-Printable Content-Transfer-Encoding!
    //  If you're looking for something to do, then it would be nice if this thing
    //  detected any non-printable characters in the input, and use base64 whenever
    //  quoted-printable wasn't chosen by the user.
    base64 = 0;

    // attach -- Added in by Tim Charron (tcharron@interlog.com)
    // If "-attach filename" is on the command line at least once,
    // then those files will be attached as base64 encoded files.
    // This variable is the count of how many of these files there are.
    attach = 0;

    // -enriched -- Added 10. June 1999 by James Greene (greene@gucc.org)
    // -html     -- Added 15. June 1999 by James Greene (greene@gucc.org)
    // if "-enriched" is on the command line, assume the text is written
    // with enriched text markers, e.g. <bold><center><flushright>
    // <FontFamily> etc.
    // if "-html" is on the command line, assume the text is well-formed HTML
    // otherwise, assume plain text

    strcpy(textmode, "plain");

    if ( argc<2 ) {
        // must have at least file name to send
		printUsage();
        return(1);
    }

    for ( i=1; i < argc; i++ ) {

        if ( lstrcmpi( "-noh",argv[i] ) == 0 )  noheader = 1;
        if ( lstrcmpi( "-noh2",argv[i] ) == 0 ) noheader = 2;
        if ( lstrcmpi( "-q",argv[i] ) == 0 ) quiet = 1;
        if ( lstrcmpi( "-debug",argv[i] ) == 0 ) {
            quiet = 0; debug=1;
        }
        if ( lstrcmpi( "-mime"  ,argv[i] ) == 0 ) {
            mime = 1; base64 = 0; uuencode=0;
        }
        if ( lstrcmpi( "-base64",argv[i] ) == 0 ) {
            mime = 0; base64 = 1; uuencode=0;
        }
        if ( lstrcmpi( "-uuencode",argv[i] ) == 0 ) {
            mime = 0; base64 = 0; uuencode=1;
        }
        if ( (lstrcmpi( "-p",argv[i] ) == 0 ) && (i+1<argc) ) {
            strcpy(Profile,argv[++i]);
        }
        if ( lstrcmpi( "-enriched"  ,argv[i] ) == 0 ) {
            mime = 1; base64 = 0; uuencode=0; strcpy(textmode, "enriched");
        }
        if ( lstrcmpi( "-html"  ,argv[i] ) == 0 ) {
            mime = 1; base64 = 0; uuencode=0; strcpy(textmode, "html");
        }
        if ( lstrcmpi( "-plain"  ,argv[i] ) == 0 ) {    // Added 27 Feb 2001 Craig Morrison
            authplain=1;
        }
        if ( lstrcmpi( "-log", argv[i] ) == 0 ) {    // Added 23 Aug 2000 Craig Morrison
            if ( i+1 < argc ) {
                logOut = fopen(argv[++i], "a+");
                // if all goes well the file is closed normally
                // if we return before closing the library SHOULD 
                // close it for us..
            } else {
                // filename not there...
				// printUsage();
                // return(1);
            }
        }
    }

    // get file name from argv[1]
    strcpy(filename,argv[1]);

    Sender[0] = '\0';
    SMTPHost[0] = '\0';
    strcpy(SMTPPort, "25");
    strcpy(Try,"ONCE");
    my_hostname[0] = '\0';
    destination = NULL;
    cc_list = NULL;
    bcc_list = NULL;
    loginname[0] = '\0';                         // RFC 821 MAIL From: <loginname>	
    senderid[0] = '\0';                          // Inconsistent use in Blat for some RFC 822 Fielsd definitions	
    sendername[0] = '\0';                        // RFC 822 Sender: <sendername> 
    fromid[0] = '\0';                            // RFC 822 From: <fromid>
    replytoid[0] = '\0';                         // RFC 822 Reply-To: <replytoid>
    returnpathid[0] = '\0';                      // RFC 822 Return-Path: <returnpath>subject[0] = '\0';
    subject[0] = '\0';
    organization[0] = '\0';
    aheaders1[0] = '\0';
    aheaders2[0] = '\0';
    xheaders[0] = '\0';
    AUTHLogin[0] = '\0';
    AUTHPassword[0] = '\0';

//    regerr = GetRegEntry();

    strcpy(senderid, Sender);
    strcpy(loginname, Sender);

    // thanks to Beverly Brown "beverly@datacube.com" for
    // fixing the argument parsing in v1.5
    for ( next_arg=1;next_arg < argc;next_arg++ ) {
        // if arg is a '-p' just increase and continue looking
        // we have already dealt with -p at the beginning
        if ( lstrcmp("-p",argv[next_arg])==0 ) {
            next_arg++;                          // increment once to step to the profile argument
        }

        // Added 23 Aug 2000 Craig Morrison
        // we've already got this one too..
        else if ( lstrcmp("-log",argv[next_arg])==0 ) {
            next_arg++;                          // skip log name argument
        }

        // we have already dealt with these at the beginning...
        else if ( (lstrcmp("-mime",argv[next_arg])==0)
                  || (lstrcmp("-enriched",argv[next_arg]) == 0)
                  || (lstrcmp("-html",argv[next_arg]) == 0)
                  || (lstrcmp("-base64",argv[next_arg]) == 0)
                  || (lstrcmp("-uuencode",argv[next_arg]) == 0)
                  || (lstrcmp("-plain",argv[next_arg]) == 0)    // added 27 Feb 2001 Craig Morrison
                  || (lstrcmp("-noh",argv[next_arg]) == 0)
                  || (lstrcmp("-noh2",argv[next_arg]) == 0)
                  || (lstrcmp("-debug",argv[next_arg]) == 0)
                  || (lstrcmp("-q",argv[next_arg]) == 0) ) {
            // next_arg++; Don't increment, just ignore.  Fixed tcharron@interlog.com
        }


        else if ( lstrcmpi("-smtphost",argv[next_arg])==0 ) {
                strcpy( SMTPHost, argv[++next_arg] );
				}

        else if ( lstrcmpi("-smtpport",argv[next_arg])==0 ) {
                strcpy( SMTPPort, argv[++next_arg] );
				}

        else if ( lstrcmpi("-sender",argv[next_arg])==0 ) {
                strcpy( Sender, argv[++next_arg] );
				}

        else if ( lstrcmpi("-try",argv[next_arg])==0 ) {
                strcpy( Try, argv[++next_arg] );
				}

/*
        // is argv[2] "-install"? If so, indicate error and return
        else if ( lstrcmpi("-install",argv[next_arg])==0 ) {
            if ( (argc >= 3) && (argc <= 7) ) {  //Correction: CH, 1998-06-15: replace || by &&
                strcpy( SMTPHost, argv[++next_arg] );
                strcpy( Sender, "" );
                if ( argc >= 4 ) strcpy( Sender, argv[++next_arg] );
                strcpy( Try, "1" );
                if ( argc >= 5 ) strcpy( Try, argv[++next_arg] );
                if ( !strcmp(Try,"-") ) strcpy(Try,"ONCE");
                if ( !strcmp(Try,"0") ) strcpy(Try,"ONCE");

                if ( argc >= 6 ) strcpy( SMTPPort, argv[++next_arg] );
                if ( !strcmp(SMTPPort,"-") ) strcpy(SMTPPort,"25");
                if ( !strcmp(SMTPPort,"0") ) strcpy(SMTPPort,"25");

                if ( argc == 7 ) strcpy( Profile, argv[++next_arg] );

                if ( CreateRegEntry() == 0 ) {
                    if ( ! quiet )  printMsg("\nSMTP server set to %s on port %s with user %s, retry %s time(s)\n", SMTPHost, SMTPPort, Sender, Try );
                    regerr = 0;
                    return(0);
                }
            } else {
                if ( ! quiet )
                    printMsg( "to set the SMTP server's address and the user name at that address do:\nblat -install server username");
                return(6);
            }
        }
*/
/*
        // is argv[2] "-profile"? If so, list or delete the profiles
        else if ( lstrcmpi("-profile",argv[next_arg])==0 ) {
            next_arg++;
            if ( (argc==2 && !quiet) || (argc==3 && quiet ) ) {
                ListProfiles("<all>");
            } else {
                if ( !strcmp(argv[next_arg],"-delete") ) {
                    next_arg++;
                    for ( i=next_arg; i<argc; i++ ) {
                        strcpy(Profile,argv[i]);
                        DeleteRegEntry();
                    }
                } else {
                    for ( i=next_arg; i<argc; i++ ) {
                        ListProfiles(argv[i]);
                    }
                }
            }
            return(0);
        }
*/
        else if ( lstrcmpi("-h",argv[next_arg])==0 ) {
			if ( !quiet ) printUsage();
            return(1);
        }

        // is argv[2] "-r"? If so, request Return-Receipt
        else if ( lstrcmpi("-r",argv[next_arg])==0 ) {
            returnreceipt=1;
        }

        // is argv[2] "-d"?  Disposition
        else if ( lstrcmpi("-d",argv[next_arg])==0 ) {
            disposition=1;
        }

        else if ( lstrcmp("-penguin",argv[next_arg])==0 ) {
            penguin = 1;
        }
/*
        else if ( next_arg == 1 ) {
            if ( lstrcmp(filename, "-") != 0 ) {
                if ( lstrlen(filename)<=0 || OpenFile(filename,&of,OF_EXIST) == HFILE_ERROR ) {
                    if ( ! quiet )   printMsg("%s does not exist\n",filename);
                    return(2);
                }
            }
        }
*/
        else if ( next_arg+1 < argc ) {          // Be sure there's another parameter supplied...
            // All the following require another parameter...

            // is argv[2] "-mailfrom"? If so, argv[3] is the RFC821 MAIL From: - added 2000-02-03 Axel Skough SCB-SE
            if ( lstrcmpi("-mailfrom",argv[next_arg])==0 ) {
                strcpy(loginname, argv[++next_arg] );
            }

            // is argv[2] "-from"? If so, argv[3] is the RFC822 From: - added 2000-02-03 Axel Skough SCB-SE
            else if ( lstrcmpi("-from",argv[next_arg])==0 ) {
                strcpy(fromid, argv[++next_arg] );
            }

            // is argv[2] "-replyto"? If so, argv[3] is the the RFC822 Reply-To: - added 2000-02-03 Axel Skough SCB-SE
            else if ( lstrcmpi("-replyto",argv[next_arg])==0 ) {
                strcpy(replytoid, argv[++next_arg] );
            }

            // is argv[2] "-returnpath"? If so, argv[3] is the the RFC822 Return-Path: - added 2000-02-03 Axel Skough SCB-SE
            else if ( lstrcmpi("-returnpath",argv[next_arg])==0 ) {
                strcpy(returnpathid, argv[++next_arg] );
            }

            // is argv[2] "-sender"? If so, argv[3] is the the RFC822 Sender: - added 2000-02-03 Axel Skough SCB-SE
            else if ( lstrcmpi("-sender",argv[next_arg])==0 ) {
                strcpy(sendername, argv[++next_arg] );
            }

            // is argv[2] "-s"/"-subject"? If so, argv[3] is the subject
            else if ( ( lstrcmpi("-s"      ,argv[next_arg])==0 ) ||
                      ( lstrcmpi("-subject",argv[next_arg])==0 ) ) {
                strcpy(subject, argv[++next_arg] );
            }

            // is argv[2] "-sf" If so, argv[3] is the subject text file.. 07 Jul 2000, Craig Morrison
            else if ( lstrcmpi("-sf" ,argv[next_arg])==0 ) {
                fileGetSubject(argv[++next_arg]);
            }

            // is argv[2] "-o"/"-org"/"-organization"? If so, argv[3] is the Organization
            else if ( ( lstrcmpi("-o",argv[next_arg])==0 ) ||
                      ( lstrcmpi("-org",argv[next_arg])==0 ) ||
                      ( lstrcmpi("-organization",argv[next_arg])==0 ) ) {
                strcpy(organization,argv[++next_arg]);
            }

            // is argv[] "-x"? If so, argv[3] is an X-Header
            // Header MUST start with X-
            else if ( lstrcmpi("-x",argv[next_arg])==0 ) {
                if ( (argv[next_arg+1][0] == 'X') && (argv[next_arg+1][1] == '-') ) {
                    if ( *xheaders ) {
                        strcat(xheaders, "\r\n");
                    }
                    strcat(xheaders,argv[++next_arg]);
                }
            }
            
			// is argv[] "-A"? If so, argv[3] is an Header that goes at the top of the message
            else if ( lstrcmpi("-a1",argv[next_arg])==0 ) {
                if ( *aheaders1 ) {
                    strcat(aheaders1, "\r\n");
				}
                strcat(aheaders1,argv[++next_arg]);
            }
            else if ( lstrcmpi("-a2",argv[next_arg])==0 ) {
                if ( *aheaders2 ) {
                    strcat(aheaders2, "\r\n");
				}
                strcat(aheaders2,argv[++next_arg]);
            }

            // is argv[2] "-cc"/"-c"? If so, argv[3] is the carbon-copy list
            else if ( ( lstrcmpi("-c" ,argv[next_arg])==0 ) ||
                      ( lstrcmpi("-cc",argv[next_arg])==0 ) ) {
                int cclen = strlen(argv[next_arg+1]);
                if ( cc_list != NULL ) {
                    printMsg("-c/-cc can not be used with -cf.  Aborting.\n");
                    return(1);
                } else {
                    cc_list = new char[cclen+1];
                    strcpy(cc_list,argv[++next_arg] );
                }
            }

            // is argv[2] "-cf"? If so, argv[3] is the carbon-copy list file
            else if ( lstrcmpi("-cf",argv[next_arg])==0 ) {
                if ( cc_list != NULL ) {
                    printMsg("-c/-cc can not be used with -cf.  Aborting.\n");
                    return(1);
                } else {
                    ret=ReadNamesFromFile("CC",argv[++next_arg], &cc_list);
                    if ( ret!=0 ) return(ret);
                }
            }

            // is argv[2] "-bcc"/"-b"? If so, argv[3] is the blind carbon-copy list
            else if ( ( lstrcmpi("-b",argv[next_arg])==0 ) ||
                      ( lstrcmpi("-bcc",argv[next_arg])==0 ) ) {
                if ( bcc_list != NULL ) {
                    printMsg("-b/-bcc can not be used with -bf.  Aborting.\n");
                    return(1);
                } else {
                    int bcclen = strlen(argv[next_arg+1]);
                    bcc_list = new char[bcclen+1];
                    strcpy(bcc_list,argv[++next_arg] );
                }
            }

            // is argv[2] "-bf"? If so, argv[3] is the blind carbon-copy list file
            else if ( lstrcmpi("-bf",argv[next_arg])==0 ) {
                if ( bcc_list != NULL ) {
                    printMsg("-b/-bcc can not be used with -bf.  Aborting.\n");
                    return(1);
                } else {
                    ret=ReadNamesFromFile("BCC",argv[++next_arg], &bcc_list);
                    if ( ret!=0 ) return(ret);
                }
            }

            // is next argv "-t"/"-to"? If so, succeeding argv is the destination
            else if ( ( lstrcmpi("-t" ,argv[next_arg])==0 ) ||
                      ( lstrcmpi("-to",argv[next_arg])==0 ) ) {
                if ( destination != NULL ) {
                    printMsg("-t/-to can not be used with -tf.  Aborting.\n");
                    return(1);
                } else {
                    int destlen = strlen(argv[next_arg+1]);
                    destination = new char[destlen+1];
                    strcpy(destination, argv[++next_arg] );
                }
            }

            // is argv[2] "-tf"? If so, argv[3] is the destination list file
            else if ( lstrcmpi("-tf",argv[next_arg])==0 ) {
                if ( destination != NULL ) {
                    printMsg("-t/-to can not be used with -tf.  Aborting.\n");
                    return(1);
                } else {
                    ret=ReadNamesFromFile("TO",argv[++next_arg], &destination);
                    if ( ret!=0 ) return(ret);
                }
            }

            else if ( lstrcmpi("-ti",argv[next_arg])==0 ) {
                globaltimeout = atoi(argv[++next_arg]);
            }

            // is next argv "-server"? If so, succeeding argv is the SMTPHost
            else if ( lstrcmpi("-server",argv[next_arg])==0 ) {
                strcpy(SMTPHost,argv[++next_arg]);
            }

            // is next argv "-hostname"? If so, succeeding argv is the SMTPHost
            else if ( lstrcmpi("-hostname",argv[next_arg])==0 ) {
                strcpy(my_hostname_wanted,argv[++next_arg]);
            }

            // is next argv "-port"? If so, succeeding argv is the SMTPPort
            else if ( lstrcmpi("-port",argv[next_arg])==0 ) {
                strcpy(SMTPPort,argv[++next_arg]);
            }

            // Toby Korn 8/4/1999
            // Check for priority. 0 is Low, 1 is High - you don't need
            // normal, since omission of a priority is normal.
            else if ( lstrcmpi ("-priority", argv[next_arg])==0 ) {
	            strncpy(priority,argv[++next_arg],1);
            }

            // is next argv "-try"? If so, succeeding argv is the number of tries
            else if ( lstrcmpi("-try",argv[next_arg])==0 ) {
                strcpy(Try,argv[++next_arg]);
            }

            //is next argv '-f'? If so, succeeding argv is the loginname
            else if ( lstrcmp("-f",argv[next_arg])==0 ) {
                strcpy(loginname, argv[++next_arg]);
            }

            //is next argv '-body'? If so, succeeding argv is the message body
            else if ( lstrcmp("-body",argv[next_arg])==0 ) {
                bodyparameter=++next_arg;
                strcpy(filename, "-");
            }

            // -attacht added 98.4.16 v1.7.2 tcharron@interlog.com
            else if ( lstrcmp("-attacht",argv[next_arg])==0 ) {
                if ( attach == 64 ) {
                    printMsg("Max of 64 files allowed!  Others are being ignored\n");
                } else {
                    attachtype[attach]=0;        // text
                    strcpy( (char *) &attachfile[attach++][0], argv[++next_arg] );
                }
            }

            // -attach added 98.2.24 v1.6.3 tcharron@interlog.com
            else if ( lstrcmp("-attach",argv[next_arg])==0 ) {
                if ( attach == 64 ) {
                    printMsg("Max of 64 files allowed!  Others are being ignored\n");
                } else {
                    attachtype[attach]=1;        // binary
                    strcpy( (char *) &attachfile[attach++][0], argv[++next_arg] );
                }
            }

            //is next argv '-i'? If so, succeeding argv is the sender id
            else if ( lstrcmp("-i",argv[next_arg])==0 ) {
                strcpy(senderid, argv[++next_arg]);
                impersonating = 1;
            }

            // is argv[2] "-u"/"-username"? If so, argv[3] is the username for auth
            else if ( ( lstrcmpi("-u"      ,argv[next_arg])==0 ) ||
                      ( lstrcmpi("-username",argv[next_arg])==0 ) ) {
                strcpy(AUTHLogin, argv[++next_arg] );
            }

            // is argv[2] "-pw"/"-password"? If so, argv[3] is the username for auth
            else if ( ( lstrcmpi("-pw"      ,argv[next_arg])==0 ) ||
                      ( lstrcmpi("-password",argv[next_arg])==0 ) ) {
                strcpy(AUTHPassword, argv[++next_arg] );
            }

            // is argv[] "-charset"?
            else if ( lstrcmpi("-charset",argv[next_arg])==0 ) {
                strcpy(charset, argv[++next_arg] );
            }

            else {
                if ( ! quiet ) printUsage();
                return(1);
            }

        }

        else {
            if ( ! quiet ) printUsage();
            return(1);
        }
    }

    // Set up some defaults if needed.
    if ( NULL ==     cc_list ) {
        cc_list = new char[2]; strcpy(    cc_list,"");
    }
    if ( NULL ==    bcc_list ) {
        bcc_list = new char[2]; strcpy(   bcc_list,"");
    }
    if ( NULL == destination ) {
        destination = new char[2]; strcpy(destination,"");
    }

//    if ( (regerr==12) && (!quiet) ) printMsg( "Failed to open registry key for Blat\n" );

    // if we are not impersonating loginname is the same as the sender
    if ( ! impersonating )
        strcpy(senderid, loginname);

    // fixing the argument parsing
    // Ends here

    if ( (SMTPHost[0]=='\0')||(loginname[0]=='\0') ) {
        if ( ! quiet ) {
            printMsg( "to set the SMTP server's address and the user name at that address do:\nblat -install server username\n");
            printMsg( "or use '-server <server name>' and '-f <user name>'\n");
            printMsg( "aborting, nothing sent\n" );
        }
        if ( bcc_list ) delete [] bcc_list;
        if ( cc_list )   delete [] cc_list;
        if ( destination ) delete [] destination;
        return(12);
    }

    // make sure filename exists, get full pathname
    if ( lstrcmp(filename, "-") != 0 )
        if ( lstrlen(filename)<=0 || OpenFile(filename,&of,OF_EXIST)==HFILE_ERROR ) {
            if ( ! quiet )  printMsg("%s does not exist\n",filename);
            if ( bcc_list ) delete [] bcc_list;
            if ( cc_list )   delete [] cc_list;
            if ( destination ) delete [] destination;
            return(2);
        }

        // build temporary recipients list for parsing the "To:" line
    char *temp = new char [ strlen(destination) + strlen(cc_list) + strlen(bcc_list) + 4 ];
    // build the recipients list
    Recipients = new char [ strlen(destination) + strlen(cc_list) + strlen(bcc_list) + 4 ];

    // Parse the "To:" line
    for ( i = j = 0; i < (int) strlen(destination); i++ ) {
        // strip white space
        while ( destination[i]==' ' )   i++;
        // look for comments in brackets, and omit
        if ( destination[i]=='(' ) {
            while ( destination[i]!=')' )   i++;
            i++;
        }
        temp[j++] = destination[i];
    }
    temp[j] = '\0';                              // End of list added!
    strcpy( Recipients, temp);

    // Parse the "Cc:" line
    for ( i = j = 0; i < (int) strlen(cc_list); i++ ) {
        // strip white space
        while ( cc_list[i]==' ' ) i++;
        // look for comments in brackets, and omit
        if ( cc_list[i]=='(' ) {
            while ( cc_list[i]!=')' ) i++;
            i++;
        }
        temp[j++] = cc_list[i];
    }
    temp[j] = '\0';                              // End of list added!
    if ( strlen(cc_list) > 0 ) {
        if ( strlen(Recipients)>0 ) strcat(Recipients, "," );
        strcat(Recipients, temp);
    }

    // Parse the "Bcc:" line
    for ( i = j = 0; i < (int) strlen(bcc_list); i++ ) {
        // strip white space
        while ( bcc_list[i]==' ' )  i++;
        // look for comments in brackets, and omit
        if ( bcc_list[i]=='(' ) {
            while ( bcc_list[i]!=')' )  i++;
            i++;
        }
        temp[j++] = bcc_list[i];
    }
    temp[j] = '\0';                              // End of list added!
    if ( strlen(bcc_list) > 0 ) {
        if ( strlen(Recipients)>0 ) strcat(Recipients, "," );
        strcat(Recipients, temp);
    }
    delete[] temp;

    // Generate a unique message boundary identifier.
    // Added 1.7.6 by Tim Charron (tcharron@interlog.com)
    srand( (unsigned int) time( NULL ) );
    char boundary1[24];
	char boundary2[25];
    char abclist[63]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for ( i=0 ; i<=20 ; i++ ) boundary1[i] = abclist[rand()%62];
    boundary1[21]=0;
	strcpy(boundary2, boundary1);
    strcat(boundary1,"\r\n");
	strcat(boundary2,"\"\r\n");

    // create a header for the message
    char tmpstr[0x200];
    char tmpstr1[0x200];
    char tmpstr2[0x200];
    char header[0x2000];
    char header2[0x2000];
    int  headerlen;
    SYSTEMTIME curtime;
    TIME_ZONE_INFORMATION tzinfo;
    char * days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    char * months[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    DWORD retval;

    GetLocalTime( &curtime );
    retval = GetTimeZoneInformation( &tzinfo );
    hours = (int) tzinfo.Bias / 60;
    minutes = (int) tzinfo.Bias % 60;
    if ( retval == TIME_ZONE_ID_STANDARD ) {
        hours += (int) tzinfo.StandardBias / 60;
        minutes += (int) tzinfo.StandardBias % 60;
    } else {
        hours += (int) tzinfo.DaylightBias / 60;
        minutes += (int) tzinfo.DaylightBias % 60;
    }

    strcpy( header, "" );

    if ( *aheaders1 ) {
        sprintf( tmpstr, "%s\r\n", aheaders1 );
        strcat( header, tmpstr);
    }
    if ( *aheaders2 ) {
        sprintf( tmpstr, "%s\r\n", aheaders2 );
        strcat( header, tmpstr);
    }

	// rfc1036 & rfc822 acceptable format
    // Mon, 29 Jun 1994 02:15:23 GMT
    sprintf (tmpstr, "Date: %s, %.2d %s %.4d %.2d:%.2d:%.2d ",
             days[curtime.wDayOfWeek],
             curtime.wDay,
             months[curtime.wMonth - 1],
             curtime.wYear,
             curtime.wHour,
             curtime.wMinute,
             curtime.wSecond);
    strcat( header, tmpstr );
    strcpy( header2, "");

    sprintf( tmpstr, "%+03d%02d", -hours, -minutes );
    //for(i=0;i<32;i++)
    //{
    // if( retval == TIME_ZONE_ID_STANDARD ) tmpstr[i] = (char) tzinfo.StandardName[i];
    // else tmpstr[i] = (char) tzinfo.DaylightName[i];
    //}
    strcat( header, tmpstr );
    strcat( header, "\r\n" );

    // RFC 822 From: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
    if ( *fromid ) {
        if ( fixup(fromid,NULL) ) mime=1;
        sprintf( tmpstr, "From: %s\r\n", fromid );
    } else {
        if ( fixup(senderid,NULL) ) mime=1;
        sprintf( tmpstr, "From: %s\r\n", senderid );
    }
    strcat( header, tmpstr );

    // now add the Received: from x.x.x.x by y.y.y.y with HTTP;
    if ( lpszFirstReceivedData!=NULL )
        if ( (*lpszFirstReceivedData)!='\0' ) {
            sprintf(tmpstr,"%s%s, %.2d %s %.2d %.2d:%.2d:%.2d %+03d%02d\r\n",
                    lpszFirstReceivedData,
                    days[curtime.wDayOfWeek],
                    curtime.wDay,
                    months[curtime.wMonth - 1],
                    curtime.wYear,
                    curtime.wHour,
                    curtime.wMinute,
                    curtime.wSecond,
                    -hours, -minutes);
            strcat(header,tmpstr);
        }
    if ( impersonating ) {
        if ( fixup(loginname,NULL) ) mime=1;
        sprintf( tmpstr, "Sender: %s\r\n", loginname );
        strcat( header, tmpstr );
        if ( !(penguin == 1) ) {
            sprintf( tmpstr, "Reply-to: %s\r\n", loginname );
            strcat( header, tmpstr );
        }
    } else {
        // RFC 822 Sender: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
        if ( *sendername ) {
            if ( fixup(sendername,NULL) )   mime=1;
            sprintf( tmpstr, "Sender: %s\r\n", sendername );
            strcat( header, tmpstr );
        }
        // RFC 822 Reply-To: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
        if ( *replytoid ) {
            if ( fixup(replytoid,NULL) ) mime=1;
            sprintf( tmpstr, "Reply-To: %s\r\n", replytoid );
            strcat( header, tmpstr );  
        }
    }
    if ( *subject ) {
        if ( fixup(subject,NULL) )  mime=1;
        sprintf( tmpstr, "Subject: %s\r\n", subject );
        strcat( header, tmpstr );
    } else {
        if ( !(penguin == 1) ) {
            if ( lstrcmp(filename, "-") == 0 )
                sprintf( tmpstr, "Subject: Contents of console input\r\n" );
            else
                sprintf( tmpstr, "Subject: Contents of file: %s\r\n", filename );
            strcat( header, tmpstr );
        }
    }

    if ( fixup(destination, &tempstring) )  mime=1;
    sprintf( tmpstr, "To: %s\r\n", tempstring );
    strcat( header, tmpstr );
    delete [] tempstring;

    if ( *cc_list ) {
        // Add line for the Carbon Copies
        if ( fixup(cc_list, &tempstring) ) mime=1;
        sprintf( tmpstr, "Cc: %s\r\n", tempstring );
        strcat( header, tmpstr );
        delete [] tempstring;
    }

    delete [] destination;
    delete [] bcc_list;
    delete [] cc_list;

    if ( *organization ) {
        if ( fixup(organization,NULL) ) mime=1;
        sprintf( tmpstr, "Organization: %s\r\n", organization );
        strcat( header, tmpstr);
    }

    // To use loginname for the RFC 822 Disposition and Return-receipt fields doesn't seem to be unambiguous. 
    // Either separate definitions should be used for these fileds to get full flexibility or - as a compromise -
    // the content of the Reply-To. field would rather be used when specified. 2000-02-03 Axel Skough SCB-SE 

    if ( disposition ) {
        if ( *replytoid ) {
            sprintf( tmpstr, "Disposition-Notification-To: %s\r\n", replytoid );
            strcat( header, tmpstr);
        } else {
            sprintf( tmpstr, "Disposition-Notification-To: %s\r\n", loginname );
            strcat( header, tmpstr);
        }
    }
    if ( returnreceipt ) {
        if ( *replytoid ) {
            sprintf( tmpstr, "Return-Receipt-To: %s\r\n", replytoid );
            strcat( header, tmpstr);
        } else {
            sprintf( tmpstr, "Return-Receipt-To: %s\r\n", loginname );
            strcat( header, tmpstr);
        }
    }
    if ( *xheaders ) {
        sprintf( tmpstr, "%s\r\n", xheaders );
        strcat( header, tmpstr);
    }

    // RFC 822 Return-Path: definition in Blat entered 2000-02-03 Axel Skough SCB-SE
    if ( *returnpathid ) {
        if ( fixup(returnpathid,NULL) ) mime=1;
        sprintf( tmpstr, "Return-Path: %s\r\n", returnpathid );
        strcat( header, tmpstr );
    }

    // Toby Korn tkorn@snl.com 8/4/1999
    // If priority is specified on the command line, add it to the header
    if ( priority [0] == '0') {
    	sprintf (tmpstr, "X-MSMail-Priority: Low\r\nX-Priority: 5\r\n");
    	strcat (header, tmpstr);
    }
    else if (priority [0] == '1') {
    	sprintf (tmpstr, "X-MSMail-Priority: High\r\nX-Priority: 1\r\n");
    	strcat (header, tmpstr);
    }

    // This is either mime, base64, uuencoded, or neither.  With or without attachments.  Whew!
    if ( (!mime) && (!base64) ) {
        if ( attach>0 ) {
            sprintf( tmpstr, "MIME-Version: 1.0\r\n" );
            strcat( header, tmpstr );
            sprintf( tmpstr, "Content-Type: Multipart/Mixed; boundary=\"BlatBoundary-");
            strcat(tmpstr, boundary2);
            strcat( header, tmpstr );

            sprintf( tmpstr, "\r\n--BlatBoundary-" );
            strcat( tmpstr, boundary1 );
            strcat( header2, tmpstr );
            sprintf( tmpstr, "Content-Type: text/%s; charset=US-ASCII\r\n", textmode);    // modified 15. June 1999 by JAG
            strcat( header2, tmpstr );
            sprintf( tmpstr, "Content-Transfer-Encoding: 7BIT\r\n" );
            strcat( header2, tmpstr );
            sprintf( tmpstr, "Content-description: Mail message body\r\n" );
            strcat( header2, tmpstr );
        }
    }
    if ( mime ) {
        // Indicate MIME version and type
        sprintf( tmpstr, "MIME-Version: 1.0\r\n" );
        strcat( header, tmpstr );
        sprintf( tmpstr1, "Content-Type: text/%s; charset=%s\r\n", textmode,charset);    // modified 15. June 1999 by JAG
        sprintf( tmpstr2, "Content-Transfer-Encoding: quoted-printable\r\n" );
        if ( attach>0 ) {
            sprintf( tmpstr, "Content-Type: Multipart/Mixed; boundary=\"BlatBoundary-" );
            strcat( tmpstr, boundary2 );
            strcat( header, tmpstr );

            sprintf( tmpstr, "\r\n--BlatBoundary-" );
            strcat( tmpstr, boundary1 );
            strcat( header2, tmpstr );
            strcat( header2, tmpstr1 );
            strcat( header2, tmpstr2 );
        } else {
            strcat( header, tmpstr1 );
            strcat( header, tmpstr2 );
        }
    }
    if ( base64 ) {
        // Indicate MIME version and type
        sprintf( tmpstr, "MIME-Version: 1.0\r\n" );
        strcat( header, tmpstr );
        sprintf( tmpstr, "Content-Type: Multipart/Mixed; boundary=\"BlatBoundary-" );
        strcat( tmpstr, boundary2 );
        strcat( header, tmpstr );
        sprintf( tmpstr, "\r\n--BlatBoundary-" );
        strcat( tmpstr, boundary1 );
        strcat( header2, tmpstr );
        if ( lstrcmp(filename, "-") == 0 ) {
            sprintf( tmpstr, "Content-Type: application/octet-stream; name=stdin.txt\r\n" );
        } else {
            sprintf( tmpstr, "Content-Type: application/octet-stream; name=%s\r\n",
                     GetNameWithoutPath(filename) );
        }
        strcat( header2, tmpstr );
        if ( lstrcmp(filename, "-") == 0 ) {
            sprintf( tmpstr, "Content-Disposition: attachment; filename=\"stdin.txt\"\r\n");
        } else {
            sprintf( tmpstr, "Content-Disposition: attachment; filename=\"%s\"\r\n",
                     GetNameWithoutPath(filename) );
        }
        strcat( header2, tmpstr );
        sprintf( tmpstr, "Content-Transfer-Encoding: BASE64\r\n" );
        strcat( header2, tmpstr );
    }

    if ( noheader == 0 ) {
        strcat( header, "X-Mailer: WinNT's Blat ver " VERSION " http://www.blat.net\r\n" );
    } else if ( noheader == 1 ) {
        strcat( header, "X-Mailer: WinNT's Blat ver " VERSION "\r\n" );
    }                                            // noheader==2 means to omit X-Mailer header.
    strcat( header, header2 );

    if ( lpszOtherHeader!=NULL ) {
        strcat(header,lpszOtherHeader);
    }
    if ( !(penguin == 1) )
        strcat( header, "\r\n" );

    headerlen = strlen( header );

    // if reading from the console, read everything into a temporary file first
    if ( lstrcmp(filename, "-") == 0 ) {

        if ( lpszMessageCgi!=NULL ) {
            InitTempConsoleBuffer(&dwTempConsoleBufferSize,
                                  &dwTempConsoleBufferLength,
                                  &lpszTempConsoleBuffer);
            AddStringORCharToTempConsoleBuffer(
                                              &dwTempConsoleBufferSize,
                                              &dwTempConsoleBufferLength,
                                              &lpszTempConsoleBuffer,
                                              lpszMessageCgi,0);
        } else if ( bodyparameter != 0 ) {
            i=0;
            InitTempConsoleBuffer(&dwTempConsoleBufferSize,
                                  &dwTempConsoleBufferLength,
                                  &lpszTempConsoleBuffer);

            while ( argv[bodyparameter][i]!=0 ) {
                if ( argv[bodyparameter][i] == '|' ) {    // CRLF signified by the pipe character
                    AddStringORCharToTempConsoleBuffer(
                                                      &dwTempConsoleBufferSize,
                                                      &dwTempConsoleBufferLength,
                                                      &lpszTempConsoleBuffer,
                                                      NULL,0x0D);
                    AddStringORCharToTempConsoleBuffer(
                                                      &dwTempConsoleBufferSize,
                                                      &dwTempConsoleBufferLength,
                                                      &lpszTempConsoleBuffer,
                                                      NULL,0x0A);
                } else {
                    AddStringORCharToTempConsoleBuffer(
                                                      &dwTempConsoleBufferSize,
                                                      &dwTempConsoleBufferLength,
                                                      &lpszTempConsoleBuffer,
                                                      NULL,argv[bodyparameter][i]);
                }
                i++;
            }
        } else {
            InitTempConsoleBuffer(&dwTempConsoleBufferSize,
                                  &dwTempConsoleBufferLength,
                                  &lpszTempConsoleBuffer);

            do {
                DWORD dwNbRead = 0;
                i=0;

                if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),&i,1,&dwNbRead,NULL) )    i=EOF;
                if ( dwNbRead == 0 )    i=EOF;
                if ( i == '\x1A' ) i=EOF;
                if ( i != EOF )
                    AddStringORCharToTempConsoleBuffer(
                                                      &dwTempConsoleBufferSize,
                                                      &dwTempConsoleBufferLength,
                                                      &lpszTempConsoleBuffer,
                                                      NULL,(char)i);
            } while ( i != EOF );
        }
        strcpy(filename, "stdin.txt");
    }

    DWORD filesize ;

    if ( dwTempConsoleBufferSize==0 ) {
        //get the text of the file into a string buffer
        if ( (fileh=CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN,NULL))==INVALID_HANDLE_VALUE ) {
            if ( ! quiet )  printMsg("error reading %s, aborting\n",filename);
            delete [] Recipients;
            return(3);
        }
        if ( GetFileType(fileh)!=FILE_TYPE_DISK ) {
            if ( ! quiet )  printMsg("Sorry, I can only mail messages from disk files...\n");
            delete [] Recipients;
            return(4);
        }
        filesize = GetFileSize( fileh,NULL );
    } else {
        filesize = dwTempConsoleBufferLength;
    }

    // Oversized buffer for output message...
    // Quoted printable takes the most space...up to 3 bytes/byte + LFs
    // base64 uses CR every 54 bytes
    // 0x1000 is for the minimal Mime/UUEncode header
    int bufsize;
    bufsize = (3*(filesize+filesize/54))+headerlen+1+0x1000+strlen(filename);

    char *buffer = new char[bufsize];
    char *filebuffer = new char[filesize+1+4];
    char *tmpptr;
    char *q;
    char *p;

    // put the header at the top...
    strcpy( buffer, header );
    // point to the end of the header
    tmpptr = buffer + headerlen;
    // and put the whole file there
    DWORD dummy;

    if ( dwTempConsoleBufferSize==0 ) {
        // TF added check for INVALID_HANDLE_VALUE
        if ( INVALID_HANDLE_VALUE == fileh || !ReadFile(fileh,filebuffer,filesize,&dummy,NULL) ) {
            if ( ! quiet )  printMsg("error reading %s, aborting\n",filename);
            CloseHandle(fileh);
            delete [] buffer;
            delete [] filebuffer;
            delete [] Recipients;
            return(5);
        }
        CloseHandle(fileh);
    } else {
        // TF added check
        if ( lpszTempConsoleBuffer ) {
            memmove(filebuffer,lpszTempConsoleBuffer,filesize);
            free(lpszTempConsoleBuffer);
        }
        lpszTempConsoleBuffer=NULL;
        dwTempConsoleBufferLength = dwTempConsoleBufferSize = 0;
    }

    q = filebuffer + filesize;
    (*q) = (*(q+1)) = (*(q+2)) = (*(q+3)) = (*(q+4)) = '\0';

    // MIME Quoted-Printable Content-Transfer-Encoding
    // or BASE64 encoding of main file.
    // or UUencoding of main file
    // or nothing special...
    if ( ! mime ) {
        if ( ! base64 ) {
            if ( ! uuencode ) {
                strcpy( tmpptr, filebuffer );
            } else {
                douuencode(filebuffer,tmpptr,filesize,filename);
            }
        } else {
            base64_encode(filebuffer,q,tmpptr);
        }
    } else {
        int PosValue = 0;
        int *Pos;
//        char workbuf [8];
        Pos = &PosValue;
        p = filebuffer;
        while ( p < q ) {
			// Changed in blat 1.9.4 for speed
//          ConvertToQuotedPrintable(*p,Pos, workbuf, 0);
//          strcat(tmpptr,workbuf);
			ConvertToQuotedPrintable(*p,Pos, tmpptr, 0);
			tmpptr += strlen(tmpptr);

			p++;
        }
    }

    if ( lpszTempConsoleBuffer != NULL ) {
        free(lpszTempConsoleBuffer) ;
        dwTempConsoleBufferLength = 0 ;
        dwTempConsoleBufferSize = 0 ;
    }

    // make some noise about what we are doing
    if ( ! quiet ) {
        printMsg("Sending %s to %s\n",filename,(lstrlen(Recipients) ? Recipients : "<unspecified>"));
        if ( lstrlen(subject) ) printMsg("Subject:%s\n",subject);
        if ( lstrlen(loginname) ) printMsg("Login name is %s\n",loginname);
    }

    // Process any attachments
    HANDLE ihandle;
    int filewasfound=0;
    char path[MAX_PATH+1];
    char * pathptr;
    if ( attach>0 ) {
        for ( i=0;i<attach;i++ ) {
            if ( !quiet ) {
                if ( attachtype[i] == 1 ) {
                    printMsg("Attached binary file: %s\n",attachfile[i]);
                } else {
                    printMsg("Attached text file: %s\n",attachfile[i]);
                }
            }

// Modification C.Henquin, 1998-06-12: Add support for wildcard

            // Get the path for opening file
            strcpy(path,attachfile[i]);
            pathptr=strrchr(path,'\\');
            if ( NULL != pathptr ) {
                *(pathptr+1) = 0;
            } else {
                strcpy(path, "");
            }

            ihandle = FindFirstFile(attachfile[i], & FindFileData);
            if ( ihandle == INVALID_HANDLE_VALUE ) {
                if ( !quiet ) printMsg("File not found: %s\n",attachfile[i]);
                filewasfound=0;
            } else {
                filewasfound=1;
            }
            while ( filewasfound != 0 ) {
                //Added 1.8.4 by A.Bergamini (abergamini@elsag.it) 1999-06-15
                // Skip Directory. Useful when using wildchar in attach (* or *.*)
                if ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
                    if ( ! quiet )   printMsg("  ignoring %s%s\n",path,FindFileData.cFileName);
                } else {
                    strcpy(attachedfile, path);
                    strcat(attachedfile,FindFileData.cFileName);
                    shortname = strrchr(attachedfile,'\\');
                    if ( shortname==NULL ) {
                        shortname=attachedfile;
                    } else {
                        shortname = shortname+1;
                    }
                    if ( (!quiet) && (   (NULL!=strchr(attachfile[i],'?'))
                                         || (NULL!=strchr(attachfile[i],'*')) ) )
                        printMsg("  found %s\n",attachedfile);
                    // Do the header bit...
                    sprintf( header, "\r\n--BlatBoundary-" );
                    strcat( header, boundary1 );
                    if ( attachtype[i] == 1 ) {
                        // 9/18/1998 by Toby Korn
                        // Replaced default Content-Type with a lookup based on file extension
                        SetFileType (tmpstr1, shortname);
                        //sprintf( tmpstr1, "Content-Type: application/octet-stream; name=%s\r\n", shortname );
                        sprintf( tmpstr2, "Content-Disposition: attachment; filename=\"%s\"\r\n", shortname );
                        strcat(  tmpstr2, "Content-Transfer-Encoding: BASE64\r\n" );
                    } else {
                        sprintf( tmpstr1, "Content-Type: text/%s; charset=US-ASCII\r\n", textmode);    // modified 15. June 1999 by JAG
                        strcat(  tmpstr1, "Content-Disposition: inline\r\n");
                        sprintf( tmpstr2, "Content-description: %s\r\n", shortname );
                    }
                    strcat( header, tmpstr1 );
                    strcat( header, tmpstr2 );

                    if ( !(penguin == 1) )
                        strcat( header, "\r\n" );
                    headerlen = strlen( header );
                    bufsize += headerlen;

                    // TF can't do this: buffer was allocated with new, not  malloc and therefore should not be realloc'd
                    // buffer = (char *) realloc( buffer , bufsize );
                    buffer = new_realloc( buffer, bufsize );

                    // put the header at the end of existing message...
                    strcat( buffer, header );

                    //get the text of the file into a string buffer
                    if ( (fileh=CreateFile(attachedfile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                                           FILE_FLAG_SEQUENTIAL_SCAN,NULL))==INVALID_HANDLE_VALUE ) {
                        if ( ! quiet )   printMsg("error reading %s, aborting\n",attachedfile);
                        delete [] Recipients;
                        return(3);
                    }
                    if ( GetFileType(fileh)!=FILE_TYPE_DISK ) {
                        if ( ! quiet )   printMsg("Sorry, I can only mail messages from disk files...\n");
                        delete [] Recipients;
                        return(4);
                    }
                    filesize = GetFileSize( fileh,NULL );

                    // Until 1.7.3, this was using 72 instead of 54.  tcharron@interlog.com
                    bufsize += (4*(filesize+3))/3+2*((filesize+53)/54)+3 ;

                    // TF see above
                    // buffer = (char *) realloc( buffer , bufsize );
                    buffer = new_realloc( buffer, bufsize );

                    // TF see above
                    // filebuffer = (char *) realloc( filebuffer, filesize+1+4 );
                    filebuffer = new_realloc( filebuffer, filesize+1+4 );

                    if ( !ReadFile(fileh,filebuffer,filesize,&dummy,NULL) ) {
                        if ( ! quiet )   printMsg("error reading %s, aborting\n",filename);
                        CloseHandle(fileh);
                        delete [] buffer;
                        delete [] filebuffer;
                        delete [] Recipients;
                        return(5);
                    }
                    CloseHandle(fileh);

                    q = filebuffer + filesize;
                    (*q) = (*(q+1)) = (*(q+2)) = (*(q+3)) = (*(q+4)) = '\0';

                    tmpptr = buffer + strlen(buffer);
                    p = filebuffer;
                    if ( attachtype[i] == 1 ) {
                        base64_encode(p,q,tmpptr);
                    } else {
                        while ( p<=q ) {
                            *tmpptr = *p;
                            tmpptr++;
                            p++;
                        }
                    }
                }
                filewasfound=FindNextFile(ihandle, & FindFileData);
            }
            FindClose( ihandle );
        }
    }

    // v1.7.9  9/10/1998 by Toby Korn (tkorn@snl.com)
    // Added this to add a terminating MIME boundary - omission of
    // a terminating boundary was causing some e-mail systems to not
    // process the message properly.
    // Removed 1.8 -- this addition confuses some e-mail systems!
    // 1.8.2 -- Added back in with required correction from Toby Korn
    //   Compuserve did not process messages with out this bit.
    if ( (attach>0) || (base64) ) {
        bufsize += 23 + strlen(boundary1);
        // buffer=(char *)realloc(buffer,bufsize);
        buffer=new_realloc(buffer,bufsize);
        strcat (buffer, "\r\n--BlatBoundary-");
        strcat (buffer, boundary1 );
        buffer[strlen(buffer) - 2] = '\0';
        strcat (buffer, "--\r\n");
    }

    // send the message to the SMTP server!
    n_of_try = noftry();
    for ( k=1; k<=n_of_try || n_of_try == -1; k++ ) {
        if ( (!quiet) && (n_of_try > 1) ) printMsg("\nTry number %d of %d.\n", k, n_of_try);

        if ( k > 1 ) Sleep(15000);

        retcode = prepare_smtp_message( Recipients, my_hostname_wanted, AUTHLogin, AUTHPassword);
        if ( 0 == retcode ) {
            retcode = send_smtp_edit_data( buffer );
            if ( 0 == retcode ) {
                finish_smtp_message();
                n_of_try = k;
                k = n_of_try+1;
            }
        } else if ( -2 == retcode ) {
            // This wasn't a connection error.  The server actively denied our connection.
            // Stop trying to send mail.
            k = n_of_try+1;
        }
        close_smtp_socket();
    }

    delete [] buffer;
    delete [] filebuffer;
    delete [] Recipients;

    if ( fCgiWork ) {
        int i;                                   //lint !e578 hiding i
        for ( i=0;i<argc;i++ ) free(argv[i]);
        free(argv);
        LPSTR lpszUrl = (retcode == 0) ? lpszCgiSuccessUrl : lpszCgiFailureUrl;
        DWORD dwLenUrl=0;
        if ( lpszUrl !=NULL ) dwLenUrl=lstrlen(lpszUrl);

        LPSTR lpszCgiText=(LPSTR)malloc(1024+(dwLenUrl*4));

        // TF added lpszCgiText check
        if ( lpszCgiText && dwLenUrl>0 ) {
            wsprintf(lpszCgiText,
                     "Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n" \
                     "Pragma: no-cache\r\n" \
                     "Location: %s\r\n" \
                     "\r\n" \
                     "<html><body>\r\n" \
                     "<a href=\"%s\">Click here to go to %s</a>\r\n"\
                     "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0; URL=%s\">\r\n"\
                     "</body></html>\r\n" ,
                     lpszUrl,lpszUrl,lpszUrl,lpszUrl);
            // TF added lpszCgiText check
        } else if ( lpszCgiText ) {
            wsprintf(lpszCgiText,
                     "Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n" \
                     "Pragma: no-cache\r\n" \
                     "Content-type: text/html\r\n" \
                     "\r\n" \
                     "<html><body>\r\n" \
                     "Blat sending message result = %d : %s\r\n"\
                     "</body></html>\r\n" ,
                     retcode,(retcode==0)?"Success":"Failure");
        }

        // TF added check, none of these functions like NULL
        if ( lpszCgiText ) {
            DWORD dwSize=lstrlen(lpszCgiText);
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),lpszCgiText,dwSize,&dwSize,NULL);
            free(lpszCgiText);
        }
    }
    if ( lpszMessageCgi!=NULL ) free(lpszMessageCgi);
    if ( lpszCgiSuccessUrl!=NULL ) free(lpszCgiSuccessUrl);
    if ( lpszFirstReceivedData!=NULL ) free(lpszFirstReceivedData);
    if ( lpszCgiFailureUrl!=NULL ) free(lpszCgiFailureUrl);
    if ( lpszOtherHeader!=NULL ) free(lpszOtherHeader);

    if ( logOut )                                // Added 23 Aug 2000 Craig Morrison
        fclose(logOut);

    return(abs(retcode));
}

// 07 Jul 2000, moved out of main to make -sf easier to implement (avoid duplicating code)
// 08 Jul 2000, at Gerald's request, modified to use the filename itself if the file doesn't exist
void fileGetSubject(char *fn)
{
    FILE *infile = fopen(fn, "r");
    if ( infile!=NULL ) {
        memset(subject, 0x00, SUBJECT_SIZE);
        fgets(subject, SUBJECT_SIZE, infile);    //lint !e534 ignore return
        if ( strlen(subject) ) {
            if ( subject[strlen(subject)-1]=='\n' ) {
                subject[strlen(subject)-1]=0x00;
            }
        }
        fclose(infile);
    } else {
        strncpy(subject, fn, SUBJECT_SIZE-1);
        subject[SUBJECT_SIZE-1] = 0x00;
    }
}

/* 
     Added 23 Aug 2000, Craig Morrison

     Provides a central location to perform message output.

     Works just like printf but allows us to output to a file
     instead of just to stdout.. -q overrides this.

*/


void printMsg(const char *p, ... )
{
    va_list args;

    va_start(args, p);

    if ( logOut ) {
        vfprintf(logOut, p, args);
    } else {
        vprintf(p, args);
    }
    va_end(args);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

