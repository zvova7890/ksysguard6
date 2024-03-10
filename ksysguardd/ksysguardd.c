/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999 - 2003 Chris Schlaeger <cs@kde.org>
                              Tobias Koenig <tokoe@kde.org>
                         2006 Greg Martyn <greg.martyn@gmail.com>

    Solaris support by Torsten Kasch <tk@Genetik.Uni-Bielefeld.DE>

    This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

/* gettimeofday, strdup, fileno, fdopen */
#define _XOPEN_SOURCE 700

/* amazingly, this is not in POSIX, XOPEN, etc. so systems like FreeBSD
 * that treat _XOPEN_SOURCE as a restriction on definitions will refuse
 * to define this with _XOPEN_SOURCE defined
 */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((in_addr_t) 0x7F000001)
#endif

#include <config-workspace.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "modules.h"

#include "ksysguardd.h"

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif

#define CMDBUFSIZE	128
#define MAX_CLIENTS	100

typedef struct {
  int socket;
  FILE* out;
} ClientInfo;

static int ServerSocket;
static ClientInfo ClientList[ MAX_CLIENTS ];
static int SocketPort = -1;
static unsigned char BindToAllInterfaces = 0;
static int CurrentSocket;
static const char LockFile[] = "/var/run/ksysguardd.pid";
static const char *ConfigFile = KSYSGUARDDRCFILE;

void signalHandler( int sig );
void makeDaemon( void );
void resetClientList( void );
int addClient( int client );
int delClient( int client );
int createServerSocket( void );

/**
  This variable is set to 1 if a module requests that the daemon should
  be terminated.
 */
int QuitApp = 0;

/**
  This variable indicates whether we are running as daemon or (1) or if
  we were have a controlling shell.
 */
int RunAsDaemon = 0;

/**
  This pointer is used by all modules. It contains the file pointer of
  the currently served client. This is stdout for non-daemon mode.
 */
FILE* CurrentClient = 0;

static int processArguments( int argc, char* argv[] )
{
  int option;

  opterr = 0;
  while ( ( option = getopt( argc, argv, "-p:f:dih" ) ) != EOF ) {
    switch ( tolower( option ) ) {
      case 'p':
        SocketPort = atoi( optarg );
        break;
      case 'f':
        ConfigFile = strdup( optarg );
        break;
      case 'd':
        RunAsDaemon = 1;
        break;
      case 'i':
        BindToAllInterfaces = 1;
        break;
      case '?':
      case 'h':
      default:
        fprintf(stderr, "Usage: %s [-d] [-i] [-p port]\n", argv[ 0 ] );
        return -1;
        break;
    }
  }

  return 0;
}

static void printWelcome( FILE* out )
{
  fprintf( out, "ksysguardd 4\n"
           "(c) 1999, 2000, 2001, 2002 Chris Schlaeger <cs@kde.org>\n"
           "(c) 2001 Tobias Koenig <tokoe@kde.org>\n"
           "(c) 2006-2008 Greg Martyn <greg.martyn@gmail.com>\n"
           "This program is part of the KDE Project and licensed under\n"
           "the GNU GPL version 2. See https://www.kde.org for details.\n");

  fflush( out );
}

static int createLockFile()
{
  FILE *file;

  if ( ( file = fopen( LockFile, "w+" ) ) != NULL ) {
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = -1;
    if ( fcntl( fileno( file ), F_SETLK, &lock ) < 0 ) {
      if ( ( errno == EACCES ) || ( errno == EAGAIN ) ) {
        log_error( "ksysguardd is running already" );
        fprintf( stderr, "ksysguardd is running already\n" );
        fclose( file );
        return -1;
      }
    }

    fseek( file, 0, SEEK_SET );
    fprintf( file, "%d\n", getpid() );
    fflush( file );
    if (ftruncate( fileno( file ), ftell( file ) ) == -1) {
      log_error( "Cannot set size of lockfile '%s'", LockFile );
      fprintf( stderr, "Cannot set size of lockfile '%s'\n", LockFile );
      fclose( file );
      return -2;
    }
  } else {
    log_error( "Cannot create lockfile '%s'", LockFile );
    fprintf( stderr, "Cannot create lockfile '%s'\n", LockFile );
    return -2;
  }

  /**
    We abandon 'file' here on purpose. It's needed nowhere else, but we
    have to keep the file open and locked. The kernel will remove the
    lock when the process terminates and the runlevel scripts has to
    remove the pid file.
   */
  return 0;
}

static void dropPrivileges( void )
{
  struct passwd *pwd;

  if ( ( pwd = getpwnam( "nobody" ) ) != NULL ) {
    if ( !setgid(pwd->pw_gid) ) {
      if (setuid(pwd->pw_uid)) {
        perror("setuid");
      }
    }
    if (!geteuid() && getuid() != pwd->pw_uid)
      _exit(1);
  }
  else {
    log_error( "User 'nobody' does not exist." );
    /**
      We exit here to avoid becoming vulnerable just because
      user nobody does not exist.
     */
    _exit(1);
  }
}

void makeDaemon( void )
{
  int fd = -1;
  switch ( fork() ) {
    case -1:
      log_error( "fork() failed" );
      break;
    case 0:
      setsid();
      if( chdir( "/" ) == -1) {
          log_error("chdir(\"/\") failed");
          _exit(1);
      }
      umask( 0 );
      if ( createLockFile() < 0 )
        _exit( 1 );

      dropPrivileges();

      fd = open("/dev/null", O_RDWR, 0);
      if (fd != -1) {
          dup2(fd, STDIN_FILENO);
          dup2(fd, STDOUT_FILENO);
          dup2(fd, STDERR_FILENO);
          close (fd);
      }
      break;
    default:
      exit( 0 );
  }
}

static int readCommand( int fd, char* cmdBuf, size_t len )
{
  unsigned int i;
  char c;
  for ( i = 0; i < len; ++i )
  {
    int result = read( fd, &c, 1 );
    if (result < 0)
      return -1; /* Error */

    if (result == 0) {
      if (i == 0)
        return -1; /* Connection lost */

      break; /* End of data */
    }

    if (c == '\n')
      break; /* End of line */

    cmdBuf[ i ] = c;
  }
  cmdBuf[i] = '\0';

  return i;
}

void resetClientList( void )
{
  for (int i = 0; i < MAX_CLIENTS; i++ ) {
    ClientList[ i ].socket = -1;
    ClientList[ i ].out = 0;
  }
}

/**
  addClient adds a new client to the ClientList.
 */
int addClient( int client )
{
  FILE* out;

  for (int i = 0; i < MAX_CLIENTS; i++ ) {
    if ( ClientList[ i ].socket == -1 ) {
      ClientList[ i ].socket = client;
      if ( ( out = fdopen( client, "w+" ) ) == NULL ) {
        log_error( "fdopen()" );
        return -1;
      }
      /* We use unbuffered IO */
      fcntl( fileno( out ), F_SETFL, O_NONBLOCK );
      ClientList[ i ].out = out;
      printWelcome( out );
      fprintf( out, "ksysguardd> " );
      fflush( out );

      return 0;
    }
  }

  return -1;
}

/**
  delClient removes a client from the ClientList.
 */
int delClient( int client )
{
  for (int i = 0; i < MAX_CLIENTS; i++ ) {
    if ( ClientList[i].socket == client ) {
      fclose( ClientList[ i ].out );
      ClientList[ i ].out = 0;
      close( ClientList[ i ].socket );
      ClientList[ i ].socket = -1;
      return 0;
    }
  }

  return -1;
}

int createServerSocket()
{
  int i = 1;
  int newSocket;
  struct sockaddr_in s_in;
  struct servent *service;

  if ( ( newSocket = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
    log_error( "socket()" );
    return -1;
  }

  setsockopt( newSocket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof( i ) );

  /**
    The -p command line option always overrides the default or the
    service entry.
   */
  if ( SocketPort == -1 ) {
    if ( ( service = getservbyname( "ksysguardd", "tcp" ) ) == NULL ) {
      /**
        No entry in service directory and no command line request,
        so we take the build-in default (the official IANA port).
       */
      SocketPort = PORT_NUMBER;
    } else
      SocketPort = htons( service->s_port );
  }

  memset( &s_in, 0, sizeof( struct sockaddr_in ) );
  s_in.sin_family = AF_INET;
  s_in.sin_addr.s_addr = htonl( BindToAllInterfaces ? INADDR_ANY : INADDR_LOOPBACK );
  s_in.sin_port = htons( SocketPort );

  if ( bind( newSocket, (struct sockaddr*)&s_in, sizeof( s_in ) ) < 0 ) {
    log_error( "Cannot bind to port %d", SocketPort );
    return -1;
  }

  if ( listen( newSocket, 5 ) < 0 ) {
    log_error( "listen()" );
    return -1;
  }

  return newSocket;
}

static int setupSelect( fd_set* fds )
{
  int highestFD = ServerSocket;
  FD_ZERO( fds );
  /**
    Fill the filedescriptor array with all relevant descriptors. If we
    not in daemon mode we only need to watch stdin.
   */
  if ( RunAsDaemon ) {
    int i;
    FD_SET( ServerSocket, fds );

    for ( i = 0; i < MAX_CLIENTS; i++ ) {
      if ( ClientList[ i ].socket != -1 ) {
        FD_SET( ClientList[ i ].socket, fds );
        if ( highestFD < ClientList[ i ].socket )
          highestFD = ClientList[ i ].socket;
      }
    }
  } else {
    FD_SET( STDIN_FILENO, fds );
    if ( highestFD < STDIN_FILENO )
      highestFD = STDIN_FILENO;
  }

  return highestFD;
}

static void checkModules()
{
  struct SensorModul *entry;

  for ( entry = SensorModulList; entry->configName != NULL; entry++ )
    if ( entry->checkCommand != NULL && entry->available )
      entry->checkCommand();
}

static void handleSocketTraffic( int socketNo, const fd_set* fds )
{
  char cmdBuf[ CMDBUFSIZE ];

  if ( RunAsDaemon ) {
    if ( FD_ISSET( socketNo, fds ) ) {
      int clientsocket;
      struct sockaddr addr;
      kde_socklen_t addr_len = sizeof( struct sockaddr );

      /* a new connection */
      if ( ( clientsocket = accept( socketNo, &addr, &addr_len ) ) < 0 ) {
        log_error( "accept()" );
        exit( 1 );
      } else
        addClient( clientsocket );
    }

    for (int i = 0; i < MAX_CLIENTS; i++ ) {
      if ( ClientList[ i ].socket != -1 ) {
        CurrentSocket = ClientList[ i ].socket;
        if ( FD_ISSET( ClientList[ i ].socket, fds ) ) {
          ssize_t cnt;
          if ( ( cnt = readCommand( CurrentSocket, cmdBuf, sizeof( cmdBuf ) - 1 ) ) <= 0 )
            delClient( CurrentSocket );
          else {
            cmdBuf[ cnt ] = '\0';
            if ( strncmp( cmdBuf, "quit", 4 ) == 0 )
              delClient( CurrentSocket );
            else {
              CurrentClient = ClientList[ i ].out;
              fflush( stdout );
              executeCommand( cmdBuf );
              output( "ksysguardd> " );
              fflush( CurrentClient );
            }
          }
        }
      }
    }
  } else if ( FD_ISSET( STDIN_FILENO, fds ) ) {
    if (readCommand( STDIN_FILENO, cmdBuf, sizeof( cmdBuf ) ) < 0) {
      exit(0);
    }
    executeCommand( cmdBuf );
    printf( "ksysguardd> " );
    fflush( stdout );
  }
}

static void initModules()
{
  struct SensorModul *entry;

  /* initialize all sensors */
  initCommand();

  for ( entry = SensorModulList; entry->configName != NULL; entry++ ) {
    if ( entry->initCommand != NULL && sensorAvailable( entry->configName ) ) {
      entry->available = 1;
      entry->initCommand( entry );
    }
  }

  ReconfigureFlag = 0;
}

static void exitModules()
{
  struct SensorModul *entry;

  for ( entry = SensorModulList; entry->configName != NULL; entry++ ) {
    if ( entry->exitCommand != NULL && entry->available )
      entry->exitCommand();
  }

  exitCommand();
}



/*
================================ public part =================================
*/

/*
 *  Will replace a "/" with "\/"
 *  Allocates a new string, so when calling this make sure to free the original.
 */
char* escapeString( char* string ) {
  int i, length;
  char* result;
  char* resultP;
  int charsToEscape = 0;

  /* Count how many characters we need to escape so that we know how much memory we'll have to allocate */
  i = 0;
  while (string[i] != '\0') {
    if( string[i] == '/' ) {
      ++charsToEscape;
    }

    ++i;
  }

  /* Note: length doesn't count the \0 at the end of the string */
  length = i;

  /* Allocate a new string, result, with enough room for the escaped characters */
  if(! (result = (char *)malloc( length + charsToEscape + 1 )) ) {
    print_error("Malloc failed - out of memory");
    exit(1);
  }
  resultP = result;
  /* Fill result with an escaped version of string */
  i = 0;
  while (string[i] != '\0') {
    if( string[i] == '/' ) {
      resultP[0] = '\\';
      resultP++;
    }
    resultP[0] = string[i];
    resultP++;

    ++i;
  }
  resultP[0] = '\0';
  return result;
}

#ifdef HAVE_SYS_INOTIFY_H
static void setupInotify(int *mtabfd) {
  (*mtabfd) = inotify_init ();
  if ((*mtabfd) >= 0) {
    int wd = inotify_add_watch ((*mtabfd), "/etc/mtab", IN_MODIFY | IN_CREATE | IN_DELETE);
    if(wd < 0) (*mtabfd) = -1; /* error setting up inotify watch */
  }

}
#endif
int main( int argc, char* argv[] )
{
  fd_set fds;

  printWelcome( stdout );

  if ( processArguments( argc, argv ) < 0 )
    return -1;

  parseConfigFile( ConfigFile );

  initModules();

  if ( RunAsDaemon ) {
    makeDaemon();

    if ( ( ServerSocket = createServerSocket() ) < 0 )
      return -1;
    resetClientList();
  } else {
    fprintf( stdout, "ksysguardd> " );
    fflush( stdout );
    CurrentClient = stdout;
    ServerSocket = 0;
  }

#ifdef HAVE_SYS_INOTIFY_H
  /* Monitor mtab for changes */
  int mtabfd = 0;
  setupInotify(&mtabfd);
#endif

  struct timeval now;
  struct timeval last;
  gettimeofday( &last, NULL );

  while ( !QuitApp ) {
    int highestFD = setupSelect( &fds );
#ifdef HAVE_SYS_INOTIFY_H
    if(mtabfd >= 0)
      FD_SET( mtabfd, &fds);
    if(mtabfd > highestFD) highestFD = mtabfd;
#endif

    /* wait for communication or timeouts */
    int ret = select( highestFD + 1, &fds, NULL, NULL, NULL );
    if(ret >= 0) {
        gettimeofday( &now, NULL );
        if ( now.tv_sec - last.tv_sec >= 5 ) { /* 5 second intervals */
            /* If so, update all sensors and save current time to last. */
            checkModules();
            last = now;
        }
#ifdef HAVE_SYS_INOTIFY_H
        if(mtabfd >= 0 && FD_ISSET(mtabfd, &fds)) {
            close(mtabfd);
            setupInotify(&mtabfd);
        }
#endif
        handleSocketTraffic( ServerSocket, &fds );
    }
  }

  exitModules();

  freeConfigFile();

  return 0;
}
