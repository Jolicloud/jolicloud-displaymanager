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
#include "displaymanager.h"
#include "config.h"
#include "locker.h"
#include "log.h"
#include "pam.h"
#include "session.h"
#include "xserver.h"
#include "ui.h"


static char* g_dmConfigurationPath = NULL;

static void _dm_signal_handler(int signal);

static gboolean _dm_arguments_parse(int ac, char** av);
static void _dm_pam_callback(PAM_CREDENTIAL_ITEM_TYPE credentialItemType, char** item);

static void _dm_session_started(void);
static void _dm_session_closed(void);

static gboolean _dm_do_authentication(void);
static gboolean _dm_session_run(void);
static void _dm_sign_in(void);


gboolean dm_init(int ac, char** av)
{
  char* display = getenv("DISPLAY");
  const char* defaultLogin = NULL;
  gboolean initUI = TRUE;

  /* parse parameters
   */
  if (_dm_arguments_parse(ac, av) == FALSE)
    return FALSE;

  if (display == NULL)
    {
      display = ":0.0";
      setenv("DISPLAY", display, 1);
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
  if (pam_init("Jolicloud-DM", _dm_pam_callback) == FALSE
      || pam_item_set(PAM_ITEM_TTY, ":0.0") == FALSE
      || pam_item_set(PAM_ITEM_REQUESTOR, "root") == FALSE
      || pam_item_set(PAM_ITEM_HOST, "localhost") == FALSE)
    goto onPamInitError;

  /* initialize session base
   */
  if (session_init(_dm_session_started, _dm_session_closed) == FALSE)
    goto onSessionInitError;

  /* start X
   */
  if (xserver_init(display) == FALSE)
    goto onXServerInitError;

  /* handle signals
   */
  signal(SIGQUIT, _dm_signal_handler);
  signal(SIGTERM, _dm_signal_handler);
  signal(SIGKILL, _dm_signal_handler);
  signal(SIGINT, _dm_signal_handler);
  signal(SIGHUP, _dm_signal_handler);
  signal(SIGPIPE, _dm_signal_handler);

  /* The thing bellow should never disappear!
   */
  {
    int ret = system("/bin/plymouth quit --retain-splash");
    if (WIFEXITED(ret))
      fprintf(stderr, "Jolicloud-DisplayManager: plymouth exited, status=%d\n", WEXITSTATUS(ret));
    else if (WIFSIGNALED(ret))
      fprintf(stderr, "Jolicloud-DisplayManager: plymouth killed, signal=%d\n", WTERMSIG(ret));
  }

  gtk_init(&ac, &av);

  if (config_autologin_enabled() == TRUE && (defaultLogin = config_autologin_login_get()) != NULL)
    {
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
	  else
	    {
	      initUI = FALSE;
	    }
	}
    }

  if (initUI == TRUE)
    ui_init(_dm_sign_in);

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

  gtk_main();
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
}




/* privates */


static void _dm_signal_handler(int signal)
{
  fprintf(stderr, "Jolicloud-DisplayManager: signal received %d. Terminating...\n", signal);

  /* FIXME: Should we close any running session properly?
   */

  xserver_cleanup();
  session_cleanup();

  exit(1);
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


static void _dm_session_started(void)
{
  ui_cleanup();
}


static void _dm_session_closed(void)
{
  pam_session_close();

  if (config_load(g_dmConfigurationPath) == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Internal Error: Configuration is broken!\n");
      exit(1);
    }

  ui_init(_dm_sign_in);
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

  if (realUsername != NULL)
    free(realUsername);

  if (session_run(passwdEntry) == FALSE)
    fprintf(stderr, "Jolicloud-DisplayManager: Failed to initialize session\n");

  return TRUE;

 onError:

  if (realUsername != NULL)
    free(realUsername);

  pam_session_close();

  return FALSE;
}


static void _dm_sign_in(void)
{
  if (_dm_do_authentication() == FALSE)
    return;

  _dm_session_run();
}
