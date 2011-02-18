#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "displaymanager.h"
#include "config.h"
#include "locker.h"
#include "log.h"
#include "pam.h"
#include "session.h"
#include "xserver.h"
#include "ui.h"


static char* g_dmConfigurationPath = NULL;
static gboolean g_dmFirstRun = TRUE;
static gboolean g_dmReload = FALSE;
static GMainLoop* g_dmMainLoop = NULL;
static int g_dmSignalPipe[2];


static gboolean _dm_signal_handler_prepare(void);
static gboolean _dm_signal_handler_set(void);

static gboolean _dm_arguments_parse(int ac, char** av);

static void _dm_xserver_ready(void);
static void _dm_xserver_terminated(void);

static void _dm_pam_callback(PAM_CREDENTIAL_ITEM_TYPE credentialItemType, char** item);

static gboolean _dm_autologin(void* context);

static void _dm_session_started(void);
static void _dm_session_closed(void);

static gboolean _dm_do_authentication(void);
static gboolean _dm_session_run(void);
static void _dm_sign_in(void);


static void gdk_set_root_window_cursor(GdkCursorType cursorType)
{
  GdkDisplay* display;
  GdkCursor* cursor;
  GdkWindow* root;

  display = gdk_display_get_default();
  cursor = gdk_cursor_new(cursorType);
  root = gdk_get_default_root_window();
  gdk_window_set_cursor(root, cursor);
  gdk_cursor_unref(cursor);
}


gboolean dm_init(int ac, char** av)
{
  char* display = NULL;

  if (_dm_signal_handler_prepare() == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to prepare signals handlers\n");
      return FALSE;
    }

  display = getenv("DISPLAY");
  if (display == NULL)
    {
      display = ":0.0";
      setenv("DISPLAY", display, 1);
    }

  /* parse parameters
   */
  if (_dm_arguments_parse(ac, av) == FALSE)
    return FALSE;

  /* early initialization of glib. config uses glib objects
   * According to the documentation, g_type_init already calls g_thread_init.
   */
  g_type_init();
  g_dmMainLoop = g_main_loop_new(NULL, FALSE);

  if (_dm_signal_handler_set() == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to link signal handler to the main loop\n");
      return FALSE;
    }

  /* load configurations
   */
  if (config_init() == FALSE || config_load(g_dmConfigurationPath) == FALSE)
    return FALSE;

  /* initialize lock file containing our pid
   */
  if (lock_init() == FALSE)
    goto onLockInitError;

  /* initialize log file
   */
  if (log_init() == FALSE)
    goto onLogInitError;

  /* initialize pam
   */
  if (pam_init(PACKAGE, _dm_pam_callback) == FALSE
      || pam_item_set(PAM_ITEM_TTY, display) == FALSE
      || pam_item_set(PAM_ITEM_REQUESTOR, "root") == FALSE
      || pam_item_set(PAM_ITEM_HOST, "localhost") == FALSE)
    goto onPamInitError;

  /* initialize session base
   */
  if (session_init(_dm_session_started, _dm_session_closed) == FALSE)
    goto onSessionInitError;

  /* start X
   */
  if (xserver_init(display, _dm_xserver_terminated) == FALSE)
    goto onXServerInitError;

  return TRUE;

 onXServerInitError:
  session_cleanup();

 onSessionInitError:
  pam_cleanup();

 onPamInitError:
  log_cleanup();

 onLogInitError:
  lock_cleanup();

 onLockInitError:
  config_cleanup();

  return FALSE;
}


void dm_run(void)
{
  /* main loop even if we don't have something to display
     because we can have dbus messages
   */

  while (1)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Entering main loop\n");
      g_main_loop_run(g_dmMainLoop);
      fprintf(stderr, "Jolicloud-DisplayManager: Out of main loop [reload %d]\n", g_dmReload);

      if (g_dmReload == FALSE)
	break;

      g_dmReload = FALSE;

      gdk_display_close(gdk_display_get_default());

      xserver_cleanup();

      session_cleanup();

      if (_dm_signal_handler_prepare() == FALSE)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Unable to prepare signals handlers\n");
	  exit(1);
	}

      g_type_init();
      g_dmMainLoop = g_main_loop_new(NULL, FALSE);

      if (_dm_signal_handler_set() == FALSE)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Unable to link signal handler to the main loop\n");
	  exit(1);
	}

      if (config_load(g_dmConfigurationPath) == FALSE)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Internal Error: Configuration is broken!\n");
	  exit(1);
	}

      if (session_init(_dm_session_started, _dm_session_closed) == FALSE)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Failed to initialize session\n");
	  exit(1);
	}

      if (xserver_init(getenv("display"), _dm_xserver_terminated) == FALSE)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Failed to start X.Org\n");
	  exit(1);
	}
    }
}


void dm_cleanup(void)
{
  ui_cleanup();

  /* FIXME: Should we close any running session properly?
   */

  xserver_cleanup();
  session_cleanup();
  pam_cleanup();
  log_cleanup();
  lock_cleanup();
  config_cleanup();

  g_main_loop_unref(g_dmMainLoop);
}




/* privates */


/* TODO: rewrite the whole signal handler thing as a real GObject thing
 */

struct SIGNAL_INFORMATION
{
  int signalNumber;
  pid_t pid;
};


static gboolean _dm_io_channel_watcher(GIOChannel* ioSource, GIOCondition ioCondition, gpointer data)
{
  struct SIGNAL_INFORMATION signalInformation;

  if (read(g_dmSignalPipe[0], &signalInformation, sizeof(struct SIGNAL_INFORMATION)) < 0)
    return TRUE;

  switch (signalInformation.signalNumber)
    {
    case SIGUSR1:
      if (xserver_pid_get() == signalInformation.pid)
	_dm_xserver_ready();
      break;

    default:
      fprintf(stderr, "Jolicloud-DisplayManager: signal %d received\n", signalInformation.signalNumber);
      session_cleanup();
      xserver_cleanup();
      /* g_main_loop_quit(g_dmMainLoop); */
      exit(1);
      break;
    }

  return TRUE;
}


static void _dm_signal_handler(int signalNumber, siginfo_t* sigInfo, void* data)
{
  struct SIGNAL_INFORMATION signalInformation;

  signalInformation.signalNumber = sigInfo->si_signo;
  signalInformation.pid = sigInfo->si_pid;

  if (write(g_dmSignalPipe[1], &signalInformation, sizeof(struct SIGNAL_INFORMATION)) < 0)
    fprintf(stderr, "Jolicloud-DisplayManager: Failed to write in signal pipe [%s]\n", strerror(errno));
}


static gboolean _dm_signal_handler_prepare(void)
{
  struct sigaction sigAction;

  if (pipe(g_dmSignalPipe) != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to create the signal pipe [%s]\n", strerror(errno));
      return FALSE;
    }

  fcntl(g_dmSignalPipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(g_dmSignalPipe[1], F_SETFD, FD_CLOEXEC);

  sigAction.sa_sigaction = _dm_signal_handler;
  sigemptyset(&sigAction.sa_mask);
  sigAction.sa_flags = SA_SIGINFO;

  sigaction(SIGTERM, &sigAction, NULL);
  sigaction(SIGINT, &sigAction, NULL);
  sigaction(SIGHUP, &sigAction, NULL);
  sigaction(SIGUSR1, &sigAction, NULL);

  return TRUE;
}


static gboolean _dm_signal_handler_set(void)
{
  GIOChannel* ioChannel;

  ioChannel = g_io_channel_unix_new(g_dmSignalPipe[0]);
  if (ioChannel == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to create a glib io channel\n");
      return FALSE;
    }

  g_io_channel_set_flags(ioChannel, G_IO_FLAG_NONBLOCK, NULL);
  g_io_add_watch(ioChannel, G_IO_IN, _dm_io_channel_watcher, NULL);
  g_io_channel_set_close_on_unref(ioChannel, TRUE);
  g_io_channel_unref(ioChannel);

  return TRUE;
}


static void _dm_usage(void)
{
  fprintf(stderr,
	  "usage: jolicloud-dm [option ...]\n" \
	  "options:\n"			       \
	  "    -c /path/to/configuration/dir\n");
}


static gboolean _dm_arguments_parse(int ac, char** av)
{
  /* FIXME: arguments was handled as a Quick&Dirty. Do it the right way!
   */

  if (ac == 1)
    {
      g_dmConfigurationPath = strdup(SYSCONFDIR);
      if (g_dmConfigurationPath == NULL)
	return FALSE;
      return TRUE;
    }

  if (ac == 2 && !strcmp(av[1], "--version"))
    {
      printf("%s %s\n", PACKAGE, VERSION);
      return FALSE;
    }

  if (ac != 3
      || (ac == 2 && !strcmp(av[1], "--help"))
      || strcmp(av[1], "-c"))
    {
      _dm_usage();
      return FALSE;
    }

  g_dmConfigurationPath = strdup(av[2]);
  if (g_dmConfigurationPath == NULL)
    return FALSE;

  return TRUE;
}


static void _dm_root_window_prepare(void)
{
  gdk_set_root_window_cursor(GDK_TOP_LEFT_ARROW);

  /* move the cursor to avoid having the crossair cursor from X
     @jeremyB has begged me on his knees to have the cursor at 10,10 (top, left) :-)
  */
  gdk_display_warp_pointer(gdk_display_get_default(),
			   gdk_display_get_default_screen(gdk_display_get_default()),
			   10, 10);
}


static void _dm_xserver_ready(void)
{
  int argc = 1;
  char* av = PACKAGE;
  char** argv = &av;

  /* The thing bellow should never disappear!
   */
  {
    int ret = system("/bin/plymouth quit --retain-splash");
    if (WIFEXITED(ret))
      fprintf(stderr, "Jolicloud-DisplayManager: plymouth exited, status=%d\n", WEXITSTATUS(ret));
    else if (WIFSIGNALED(ret))
      fprintf(stderr, "Jolicloud-DisplayManager: plymouth killed, signal=%d\n", WTERMSIG(ret));
  }

  gtk_init(&argc, &argv);

  _dm_root_window_prepare();

  /* we delay the autologin to hit the main loop in order to have a cursor correctly set
     It's not a joke...
  */
  if (g_dmFirstRun == TRUE && config_autologin_enabled() == TRUE)
    {
      g_dmFirstRun = FALSE;
      g_idle_add(_dm_autologin, NULL);
      return;
    }

  ui_init(_dm_sign_in);
}


static void _dm_xserver_terminated(void)
{
  /* printf("[CALLBACK] X.Org terminated\n"); */
}


static void _dm_pam_callback(PAM_CREDENTIAL_ITEM_TYPE credentialItemType, char** item)
{
  const char* tmp = NULL;

  switch (credentialItemType)
    {
    case PAM_CREDENTIAL_ITEM_USER:
      tmp = ui_username_get();
      if (tmp != NULL)
	*item = strdup(tmp);
      else
	*item = NULL;
      break;

    case PAM_CREDENTIAL_ITEM_PASSWORD:
      tmp = ui_password_get();
      if (tmp != NULL)
	*item = strdup(tmp);
      else
	*item = NULL;
      break;
    }
}


static gboolean _dm_autologin(void* context)
{
  const char* defaultLogin = config_autologin_login_get();

  fprintf(stderr, "Jolicloud-DisplayManager: Autologin requested for user '%s'\n", defaultLogin);

  if (pam_item_set(PAM_ITEM_USER, defaultLogin) == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Failed to define user '%s' for autologin. Pam failed with error %d\n",
	      defaultLogin, pam_last_status());
    }
  else
    {
      if (_dm_session_run() == FALSE)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Failed to run session for user '%s' for autologin.\n",
		  defaultLogin);
	}
    }

  return FALSE;
}


static void _dm_session_started(void)
{
  ui_cleanup();
}


static void _dm_session_closed(void)
{
  int ret;

  g_dmReload = TRUE;

  pam_session_close();

  ret = system("/bin/plymouth show-splash");
  if (WIFEXITED(ret))
    fprintf(stderr, "Jolicloud-DisplayManager: plymouth exited, status=%d\n", WEXITSTATUS(ret));
  else if (WIFSIGNALED(ret))
    fprintf(stderr, "Jolicloud-DisplayManager: plymouth killed, signal=%d\n", WTERMSIG(ret));

  g_main_loop_quit(g_dmMainLoop);
  g_main_loop_unref(g_dmMainLoop);
}


static gboolean _dm_do_authentication(void)
{
  pam_item_set(PAM_ITEM_USER, NULL);

  if (pam_auth() == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Pam Authentication failed with error %d\n",
	      pam_last_status());
      ui_report_status(STATUS_FAILURE);
      return FALSE;
    }

  return TRUE;
}


static gboolean _dm_session_run(void)
{
  char* realUsername = NULL;
  struct passwd* passwdEntry = NULL;

  if (pam_session_open() == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Pam session opening failed with error %d\n",
	      pam_last_status());
      ui_report_status(STATUS_INTERNAL_ERROR);
      return FALSE;
    }

  if (pam_item_get(PAM_ITEM_USER, (const void **)(&realUsername)) == FALSE
      || realUsername == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to retrieve real username from PAM. Error %d\n",
	      pam_last_status());
      ui_report_status(STATUS_INTERNAL_ERROR);
      goto onError;
    }

  passwdEntry = getpwnam(realUsername);

  /* FIXME: Should we release tne password entry now?
   */
  endpwent();

  if (passwdEntry == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to retrieve passwd entry for username '%s' [%s]\n",
	      realUsername, strerror(errno));
      goto onError;
    }

  /* if (realUsername != NULL) */
  /*   free(realUsername); */

  if (session_run(passwdEntry) == FALSE)
    fprintf(stderr, "Jolicloud-DisplayManager: Failed to initialize session\n");

  return TRUE;

 onError:

  /* if (realUsername != NULL) */
  /*   free(realUsername); */

  pam_session_close();

  return FALSE;
}


static void _dm_sign_in(void)
{
  if (_dm_do_authentication() == FALSE)
    return;

  _dm_session_run();
}
