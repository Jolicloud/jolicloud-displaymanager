#include <glib.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>


/* FIXME: avoid loading of ~ and dot files
 */


#define CONFIG_FREE_STRING_VAR(__var)		\
  do						\
    {						\
      if ((__var) != NULL)			\
	{					\
	  g_free((gchar *)(__var));		\
	  (__var) = NULL;			\
	}					\
    }						\
  while (0)

#define CONFIG_CHECK_STRING_VAR(__var, __error, __failOnError)		\
  do									\
    {									\
      if ((__failOnError) == TRUE && (__var) == NULL)			\
	{								\
	  fprintf(stderr, "Jolicloud-DisplayManager: No configuration entry found for " # __error "\n"); \
	  goto onError;							\
	}								\
    }									\
  while (0)

#define CONFIG_GET_STRING_VALUE(__group, __key, __var)			\
  do									\
    {									\
      char* localTmp;							\
      GError* localError = NULL;					\
      localTmp = g_key_file_get_string(keyFile, (__group), (__key), &localError); \
      if (localTmp != NULL)						\
	{								\
	  if (!(*localTmp))						\
	    {								\
	      g_free((gchar *)localTmp);				\
	      localTmp = NULL;						\
	    }								\
	  if ((__var) != NULL)						\
	    g_free((gchar *)(__var));					\
	  (__var) = (char *)localTmp;					\
	}								\
      else								\
	{								\
	  g_error_free(localError);					\
	}								\
    }									\
  while (0)



static char* g_configLockFilePath = NULL;
static char* g_configLogFilePath = NULL;
static char* g_configXAuthPath = NULL;
static char* g_configXAuthFilePath = NULL;
static char* g_configSessregPath = NULL;
static char* g_configThemeUrl = NULL;
static char* g_configUserPath = NULL;
static char* g_configUserLoginCmd = NULL;
static char* g_configGuestLoginCmd = NULL;
static char* g_configAutologin = NULL;

static gboolean _config_parse_file(const char* filePath);


gboolean config_init(void)
{
  return TRUE;
}


void config_cleanup(void)
{
  CONFIG_FREE_STRING_VAR(g_configLockFilePath);
  CONFIG_FREE_STRING_VAR(g_configLogFilePath);
  CONFIG_FREE_STRING_VAR(g_configXAuthPath);
  CONFIG_FREE_STRING_VAR(g_configXAuthFilePath);
  CONFIG_FREE_STRING_VAR(g_configSessregPath);
  CONFIG_FREE_STRING_VAR(g_configThemeUrl);
  CONFIG_FREE_STRING_VAR(g_configUserPath);
  CONFIG_FREE_STRING_VAR(g_configUserLoginCmd);
  CONFIG_FREE_STRING_VAR(g_configGuestLoginCmd);
  CONFIG_FREE_STRING_VAR(g_configAutologin);
}


gboolean config_load(const char* configPath)
{
  GDir* dir = NULL;
  GError* error = NULL;
  const gchar* configFile = NULL;

  dir = g_dir_open(configPath, 0, &error);
  if (dir == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to open configuration path '%s'. Error '%s'\n",
	      configPath, error->message);
      goto onError;
    }

  /* according to glib's documentation: DO NOT FREE RETURN VALUE OF g_dir_read_name
   */
  while ((configFile = g_dir_read_name(dir)) != NULL)
    {
      gchar* filePath = NULL;

      filePath = g_build_filename(configPath, configFile, NULL);
      if (filePath == NULL)
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Unable to construct configuration file path\n");
	  goto onError;
	}

      if (g_file_test(filePath, G_FILE_TEST_IS_REGULAR) == TRUE
	  && _config_parse_file(filePath) == FALSE)
	{
	  g_free(filePath);
	  goto onError;
	}

      g_free(filePath);
    }

  CONFIG_CHECK_STRING_VAR(g_configLockFilePath, "lockfile", TRUE);
  CONFIG_CHECK_STRING_VAR(g_configLogFilePath, "logfile", TRUE);

  CONFIG_CHECK_STRING_VAR(g_configXAuthPath, "xauth", TRUE);
  CONFIG_CHECK_STRING_VAR(g_configXAuthFilePath, "xauthfile", TRUE);
  CONFIG_CHECK_STRING_VAR(g_configSessregPath, "sessreg", TRUE);

  CONFIG_CHECK_STRING_VAR(g_configThemeUrl, "theme", TRUE);

  CONFIG_CHECK_STRING_VAR(g_configUserPath, "path", TRUE);
  CONFIG_CHECK_STRING_VAR(g_configUserLoginCmd, "logincmd", TRUE);

  CONFIG_CHECK_STRING_VAR(g_configGuestLoginCmd, "[guest] logincmd", FALSE);

  CONFIG_CHECK_STRING_VAR(g_configAutologin, "[autologin] login", FALSE);

  g_dir_close(dir);

  return TRUE;

 onError:

  if (error != NULL)
    g_error_free(error);

  if (dir != NULL)
    g_dir_close(dir);

  CONFIG_FREE_STRING_VAR(g_configLockFilePath);
  CONFIG_FREE_STRING_VAR(g_configLogFilePath);
  CONFIG_FREE_STRING_VAR(g_configXAuthPath);
  CONFIG_FREE_STRING_VAR(g_configXAuthFilePath);
  CONFIG_FREE_STRING_VAR(g_configSessregPath);
  CONFIG_FREE_STRING_VAR(g_configThemeUrl);
  CONFIG_FREE_STRING_VAR(g_configUserPath);
  CONFIG_FREE_STRING_VAR(g_configUserLoginCmd);
  CONFIG_FREE_STRING_VAR(g_configGuestLoginCmd);
  CONFIG_FREE_STRING_VAR(g_configAutologin);

  return FALSE;
}


const char* config_lockfile_path_get(void)
{
  return g_configLockFilePath;
}


const char* config_logfile_path_get(void)
{
  return g_configLogFilePath;
}


const char* config_xauth_path_get(void)
{
  return g_configXAuthPath;
}


const char* config_xauthfile_path_get(void)
{
  return g_configXAuthFilePath;
}


const char* config_sessreg_path_get(void)
{
  return g_configSessregPath;
}


const char* config_theme_url_get(void)
{
  return g_configThemeUrl;
}


const char* config_user_path_get(void)
{
  return g_configUserPath;
}


const char* config_user_logincmd_get(void)
{
  return g_configUserLoginCmd;
}


const char* config_guest_logincmd_get(void)
{
  return g_configGuestLoginCmd;
}


const char* config_autologin_get(void)
{
  return g_configAutologin;
}


/* privates
 */

static gboolean _config_parse_file(const char* filePath)
{
  GKeyFile* keyFile = NULL;
  GError* error = NULL;

  keyFile = g_key_file_new();
  if (keyFile == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to initialize parsing of configuration file '%s'\n",
	      filePath);
      return FALSE;
    }

  if (g_key_file_load_from_file(keyFile, filePath, G_KEY_FILE_NONE, &error) == FALSE)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Failed to load configuration file '%s'. Error '%s'\n",
	      filePath, error->message);
      goto onError;
    }

  /* we don't care about error because we may load more than one file,
     each one having some part of the configuration.
     Errors will be handled in the parent function.
  */

  CONFIG_GET_STRING_VALUE("internals", "lockfile", g_configLockFilePath);
  CONFIG_GET_STRING_VALUE("internals", "logfile", g_configLogFilePath);

  CONFIG_GET_STRING_VALUE("xserver", "xauth", g_configXAuthPath);
  CONFIG_GET_STRING_VALUE("xserver", "xauthfile", g_configXAuthFilePath);
  CONFIG_GET_STRING_VALUE("xserver", "sessreg", g_configSessregPath);

  CONFIG_GET_STRING_VALUE("theme", "url", g_configThemeUrl);

  CONFIG_GET_STRING_VALUE("user", "path", g_configUserPath);
  CONFIG_GET_STRING_VALUE("user", "logincmd", g_configUserLoginCmd);

  CONFIG_GET_STRING_VALUE("guest", "logincmd", g_configGuestLoginCmd);

  CONFIG_GET_STRING_VALUE("autologin", "login", g_configAutologin);

  g_key_file_free(keyFile);

  return TRUE;

 onError:

  if (error != NULL)
    g_error_free(error);

  if (keyFile != NULL)
    g_key_file_free(keyFile);

  return FALSE;
}
