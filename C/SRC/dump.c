﻿/*****************************************************************************\
*                                                                             *
*   File name	    dump.c				         	      *
*									      *
*   Description     Dump a file in hexadecimal on standard output	      *
*									      *
*   Notes	    Under Unix, with gcc 3.x, compile with the -x c option,   *
*		    else it'd think this is a C++ source, and generate an     *
*		    error "undefined reference to '__gxx_personality_v0'".    *
*		    Alternative, use the -lstdc++ option to link-in C++ entry.*
*		    							      *
*		    There's a Linux tool called xxd that does almost the same.*
*		    							      *
*   History								      *
*    1995-12-19 JFL jf.larvoire@hp.com created this program.		      *
*    2004-04-05 JFL Dump the standard input if no file is specified.	      *
*                   Display a title with byte offsets, to improve readability.*
*                   Version 1.1.                                              *
*    2005-02-07 JFL Added support for a Unix version. No functional change.   *
*    2005-02-22 JFL Don't consider / as a switch character under Unix.        *
*                   Fixed bug with GetScreenRows, causing /p option to be off *
*                    by 1 line under all OS but MS-DOS. Version 1.1a.         *
*    2005-05-05 JFL Updated GetScreenRows to work under Unix. Version 1.1b.   *
*    2008-01-25 JFL Make termcap library optional under Unix, else use tput.  *
*                   Use fread instead of getch. Version 1.1c.                 *
*    2012-10-01 JFL Added support for a Win64 version. No new features.       *
*		    Version 1.1.4.					      *
*    2012-10-18 JFL Added my name in the help. Version 1.1.5.                 *
*    2014-12-04 JFL Define _GNU_SOURCE to avoid warnings in Linux.            *
*		    Version 1.1.6.					      *
*    2016-01-07 JFL Fixed all warnings in Linux, and a few real bugs.         *
*		    Version 1.1.7.  					      *
*    2017-04-13 JFL Do not print a final blank line in DOS and Windows.       *
*		    Version 1.1.8.  					      *
*    2019-04-19 JFL Use the version strings from the new stversion.h. V.1.1.9.*
*    2019-06-12 JFL Added PROGRAM_DESCRIPTION definition. Version 1.1.10.     *
*                                                                             *
*         © Copyright 2016 Hewlett Packard Enterprise Development LP          *
* Licensed under the Apache 2.0 license - www.apache.org/licenses/LICENSE-2.0 *
\******************************************************************************/

#define PROGRAM_DESCRIPTION "Dump data as both hexadecimal and text"
#define PROGRAM_NAME    "dump"
#define PROGRAM_VERSION "1.1.10"
#define PROGRAM_DATE    "2019-06-12"

#define _GNU_SOURCE		/* ISO C, POSIX, BSD, and GNU extensions */
#define _CRT_SECURE_NO_WARNINGS /* Avoid MSVC security warnings */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/************************* Unix-specific definitions *************************/

#ifdef __unix__     /* Unix */

#define SUPPORTED_OS 1

#define _getch getchar

#include <unistd.h>
#include <ctype.h>

#ifndef USE_TERMCAP
#define USE_TERMCAP 0 /* 1=Use termcap lib; 0=Don't */
#endif

#endif

/************************* OS/2-specific definitions *************************/

#ifdef _OS2    /* To be defined on the command line for the OS/2 version */

#define SUPPORTED_OS 1

#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_VIO
#include "os2.h" 

#undef FILENAME_MAX
#define FILENAME_MAX 260	/* FILENAME_MAX incorrect in stdio.h */

#include <conio.h>
#include <io.h>

#endif

/************************ Win32-specific definitions *************************/

#ifdef _WIN32	     /* Automatically defined when targeting a Win32 applic. */

#define SUPPORTED_OS 1

#define NOGDI
#define NOUSER
#define NONLS
#include <windows.h>

#include <conio.h>
#include <io.h>

#endif

/************************ MS-DOS-specific definitions ************************/

#ifdef _MSDOS	    /* Automatically defined when targeting an MS-DOS applic. */

#define SUPPORTED_OS 1

#include <conio.h>
#include <io.h>

#endif

/******************************* Any other OS ********************************/

#ifndef SUPPORTED_OS
#error "Unsupported OS"
#endif

/********************** End of OS-specific definitions ***********************/

/* SysToolsLib include files */
#include "stversion.h"	/* SysToolsLib version strings. Include last. */

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

#define TRUE 1
#define FALSE 0

#define streq(string1, string2) (strcmp(string1, string2) == 0)

/* Global variables */

int paginate = FALSE;

/* Forward references */

void usage(void);
int IsSwitch(char *pszArg);
int between(DWORD floor, DWORD u, DWORD ceiling);
void printflf(void);
int GetScreenRows(void);
int is_redirected(FILE *f);

/*---------------------------------------------------------------------------*\
*                                                                             *
|   Function:	    main						      |
|									      |
|   Description:    EXE program main initialization routine		      |
|									      |
|   Parameters:     int argc		    Number of arguments 	      |
|		    char *argv[]	    List of arguments		      |
|									      |
|   Returns:	    The return code to pass to DOS.			      |
|									      |
|   Updates:								      |
|									      |
|    1995/12/19 JFL Created this routine				      |
*									      *
\*---------------------------------------------------------------------------*/

int main(int argc, char *argv[])
    {
    int i;
    DWORD dwBase = 0xFFFFFFFFL;     /* First address to dump */
    DWORD dwLength = 0xFFFFFFFFL;   /* Number of bytes to dump */
    BYTE table[16];		    /* 16 bytes table */
    DWORD ul;
    WORD u;
    char *pszName = NULL;	    /* File name */
    FILE *f;

#ifndef __unix__
    /* Force stdin and stdout to untranslated */
    _setmode( _fileno( stdin ), _O_BINARY );
#endif

    for (i=1; i<argc; i++)
        {
        if (IsSwitch(argv[i]))
            {
	    if (   streq(argv[i]+1, "?")
	        || streq(argv[i]+1, "h")
	        || streq(argv[i]+1, "-help"))
                {
		usage();
                }
	    if (streq(argv[i]+1, "p"))
                {
		paginate = GetScreenRows() - 1;  /* Pause once per screen */
		continue;
                }
	    if (streq(argv[i]+1, "V")) {	    /* -V: Display the version */
		puts(DETAILED_VERSION);
		exit(0);
	    }
	    printf("Unrecognized switch %s. Ignored.\n", argv[i]);
            continue;
	    }
	if (!pszName)
	    {
	    pszName = argv[i];
	    continue;
	    }
	if (dwBase == 0xFFFFFFFFL)
            {
	    if (sscanf(argv[i], "%lX", &ul)) dwBase = ul;
            continue;
            }
	if (dwLength == 0xFFFFFFFFL)
            {
	    if (sscanf(argv[i], "%lX", &ul)) dwLength = ul;
            continue;
            }
        printf("Unexpected argument: %s\nIgnored.\n", argv[i]);
        break;  /* Ignore other arguments */
	}

    if (pszName)
	{
        f = fopen(pszName, "rb");
        if (!f)
            {
            printf("Cannot open file %s.\n", pszName);
            exit(1);
            }
        }
    else
        {
	if (!is_redirected(stdin)) usage();
        f = stdin;
        paginate = FALSE;  /* Avoid waiting forever */
        }

    printf("\n\
Offset    00           04           08           0C           0   4    8   C   \n\
--------  -----------  -----------  -----------  -----------  -------- --------\n\
");

    if (dwBase == 0xFFFFFFFFL) dwBase = 0;
    fseek(f, dwBase & 0xFFFFFFF0L, SEEK_SET);

    for (ul = dwBase & 0xFFFFFFF0L;
	 between(dwBase & 0xFFFFFFF0L, ul, dwBase+dwLength);
         ul += 16)
	{
	size_t nRead;

	nRead = fread(table, 1, 16, f);
	if (!nRead) break;

	printf("%08lX ", ul);

	/* Display the hex dump */

	for (u=0; u<nRead; u++)
            {
	    if (!(u&3)) printf(" ");
	    if (between(dwBase, ul+u, dwBase+dwLength))
		printf("%02X ", (WORD)table[u]);
            else
		printf("   ");
            }
	for ( ; u<16; u++)
	    {
	    if (!(u&3)) printf(" ");
	    printf("   ");
	    }

	/* Display the character dump */

	for (u=0; u<nRead; u++)
	    {
	    if (!(u&7)) printf(" ");
#ifdef __unix__
	    if ( (table[u] & 0x7F) < 0x20) table[u] = ' ';
#else
	    switch (table[u])
		{
		case '\x07': table[u] = ' '; break;
		case '\x08': table[u] = ' '; break; /* Backspace	*/
		case '\x09': table[u] = ' '; break; /* Tab		*/
		case '\x0A': table[u] = ' '; break; /* Line feed	*/
		case '\x0D': table[u] = ' '; break; /* Carrier return	*/
		case '\x1A': table[u] = ' '; break; 
		default: break;
		}
#endif
	    if (between(dwBase, ul+u, dwBase+dwLength) && (table[u]>' '))
		printf("%c", table[u]);
            else
		printf(" ");
            }
	for ( ; u<16; u++)
	    {
	    if (!(u&7)) printf(" ");
	    printf(" ");
	    }

	printflf();
	}

#ifdef __unix__
    printflf();
#endif

    return 0;
    }

/*---------------------------------------------------------------------------*\
*                                                                             *
|   Function:	    usage						      |
|									      |
|   Description:    Display a brief help screen 			      |
|									      |
|   Parameters:     None						      |
|									      |
|   Returns:	    Does not return					      |
|									      |
|   Updates:								      |
|									      |
|    1995/12/19 JFL Created this routine				      |
*									      *
\*---------------------------------------------------------------------------*/

void usage(void)
    {
    printf(
PROGRAM_NAME_AND_VERSION " - " PROGRAM_DESCRIPTION "\n\
\n\
Usage: dump [switches] [filename] [address] [length]\n\
\n\
Switches:\n\
\n\
  -?	Display this help screen\n\
  -p	Pause for each screen-full of information.\n\
\n"
#ifdef _MSDOS
"Author: Jean-Francois Larvoire"
#else
"Author: Jean-François Larvoire"
#endif
" - jf.larvoire@hpe.com or jf.larvoire@free.fr\n"
#ifdef __unix__
"\n"
#endif
);
    exit(1);
    }

int between(DWORD floor, DWORD u, DWORD ceiling)
    {
    if (ceiling >= floor)
        return (u >= floor) && (u < ceiling);       /* TRUE if inside */
    else    /* 32 bits wraparound */
        return !((u < floor) && (u >= ceiling));    /* TRUE if outside */
    }

/*---------------------------------------------------------------------------*\
*                                                                             *
|   Function:	    IsSwitch						      |
|                                                                             |
|   Description:    Test if an argument is a command-line switch.             |
|                                                                             |
|   Parameters:     char *pszArg	    Would-be argument		      |
|                                                                             |
|   Return value:   TRUE or FALSE					      |
|                                                                             |
|   Notes:								      |
|                                                                             |
|   History:								      |
*                                                                             *
\*---------------------------------------------------------------------------*/

int IsSwitch(char *pszArg)
    {
    return (   (*pszArg == '-')
#ifndef __unix__
            || (*pszArg == '/')
#endif
           ); /* It's a switch */
    }

/*---------------------------------------------------------------------------*\
*                                                                             *
|   Function:	    printflf						      |
|                                                                             |
|   Description:    Print a line feed, and possibly pause on full screens     |
|                                                                             |
|   Arguments:								      |
|                                                                             |
|	None								      |
|                                                                             |
|   Return value:   None						      |
|                                                                             |
|   Notes:								      |
|                                                                             |
|   History:								      |
|    2008/01/25 JFL Added test for Control-C and ESC.                         |
*                                                                             *
\*---------------------------------------------------------------------------*/

void printflf(void)
   {
   static int nlines = 0;
   int c;

   printf("\n");

   if (!paginate) return;

   nlines += 1;
   if (nlines < paginate) return;
   nlines = 0;

   fflush(stdin);		/* Flush any leftover characters */
   printf("Press any key to continue... ");
   c = _getch();		/* Pause until a key is pressed */
   fflush(stdin);		/* Flush any additional characters that may be left */
   printf("\r                                   \r");
   if (c==3 || c==27) exit(0);	/* Ctrl-C or ESC pressed. Exit immediately. */
   return;
   }

/*---------------------------------------------------------------------------*\
*                                                                             *
|   Function:	    is_redirected					      |
|									      |
|   Description:    Check if a FILE is a device or a disk file. 	      |
|									      |
|   Parameters:     FILE *f		    The file to test		      |
|									      |
|   Returns:	    TRUE if the FILE is a disk file			      |
|									      |
|   Notes:	    Designed for use with the stdin and stdout FILEs. If this |
|		    routine returns TRUE, then they've been redirected.       |
|									      |
|   History:								      |
|    2004/04/05 JFL Added a test of the S_IFIFO flag, for pipes under Windows.|
*									      *
\*---------------------------------------------------------------------------*/

#ifndef S_IFIFO
#define S_IFIFO         0010000         /* pipe */
#endif

int is_redirected(FILE *f)
    {
    int err;
    struct stat buf;			/* Use MSC 6.0 compatible names */
    int h;

    h = fileno(f);			/* Get the file handle */
    err = fstat(h, &buf);		/* Get information on that handle */
    if (err) return FALSE;		/* Cannot tell more if error */
    return (   (buf.st_mode & S_IFREG)	/* Tell if device is a regular file */
            || (buf.st_mode & S_IFIFO)	/* or it's a FiFo */
	   );
    }

/*---------------------------------------------------------------------------*\
*									      *
|   Function:	    GetScreenRows					      |
|									      |
|   Description:    Get the number of rows on the text screen		      |
|									      |
|   Arguments:								      |
|									      |
|	None								      |
|									      |
|   Return value:   Per the function definition 			      |
|									      |
|   Notes:								      |
|									      |
|   History:								      |
*									      *
\*---------------------------------------------------------------------------*/

/*****************************************************************************\
*									      *
*				OS/2 Version				      *
*									      *
\*****************************************************************************/

#ifdef _OS2

/* Make sure to include os2.h at the beginning of this file, and before that
    to define the INCL_VIO constant to enable the necessary section */

int GetScreenRows(void)
    {
    VIOMODEINFO vmi;

    VioGetMode(&vmi, 0);

    return vmi.row;
    }

#endif

/*****************************************************************************\
*									      *
*				WIN32 Version				      *
*									      *
\*****************************************************************************/

#ifdef _WIN32

/* Make sure to include windows.h at the beginning of this file, and especially
    the kernel section */

int GetScreenRows(void)
    {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
	return 0;	/* Disable pause mode if console size unknown */

    return csbi.srWindow.Bottom + 1 - csbi.srWindow.Top;
    }

#endif

/*****************************************************************************\
*									      *
*				MS_DOS Version				      *
*									      *
\*****************************************************************************/

#ifdef _MSDOS

int GetScreenRows(void)
    {
    BYTE far *fpc;

    fpc = (BYTE far *)0x00400084L; 	/* Index of the last line, 0-based. */
    return *fpc + 1; 			/* Add 1 to get the number of lines. */
    }

#endif

/*****************************************************************************\
*									      *
*				 Unix Version				      *
*									      *
\*****************************************************************************/

/* Requires linking with the  -ltermcap option */

#ifdef __unix__

#if defined(USE_TERMCAP) && USE_TERMCAP

#include <termcap.h>

static char term_buffer[2048];
static int tbInitDone = FALSE;

int init_terminal_data()
    {
    char *termtype;
    int success;

    if (tbInitDone) return 0;

    termtype = getenv ("TERM");
    if (termtype == 0) {
      printf("Specify a terminal type with `setenv TERM <yourtype>'.\n");
      exit(1);
    }

    success = tgetent (term_buffer, termtype);
    if (success < 0) {
      printf("Could not access the termcap data base.\n");
      exit(1);
    }
    if (success == 0) {
      printf("Terminal type `%s' is not defined.\n", termtype);
      exit(1);
    }

    tbInitDone = TRUE;
    return 0;
    }

int GetScreenRows(void)
    {
    init_terminal_data();
    return tgetnum("li");
    }

int GetScreenColumns(void)
    {
    init_terminal_data();
    return tgetnum ("co");
    }

#else

extern int errno;

/* Execute a command, and capture its output */
#define TEMP_BLOCK_SIZE 1024
char *Exec(char *pszCmd) {
  size_t nBufSize = 0;
  char *pszBuf = malloc(0);
  size_t nRead = 0;
  int iPid = getpid();
  char szTempFile[32];
  char *pszCmd2 = malloc(strlen(pszCmd) + 32);
  int iErr;
  FILE *hFile;
  sprintf(szTempFile, "/tmp/RowCols.%d", iPid);
  sprintf(pszCmd2, "%s >%s", pszCmd, szTempFile);
  iErr = system(pszCmd2);
  if (iErr) {
    free(pszBuf);
    return NULL;
  }
  /* Read the temp file contents */
  hFile = fopen(szTempFile, "r");
  while (1) {
    char *pszBuf2 = realloc(pszBuf, nBufSize + TEMP_BLOCK_SIZE);
    if (!pszBuf2) break;
    pszBuf = pszBuf2;
    nRead = fread(pszBuf+nBufSize, 1, TEMP_BLOCK_SIZE, hFile);
    nBufSize += TEMP_BLOCK_SIZE;
    if (nRead < TEMP_BLOCK_SIZE) break;
    if (feof(hFile)) break;
  }
  fclose(hFile);
  /* Cleanup */
  remove(szTempFile);
  free(pszCmd2);
  return pszBuf; /* Must be freed by the caller */
}

int GetScreenRows(void) {
  int nRows = 25; /* Default for VGA screens */
  /* char *pszRows = getenv("LINES"); */
  /* if (pszRows) nRows = atoi(pszRows); */
  char *pszBuf = Exec("tput lines");
  if (pszBuf) {
    nRows = atoi(pszBuf);
    free(pszBuf);
  }
  return nRows;
}

int GetScreenColumns(void) {
  int nCols = 80; /* Default for VGA screens */
  /* char *pszCols = getenv("COLUMNS"); */
  /* if (pszCols) nCols = atoi(pszCols); */
  char *pszBuf = Exec("tput cols");
  if (pszBuf) {
    nCols = atoi(pszBuf);
    free(pszBuf);
  }
  return nCols;
}

#endif

#endif

/*****************************************************************************\
*									      *
*			  End of OS-specific routines			      *
*									      *
\*****************************************************************************/
