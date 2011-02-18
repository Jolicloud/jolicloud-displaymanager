#include <glib.h>
#include "xserver.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <sys/types.h>
#include <sys/wait.h>


static pid_t g_xserverPid = 0;
static guint g_xserverPidWatcherId = 0;

static xserver_callback g_xserverTerminated = NULL;

static void _xserver_start(const char* display);

static void _xserver_pid_watcher(GPid pid, gint status, void* context);



gboolean xserver_init(const char* display, xserver_callback xserverTerminated)
{
  if (g_xserverPid != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: XServer already initialized\n");
      return FALSE;
    }

  if (xserverTerminated == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: No termination callback defined\n");
      return FALSE;
    }

  g_xserverTerminated = xserverTerminated;
  g_xserverPid = fork();

  if (g_xserverPid == -1)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to fork for starting X.Org. [%s]\n",
	      strerror(errno));
      return FALSE;
    }

  if (g_xserverPid == 0)
    {
      _xserver_start(display);
      /* this return should never been reached */
      return FALSE;
    }

  g_xserverPidWatcherId = g_child_watch_add(g_xserverPid, _xserver_pid_watcher, NULL);

  return TRUE;
}


void xserver_cleanup(void)
{
  int status;

  if (g_xserverPid == 0)
    return;

  kill(g_xserverPid, SIGTERM);

  waitpid(g_xserverPid, &status, 0);

  g_xserverPid = 0;
  g_xserverPidWatcherId = 0;
  g_xserverTerminated = NULL;
}


pid_t xserver_pid_get(void)
{
  return g_xserverPid;
}


void xserver_rewatch(void)
{
  if (g_xserverPid == 0)
    return;

  g_xserverPidWatcherId = g_child_watch_add(g_xserverPid, _xserver_pid_watcher, NULL);
}


/* privates
 */

static void _xserver_start(const char* display)
{
  gchar** xserverArgs = NULL;
  char* av[32] = { 0, };
  int ac = 0;

  /* FIXME: close log?
   */

  setsid();
  signal(SIGTERM, SIG_DFL);
  signal(SIGHUP, SIG_DFL);

  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGUSR1, SIG_IGN);

  av[ac++] = (char *)config_xserver_path_get();
  av[ac++] = ":0";
  av[ac++] = "-auth";
  av[ac++] = (char *)config_xauthfile_path_get();

  xserverArgs = g_strsplit(config_xserverargs_get(), " ", 0);
  if (xserverArgs != NULL)
    {
      int i;

      for (i = 0; ac < 30 && xserverArgs[i]; ++i)
  	av[ac++] = xserverArgs[i];
    }

  setpgid(0, getpid());

  execv(av[0], av);

  fprintf(stderr, "Jolicloud-DisplayManager: Unable to start X.Org [%s]\n",
	  strerror(errno));

  exit(0);
}


static void _xserver_pid_watcher(GPid pid, gint status, void* context)
{
  g_xserverPidWatcherId = 0;
  g_xserverPid = 0;

  fprintf(stderr, "Jolicloud-DisplayManager: X closed with status %d\n", status);

  g_xserverTerminated();
}
