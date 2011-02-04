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


static pid_t g_xserverPid = 0;
static guint g_xserverPidWatcherId = 0;

static void _xserver_start(const char* display);
static gboolean _xserver_wait_ready(const char* display);

static void _xserver_pid_watcher(GPid pid, gint status, void* context);


gboolean xserver_init(const char* display)
{
  if (g_xserverPid != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: XServer already initialized\n");
      return FALSE;
    }

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

  if (_xserver_wait_ready(display) == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: X.Org failed to start. See X.Org log messages for more information\n");
      return FALSE;
    }

  return TRUE;
}


void xserver_cleanup(void)
{

}


/* privates
 */

static void _xserver_start(const char* display)
{
  gchar** xserverArgs = NULL;
  const char* av[32] = { 0, };
  int ac = 0;

  /* FIXME: close log?
   */

  /* signal(SIGTTIN, SIG_IGN); */
  /* signal(SIGTTOU, SIG_IGN); */
  /* signal(SIGUSR1, SIG_IGN); */

  av[ac++] = config_xserver_path_get();
  av[ac++] = ":0";
  av[ac++] = "-auth";
  av[ac++] = config_xauthfile_path_get();

  xserverArgs = g_strsplit(config_xserverargs_get(), " ", 0);
  if (xserverArgs != NULL)
    {
      int i;

      for (i = 0; ac < 30 && xserverArgs[i]; ++i)
	av[ac++] = xserverArgs[i];
    }

  setpgid(0, getpid());

  execvp(av[0], (char **)av);

  fprintf(stderr, "Jolicloud-DisplayManager: Unable to start X.Org [%s]\n",
	  strerror(errno));

  exit(0);
}


static gboolean _xserver_wait_ready(const char* display)
{
  Display* displayHandle = NULL;
  int max = 3;
  int i = 0;

  while (i < max && (displayHandle = XOpenDisplay(display)) == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: X.Org is still not ready. Waiting 1s\n");
      ++i;
      sleep(1);
    }

  if (displayHandle == NULL)
    return FALSE;

  XCloseDisplay(displayHandle);
  return TRUE;
}


static void _xserver_pid_watcher(GPid pid, gint status, void* context)
{
  g_xserverPidWatcherId = 0;
  g_xserverPid = 0;

  fprintf(stderr, "Jolicloud-DisplayManager: X closed with status %d\n", status);
}
