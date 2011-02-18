#include <glib.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "session.h"
#include "config.h"
#include "datetime.h"
#include "pam.h"
#include "ui.h"



static char g_sessionCookie[33] = { 0, };

static session_callback g_sessionStartedCallback = NULL;
static session_callback g_sessionClosedCallback = NULL;

static pid_t g_sessionPid = 0;
static guint g_sessionPidWatcherId = 0;


static gboolean _session_cookie_add(const char* display,
				    const char* cookiePath);
static void _session_watcher(GPid pid, gint status, void* context);
static gboolean _session_started(void* context);

static gboolean _session_user_information_set(const struct passwd* passwdEntry);
static const char* _session_logincmd_get(const struct passwd* passwdEntry);

gboolean session_init(session_callback sessionStartedCallback,
		      session_callback sessionClosedCallback)
{
  const char* hex = "0123456789abcdef";
  DateTime currentTime;
  guint time = 0;
  int i;

  if (sessionStartedCallback == NULL || sessionClosedCallback == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to initialize session component without callbacks\n");
      return FALSE;
    }

  g_sessionStartedCallback = sessionStartedCallback;
  g_sessionClosedCallback = sessionClosedCallback;

  datetime_current_get(&currentTime);
  time = datetime_hour_get(&currentTime);
  time = time * 10 + datetime_minute_get(&currentTime);
  time = time * 10 + datetime_second_get(&currentTime);
  time = time * 1000 + datetime_microsecond_get(&currentTime);

  srand(time);

  for (i = 0; i < 32; i += 4)
    {
      guint16 word = random() & 0xffff;
      guint8 low = word & 0xff;
      guint8 high = word >> 8;

      g_sessionCookie[i] = hex[low & 0x0f];
      g_sessionCookie[i + 1] = hex[low >> 4];
      g_sessionCookie[i + 2] = hex[high & 0x0f];
      g_sessionCookie[i + 3] = hex[high >> 4];
    }

  fprintf(stderr, "Jolicloud-DisplayManager: cookie [%s]\n", g_sessionCookie);

  setenv("XAUTHORITY", config_xauthfile_path_get(), 1);

  return _session_cookie_add(":0", config_xauthfile_path_get());
}


void session_cleanup(void)
{
  memset(g_sessionCookie, 0, 33);

  g_sessionStartedCallback = NULL;
  g_sessionClosedCallback = NULL;

  if (g_sessionPid == 0)
    return;

  killpg(g_sessionPid, SIGHUP);

  if (killpg(g_sessionPid, SIGTERM))
    killpg(g_sessionPid, SIGKILL);

  g_sessionPid = 0;
  g_sessionPidWatcherId = 0;
}


gboolean session_run(const struct passwd* passwdEntry)
{
  pid_t pid;
  gchar* tmp = NULL;
  gchar** userEnv = NULL;
  int i = 0;
  const char* loginCmd = NULL;

  if (g_sessionPid != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Internal Error: A session is already running.\n");
      return FALSE;
    }

  pid = fork();
  if (pid == -1)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to fork for starting the session. [%s]\n",
	      strerror(errno));
      return FALSE;
    }

  if (pid != 0)
    {
      g_sessionPid = pid;
      g_sessionPidWatcherId = g_child_watch_add(pid, _session_watcher, NULL);
      g_timeout_add_seconds(2, _session_started, NULL);
      return TRUE;
    }

  /* set up user information
   */
  if (_session_user_information_set(passwdEntry) == FALSE)
    exit(1);

  /* FIXME: Ensure the check of the shell is usefull
   */
  if (passwdEntry->pw_shell[0] == '\0')
    {
      setusershell();
      fprintf(stderr, "Jolicloud-DisplayManager: setting user shell to '%s'\n", getusershell());
      strcpy(passwdEntry->pw_shell, getusershell());
      endusershell();
    }

  tmp = g_strdup_printf("%s/.Xauthority", passwdEntry->pw_dir);
  if (tmp == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Internal Error: Not enough memory\n");
      exit(1);
    }

  if (_session_cookie_add(":0", (char *)tmp) == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to set user xauth cookie file\n");
      g_free(tmp);
      exit(1);
    }

  g_free(tmp);

  tmp = g_strdup_printf("%s -a -l :0.0 %s", config_sessreg_path_get(), passwdEntry->pw_name);
  if (tmp == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Internal Error: Not enough memory\n");
      exit(1);
    }

  if (system(tmp) == -1)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to register session\n");
      g_free(tmp);
      exit(1);
    }

  g_free(tmp);

  if (chdir(passwdEntry->pw_dir) != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to move to user home dir [%s]\n",
	      strerror(errno));
      exit(1);
    }

  /* FIXME: this is so ugly!
   */
  userEnv = g_malloc(sizeof(gchar *) * 15);
  if (userEnv == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Internal Error: Not enough memory\n");
      exit(1);
    }

  if (getenv("TERM") != NULL)
    userEnv[i++] = g_strdup_printf("TERM=%s", getenv("TERM"));

  userEnv[i++] = g_strdup_printf("HOME=%s", passwdEntry->pw_dir);
  userEnv[i++] = g_strdup_printf("SHELL=%s", passwdEntry->pw_shell);
  userEnv[i++] = g_strdup_printf("USER=%s", passwdEntry->pw_name);
  userEnv[i++] = g_strdup_printf("LOGNAME=%s", passwdEntry->pw_name);
  userEnv[i++] = g_strdup_printf("PATH=%s", config_user_path_get());
  userEnv[i++] = g_strdup_printf("DISPLAY=:0.0");
  userEnv[i++] = g_strdup_printf("MAIL=");
  userEnv[i++] = g_strdup_printf("XAUTHORITY=%s/.Xauthority", passwdEntry->pw_dir);
  userEnv[i] = 0;

  /* retrieve the correct login cmd (normal user or guest)
   */
  loginCmd = _session_logincmd_get(passwdEntry);

  tmp = g_strdup_printf("%s > %s/.jolicloud-displaymanager.log 2>&1",
			loginCmd,
			passwdEntry->pw_dir);

  execle(passwdEntry->pw_shell,
	 passwdEntry->pw_shell,
	 "-c",
	 config_user_logincmd_get(),
	 NULL,
	 userEnv);

  exit(1);
  return FALSE;
}


/* privates
 */

static gboolean _session_cookie_add(const char* display,
				    const char* cookiePath)
{
  gchar* arg = NULL;
  FILE* pipeHandle = NULL;

  printf("Jolicloud-DisplayManager: Adding XAuth cookie\n");

  remove(cookiePath);

  arg = g_strdup_printf("%s -f %s -q", config_xauth_path_get(), cookiePath);
  if (arg == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to initialize session cookie\n");
      return FALSE;
    }

  pipeHandle = popen((char *)arg, "w");
  g_free(arg);
  if (pipeHandle == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to start xauth (%s) [%s]\n",
	      config_xauth_path_get(), strerror(errno));
      return FALSE;
    }

  fprintf(pipeHandle, "remove %s\n", display);
  fprintf(pipeHandle, "add %s . %s\n", display, g_sessionCookie);
  fprintf(pipeHandle, "exit\n");
  pclose(pipeHandle);

  fprintf(stderr, "Jolicloud-DisplayManager: XAuth cookie added\n");

  return TRUE;
}


static void _session_watcher(GPid pid, gint status, void* context)
{
  fprintf(stderr, "Jolicloud-DisplayManager: Session closed with status %d\n", status);

  killpg(g_sessionPid, SIGHUP);

  if (killpg(g_sessionPid, SIGTERM))
    killpg(g_sessionPid, SIGKILL);

  g_sessionPidWatcherId = 0;
  g_sessionPid = 0;

  g_sessionClosedCallback();
}


static gboolean _session_started(void* context)
{
  if (g_sessionPid == 0)
    return FALSE;

  fprintf(stderr, "Jolicloud-DisplayManager: Session marked as started\n");

  g_sessionStartedCallback();

  return FALSE;
}


static gboolean _session_user_information_set(const struct passwd* passwdEntry)
{
  if (initgroups(passwdEntry->pw_name, passwdEntry->pw_gid) != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: initgroups failed [%s]\n",
	      strerror(errno));
      return FALSE;
    }

  if (setgid(passwdEntry->pw_gid) != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: setgid failed [%s]\n",
	      strerror(errno));
      return FALSE;
    }

  if (setuid(passwdEntry->pw_uid) != 0)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: setuid failed [%s]\n",
	      strerror(errno));
      return FALSE;
    }

  return TRUE;
}


static const char* _session_logincmd_get(const struct passwd* passwdEntry)
{
  const char* guestGroup = NULL;
  struct group* grEntry = NULL;

  if (config_guestmode_enabled() == FALSE)
    goto fallback;

  guestGroup = config_guestmode_group_get();
  if (guestGroup == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Guest Mode is enabled from configuration but no group has been specified\n");
      goto fallback;
    }

  grEntry = getgrgid(passwdEntry->pw_gid);
  endgrent();

  if (grEntry == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to retrieve group entry for user '%s' with gid '%d'\n",
	      passwdEntry->pw_name, passwdEntry->pw_gid);
      goto fallback;
    }

  if (!strcmp(grEntry->gr_name, guestGroup))
    {
      const char* logincmd = config_guestmode_logincmd_get();
      if (logincmd == NULL)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Guest Mode is enabled from configuration but no login cmd has been specified\n");
	  goto fallback;
	}

      return logincmd;
    }

 fallback:
  return config_user_logincmd_get();
}
