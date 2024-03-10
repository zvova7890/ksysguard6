/*
    KSysGuard, the KDE System Guard

    Copyright (c) 1999 - 2001 Chris Schlaeger <cs@kde.org>

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

#define _BSD_SOURCE /* kill, syscall */
#define _DEFAULT_SOURCE /* Eliminate warning from prev */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <asm/unistd.h>



#include "../../gui/SignalIDs.h"
#include "Command.h"
#include "PWUIDCache.h"
#include "ccont.h"
#include "ksysguardd.h"

#include "ProcessList.h"

#define BUFSIZE 1024
#define TAGSIZE 32
#define KDEINITLEN sizeof( "kdeinit: " )

/* For ionice */
extern int sys_ioprio_set(int, int, int);
extern int sys_ioprio_get(int, int);

#define HAVE_IONICE

/* Check if this system has ionice */
#if !defined(SYS_ioprio_get) || !defined(SYS_ioprio_set)
/* All new kernels have SYS_ioprio_get and _set defined, but for the few that do not, here are the definitions */
#if defined(__i386__)
#define __NR_ioprio_set         289
#define __NR_ioprio_get         290
#elif defined(__ppc__) || defined(__powerpc__)
#define __NR_ioprio_set         273
#define __NR_ioprio_get         274
#elif defined(__x86_64__)
#define __NR_ioprio_set         251
#define __NR_ioprio_get         252
#elif defined(__ia64__)
#define __NR_ioprio_set         1274
#define __NR_ioprio_get         1275
#else
#ifdef __GNUC__
#warning "This architecture does not support IONICE.  Disabling ionice feature."
#endif
#undef HAVE_IONICE
#endif
/* Map these to SYS_ioprio_get */
#define SYS_ioprio_get                __NR_ioprio_get
#define SYS_ioprio_set                __NR_ioprio_set

#endif /* !SYS_ioprio_get */

/* Set up ionice functions */
#ifdef HAVE_IONICE
#define IOPRIO_WHO_PROCESS 1
#define IOPRIO_CLASS_SHIFT 13

/* Expose the kernel calls to usespace via syscall
 * See man ioprio_set  and man ioprio_get   for information on these functions */
static int ioprio_set(int which, int who, int ioprio)
{
  return syscall(SYS_ioprio_set, which, who, ioprio);
}
 
static int ioprio_get(int which, int who)
{
  return syscall(SYS_ioprio_get, which, who);
}
#endif


#ifndef bool
#define bool char
#define true 1
#define false 0
#endif

typedef struct {

  /** The parent process ID */
  pid_t ppid;

  /** The real user ID */
  uid_t uid;

  /** The real group ID */
  gid_t gid;

  /** The process ID of any application that is debugging this one. 0 if none */
  pid_t tracerpid;

  /** A character description of the process status */
  char status[ 16 ];

  /** The tty the process owns */
  char tty[10];

  /**
    The nice level. The range should be -20 to 20. I'm not sure
    whether this is true for all platforms.
   */
  int niceLevel;

  /** The scheduling priority. */
  int priority;

  /** The i/o scheduling class and priority. */
  int ioPriorityClass;  /**< 0 for none, 1 for realtime, 2 for best-effort, 3 for idle.  -1 for error. */
  int ioPriority;       /**< Between 0 and 7.  0 is highest priority, 7 is lowest.  -1 for error. */

  /**
    The total amount of virtual memory space that this process uses. This includes shared and
    swapped memory, plus graphics memory and mmap'ed files and so on.

    This is in KiB
   */
  unsigned long vmSize;

  /**
    The amount of physical memory the process currently uses, including the physical memory used by any
    shared libraries that it uses.  Hence 2 processes sharing a library will both report their vmRss as including
    this shared memory, even though it's only allocated once. 
   
    This is in KiB
   */
  
  unsigned long vmRss;

  /** The amount of physical memory that is used by this process, not including any memory used by any shared libraries.
   *  This is in KiB */
  unsigned long vmURss;

  /**
    The number of 1/100 of a second the process has spend in user space.
    If a machine has an uptime of 1 1/2 years or longer this is not a
    good idea. I never thought that the stability of UNIX could get me
    into trouble! ;)
   */
  unsigned long userTime;

  /**
    The number of 1/100 of a second the process has spent in system space.
    If a machine has an uptime of 1 1/2 years or longer this is not a
    good idea. I never thought that the stability of UNIX could get me
    into trouble! ;)
   */
  unsigned long sysTime;

  /* NOTE:  To get the user/system percentage, record the userTime and sysTime from between calls, then use the difference divided by the difference in time measure in 100th's of a second */

  /** The name of the process */
  char name[ 64 ];

  /** The command used to start the process */
  char cmdline[ 256 ];

  /** The login name of the user that owns this process */
  char userName[ 32 ];

  /** NoNewPrivileges: task can't gain higher privileges via setuid etc. */
  int noNewPrivileges;

  /** cgroup: Linux process control group */
  char cGroup[ 256 ];

  /** Mandatory Access Control (SELinux or AppArmor) context */
  char macContext[ 256 ];

} ProcessInfo;

void getIOnice( int pid, ProcessInfo *ps );
void ioniceProcess( const char* cmd );

static unsigned ProcessCount;
static DIR* procDir;
static void validateStr( char* str )
{
  char* s = str;

  /* All characters that could screw up the communication will be removed. */
  while ( *s ) {
    if ( *s == '\t' || *s == '\n' || *s == '\r' )
      *s = ' ';
    ++s;
  }

  /* Make sure that string contains at least one character (blank). */
  if ( str[ 0 ] == '\0' )
    strcpy( str, " " );
}

static bool getProcess( int pid, ProcessInfo *ps )
{
  FILE* fd;
  char buf[ BUFSIZE ];
  char tag[ TAGSIZE ];
  char format[ 32 ];
  char tagformat[ 32 ];
  const char* uName;
  char status;

  snprintf( buf, BUFSIZE - 1, "/proc/%d/status", pid );
  if ( ( fd = fopen( buf, "r" ) ) == 0 ) {
    /* process has terminated in the mean time */
    return false;
  }
  ps->uid = 0;
  ps->gid = 0;
  ps->tracerpid = -1;
  
  sprintf( format, "%%%d[^\n]\n", (int)sizeof( buf ) - 1 );
  sprintf( tagformat, "%%%ds", (int)sizeof( tag ) - 1 );
  for ( ;; ) {
    if ( fscanf( fd, format, buf ) != 1 )
      break;
    buf[ sizeof( buf ) - 1 ] = '\0';
    sscanf( buf, tagformat, tag );
    tag[ sizeof( tag ) - 1 ] = '\0';
    if ( strcmp( tag, "Name:" ) == 0 ) {
      sscanf( buf, "%*s %63s", ps->name );
      validateStr( ps->name );
    } else if ( strcmp( tag, "Uid:" ) == 0 ) {
      sscanf( buf, "%*s %d %*d %*d %*d", (int*)&ps->uid );
    } else if ( strcmp( tag, "Gid:" ) == 0 ) {
      sscanf( buf, "%*s %d %*d %*d %*d", (int*)&ps->gid );
    } else if ( strcmp( tag, "TracerPid:" ) == 0 ) {
      sscanf( buf, "%*s %d", (int*)&ps->tracerpid );
      if (ps->tracerpid == 0)
          ps->tracerpid = -1; /* ksysguard uses -1 to indicate no tracerpid, but linux uses 0 */
    } else if ( strcmp( tag, "NoNewPrivs:" ) == 0 ) {
      sscanf( buf, "%*s %d", &ps->noNewPrivileges );
    }
  }

  if ( fclose( fd ) )
    return false;

  snprintf( buf, BUFSIZE - 1, "/proc/%d/stat", pid );
  buf[ BUFSIZE - 1 ] = '\0';
  if ( ( fd = fopen( buf, "r" ) ) == 0 )
    return false;
  int ttyNo;
  if ( fscanf( fd, "%*d %*s %c %d %*d %*d %d %*d %*u %*u %*u %*u %*u %lu %lu"
                   "%*d %*d %*d %d %*u %*u %*d %lu %lu",
                   &status, (int*)&ps->ppid, &ttyNo,
                   &ps->userTime, &ps->sysTime, &ps->niceLevel, &ps->vmSize,
                   &ps->vmRss) != 8 ) {
    fclose( fd );
    return false;
  }
  if (ps->ppid == 0) /* ksysguard uses -1 to indicate no parent, but linux uses 0 */
      ps->ppid = -1;
  int major = ttyNo >> 8;
  int minor = ttyNo & 0xff;
  switch(major) {
    case 136:
      snprintf(ps->tty, sizeof(ps->tty)-1, "pts/%d", minor);
      break;
    case 4:
      if(minor < 64)
        snprintf(ps->tty, sizeof(ps->tty)-1, "tty/%d", minor);
      else
        snprintf(ps->tty, sizeof(ps->tty)-1, "ttyS/%d", minor-64);
      break;
    default:
      ps->tty[0] = 0;
  }

  /*There was a "(ps->vmRss+3) * sysconf(_SC_PAGESIZE)" here originally.  I have no idea why!  After comparing it to
  meminfo and other tools, this means we report the RSS by 12 bytes different compared to them.  So I'm removing the +3
  to be consistent.  NEXT TIME COMMENT STRANGE THINGS LIKE THAT! :-)
  
    Update: I think I now know why.  The kernel reserves 3kb for process information.
  */
  ps->vmRss = ps->vmRss * sysconf(_SC_PAGESIZE) / 1024; /*convert to KiB*/
  ps->vmSize /= 1024; /* convert to KiB */

  if ( fclose( fd ) )
    return false;

  snprintf( buf, BUFSIZE - 1, "/proc/%d/statm", pid );
  buf[ BUFSIZE - 1 ] = '\0';
  ps->vmURss = -1;
  if ( ( fd = fopen( buf, "r" ) ) != 0 )  {
    unsigned long shared;
    if ( fscanf( fd, "%*d %*u %lu",
                   &shared)==1) {
      /* we use the rss - shared  to find the amount of memory just this app uses */
      ps->vmURss = ps->vmRss - (shared * sysconf(_SC_PAGESIZE) / 1024);
    }
    fclose( fd );
  }


  /* status decoding as taken from fs/proc/array.c */
  if ( status == 'R' )
    strcpy( ps->status, "running" );
  else if ( status == 'S' )
    strcpy( ps->status, "sleeping" );
  else if ( status == 'D' )
    strcpy( ps->status, "disk sleep" );
  else if ( status == 'Z' )
    strcpy( ps->status, "zombie" );
  else if ( status == 'T' )
    strcpy( ps->status, "stopped" );
  else if ( status == 'W' )
    strcpy( ps->status, "paging" );
  else
    sprintf( ps->status, "Unknown: %c", status );


  snprintf( buf, BUFSIZE - 1, "/proc/%d/cmdline", pid );
  if ( ( fd = fopen( buf, "r" ) ) == 0 )
    return false;

  ps->cmdline[ 0 ] = '\0';

  unsigned int processNameStartPosition = 0;
  unsigned int firstZeroPosition = -1U;
 
  unsigned int i =0;
  int c = EOF;
  while( (c = fgetc(fd)) != EOF && i < sizeof(ps->cmdline)-3) {
    ps->cmdline[i] = (char)c;
    if(ps->cmdline[i] == '\0')
    {
      ps->cmdline[i] = ' ';
      if(firstZeroPosition == -1U)
        firstZeroPosition = i;
    }
    if(ps->cmdline[i] == '/' && firstZeroPosition == -1U)
      processNameStartPosition = i + 1;
    i++;
  }

  if(firstZeroPosition != -1U)
  {
    unsigned int processNameLength = firstZeroPosition - processNameStartPosition;
    memcpy(ps->name, ps->cmdline + processNameStartPosition, processNameLength);
    ps->name[processNameLength] = '\0';
  }

  if(i > 2) {
    if(ps->cmdline[i-2] == ' ') ps->cmdline[i-2] = '\0';
    else ps->cmdline[i-1] = '\0';
  } else {
    ps->cmdline[0] = '\0';
  }

  validateStr( ps->cmdline );
  if ( fclose( fd ) )
    return false;

  /* Ugly hack to "fix" program name for kdeinit launched programs. */
  if ( strcmp( ps->name, "kdeinit" ) == 0 &&
       strncmp( ps->cmdline, "kdeinit: ", KDEINITLEN ) == 0 &&
       strcmp( ps->cmdline + KDEINITLEN, "Running..." ) != 0 ) {
    size_t len;
    char* end = strchr( ps->cmdline + KDEINITLEN, ' ' );
    if ( end )
      len = ( end - ps->cmdline ) - KDEINITLEN;
    else
      len = strlen( ps->cmdline + KDEINITLEN );
    if ( len > 0 ) {
      if ( len > sizeof( ps->name ) - 1 )
        len = sizeof( ps->name ) - 1;
      strncpy( ps->name, ps->cmdline + KDEINITLEN, len );
      ps->name[ len ] = '\0';
    }
  }
  /* find out user name with the process uid */
  uName = getCachedPWUID( ps->uid );
  strncpy( ps->userName, uName, sizeof( ps->userName ) - 1 );
  ps->userName[ sizeof( ps->userName ) - 1 ] = '\0';
  validateStr( ps->userName );

  getIOnice(pid, ps);

  /* CGroups */
  ps->cGroup[ 0 ] = '\0';
  snprintf( buf, BUFSIZE - 1, "/proc/%d/cgroup", pid );
  buf[ BUFSIZE - 1 ] = '\0';
  if ( ( fd = fopen( buf, "r" ) ) != 0 )  {
    while (fgets(buf, BUFSIZE, fd) && buf[0] != '0');
    fclose ( fd );

    if ( buf[0] == '0' && buf[1] == ':' && buf[2] == ':' ) {
      strncpy( ps->cGroup, &buf[3], sizeof( ps->cGroup ) - 1 );
      ps->cGroup[ sizeof( ps->cGroup ) - 1 ] = '\0';
      validateStr( ps->cGroup );
    }
  }

  /* Mandatory Access Control (SELinux or AppArmor) context */
  ps->macContext[ 0 ] = '\0';
  snprintf( buf, BUFSIZE - 1, "/proc/%d/attr/current", pid );
  if ( ( fd = fopen( buf, "r" ) ) != 0 )  {
    if (fgets( ps->macContext, sizeof( ps->macContext ), fd ))
      validateStr( ps->macContext );
    fclose ( fd );
  }

  return true;
}

void printProcessList( const char* cmd)
{
  (void)cmd;
  struct dirent* entry;

  ProcessInfo ps;
  ProcessCount = 0;
  rewinddir(procDir);
  while ( ( entry = readdir( procDir ) ) ) {
    if ( isdigit( entry->d_name[ 0 ] ) ) {
      long pid;
      pid = atol( entry->d_name );
      if(getProcess( pid, &ps )) /* Print out the details of the process.  Because of a stupid bug in kde3 ksysguard, make sure cmdline and tty are not empty */
        output( "%s\t%ld\t%ld\t%lu\t%lu\t%s\t%lu\t%lu\t%d\t%lu\t%lu\t%lu\t%s\t%ld\t%s\t%s\t%d\t%d\t%d\t%s\t%s\n",
             ps.name, pid, (long)ps.ppid,
             (long)ps.uid, (long)ps.gid, ps.status, ps.userTime,
             ps.sysTime, ps.niceLevel, ps.vmSize, ps.vmRss, ps.vmURss,
             (ps.userName[0]==0)?" ":ps.userName, (long)ps.tracerpid,
             (ps.tty[0]==0)?" ":ps.tty, (ps.cmdline[0]==0)?" ":ps.cmdline,
             ps.ioPriorityClass, ps.ioPriority, ps.noNewPrivileges, ps.cGroup, ps.macContext
        );
    }
  }
  output( "\n" );
  return;
}

void getIOnice( int pid, ProcessInfo *ps ) {
#ifdef HAVE_IONICE
  int ioprio = ioprio_get(IOPRIO_WHO_PROCESS, pid);  /* Returns from 0 to 7 for the iopriority, and -1 if there's an error */
  if(ioprio == -1) {
      ps->ioPriority = -1;
      ps->ioPriorityClass = -1;
      return; /* Error. Just give up. */
  }
  ps->ioPriority = ioprio & 0xff;  /* Bottom few bits are the priority */
  ps->ioPriorityClass = ioprio >> IOPRIO_CLASS_SHIFT; /* Top few bits are the class */
#else
  return;  /* Do nothing, if we do not support this architecture */
#endif
}


/*
================================ public part =================================
*/

void initProcessList( struct SensorModul* sm )
{
  initPWUIDCache();

  registerMonitor( "pscount", "integer", printProcessCount, printProcessCountInfo, sm );
  registerMonitor( "ps", "table", printProcessList, printProcessListInfo, sm );

  if ( !RunAsDaemon ) {
    registerCommand( "kill", killProcess );
    registerCommand( "setpriority", setPriority );
#ifdef HAVE_IONICE
    registerCommand( "ionice", ioniceProcess );
#endif
  }

  /*open /proc now in advance*/
  /* read in current process list via the /proc file system entry */
  if ( ( procDir = opendir( "/proc" ) ) == NULL ) {
    print_error( "Cannot open directory \'/proc\'!\n"
                 "The kernel needs to be compiled with support\n"
                 "for /proc file system enabled!\n" );
    return;
  }
}

void exitProcessList( void )
{
  removeMonitor( "ps" );
  removeMonitor( "pscount" );

  if ( !RunAsDaemon ) {
    removeCommand( "kill" );
    removeCommand( "setpriority" );
  }
  closedir( procDir );

  exitPWUIDCache();
}

void printProcessListInfo( const char* cmd )
{
  (void)cmd;
  output( "Name\tPID\tPPID\tUID\tGID\tStatus\tUser Time\tSystem Time\tNice\tVmSize"
                          "\tVmRss\tVmURss\tLogin\tTracerPID\tTTY\tCommand\tIO Priority Class\tIO Priority\tNNP\tCGroup\tMAC Context\n" );
  output( "s\td\td\td\td\tS\td\td\td\tD\tD\tD\ts\td\ts\ts\td\td\td\ts\ts\n" );
}

void printProcessCount( const char* cmd )
{
  (void)cmd;
  struct dirent* entry;
  ProcessCount = 0;
  rewinddir(procDir);
  while ( ( entry = readdir( procDir ) ) )
    if ( isdigit( entry->d_name[ 0 ] ) )
      ProcessCount++;


  output( "%d\n", ProcessCount );
}

void printProcessCountInfo( const char* cmd )
{
  (void)cmd;
  output( "Number of Processes\t0\t0\t\n" );
}

void killProcess( const char* cmd )
{
  /* Sends a signal (not necessarily kill!) to the process.  cmd is a string containing "kill <pid> <signal>" */
  int sig, pid;

  sscanf( cmd, "%*s %d %d", &pid, &sig );
  switch( sig ) {
    case MENU_ID_SIGABRT:
      sig = SIGABRT;
      break;
    case MENU_ID_SIGALRM:
      sig = SIGALRM;
      break;
    case MENU_ID_SIGCHLD:
      sig = SIGCHLD;
      break;
    case MENU_ID_SIGCONT:
      sig = SIGCONT;
      break;
    case MENU_ID_SIGFPE:
      sig = SIGFPE;
      break;
    case MENU_ID_SIGHUP:
      sig = SIGHUP;
      break;
    case MENU_ID_SIGILL:
      sig = SIGILL;
      break;
    case MENU_ID_SIGINT:
      sig = SIGINT;
      break;
    case MENU_ID_SIGKILL:
      sig = SIGKILL;
      break;
    case MENU_ID_SIGPIPE:
      sig = SIGPIPE;
      break;
    case MENU_ID_SIGQUIT:
      sig = SIGQUIT;
      break;
    case MENU_ID_SIGSEGV:
      sig = SIGSEGV;
      break;
    case MENU_ID_SIGSTOP:
      sig = SIGSTOP;
      break;
    case MENU_ID_SIGTERM:
      sig = SIGTERM;
      break;
    case MENU_ID_SIGTSTP:
      sig = SIGTSTP;
      break;
    case MENU_ID_SIGTTIN:
      sig = SIGTTIN;
      break;
    case MENU_ID_SIGTTOU:
      sig = SIGTTOU;
      break;
    case MENU_ID_SIGUSR1:
      sig = SIGUSR1;
      break;
    case MENU_ID_SIGUSR2:
      sig = SIGUSR2;
      break;
  }

  if ( kill( (pid_t)pid, sig ) ) {
    switch ( errno ) {
      case EINVAL:
        output( "4\t%d\n", pid );
        break;
      case ESRCH:
        output( "3\t%d\n", pid );
        break;
      case EPERM:
	if(vfork() == 0) {
	  exit(0);/* Won't execute unless execve fails.  Need this for the parent process to continue */
	}
        output( "2\t%d\n", pid );
        break;
      default: /* unknown error */
        output( "1\t%d\n", pid );
        break;
    }
  } else
    output( "0\t%d\n", pid );
}

void setPriority( const char* cmd )
{
  int pid, prio;
  /** as:  setpriority <pid> <priority> */
  sscanf( cmd, "%*s %d %d", &pid, &prio );
  if ( setpriority( PRIO_PROCESS, pid, prio ) ) {
    switch ( errno ) {
      case EINVAL:
        output( "4\t%d\t%d\n", pid, prio  );
        break;
      case ESRCH:
        output( "3\t%d\t%d\nn", pid, prio );
        break;
      case EPERM:
      case EACCES:
        output( "2\t%d\t%d\n", pid, prio );
        break;
      default: /* unknown error */
        output( "1\t%d\t%d\n", pid, prio );
        break;
    }
  } else
    output( "0\t%d\t%d\n",pid, prio );
}

void ioniceProcess( const char* cmd )
{
  /* Re-ionice's a process. cmd is a string containing:
   *
   * ionice <pid> <class> <priority>
   *
   * where c = 1 for real time, 2 for best-effort, 3 for idle
   * and priority is between 0 and 7, 0 being the highest priority, and ignored if c=3
   *
   * For more information, see:  man ionice
   *
   */
  int pid = 0;
  int class = 2;
  int priority = 0;
  if(sscanf( cmd, "%*s %d %d %d", &pid, &class, &priority ) < 2) {
    output( "4\t%d\n", pid ); /* 4 means error in values */
    return; /* Error with input. */
  }

#ifdef HAVE_IONICE
  if(pid < 1 || class < 0 || class > 3) {
    output( "4\t%d\n", pid ); /* 4 means error in values */
    return; /* Error with input. Just ignore. */
  }

  if (ioprio_set(IOPRIO_WHO_PROCESS, pid, priority | class << IOPRIO_CLASS_SHIFT) == -1) {
    switch ( errno ) {
      case EINVAL:
        output( "4\t%d\n", pid );
        break;
      case ESRCH:
        output( "3\t%d\n", pid );
        break;
      case EPERM:
        output( "2\t%d\n", pid );
        break;
      default: /* unknown error */
        output( "1\t%d\n", pid );
        break;
    }
  } else {
    /* Successful */
    output( "0\t%d\n", pid );
  }
  return;
#else
  /** should never reach here */
  output( "1\t%d\n", pid );
  return;
#endif
}
