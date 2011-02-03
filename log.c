#include <glib/gtypes.h>
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

static FILE* g_LogStdHandle = NULL;
static FILE* g_LogErrHandle = NULL;

gboolean log_init(void)
{
  const char* filePath = config_logfile_path_get();
  time_t rawTime;

  if (g_LogStdHandle != NULL && g_LogErrHandle != NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: log file already initialized\n");
      return TRUE;
    }

  g_LogStdHandle = freopen(filePath, "a", stdout);
  if (g_LogStdHandle == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to open logfile '%s' [%s]\n",
	      filePath, strerror(errno));
      goto onError;
    }

  g_LogErrHandle = freopen(filePath, "a", stderr);
  if (g_LogErrHandle == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to open logfile '%s' [%s]\n",
	      filePath, strerror(errno));
      goto onError;
    }

  setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
  setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

  time(&rawTime);
  printf("Jolicloud-DisplayManager: Starting %s", ctime(&rawTime));

  return TRUE;

 onError:

  if (g_LogStdHandle != NULL)
    {
      fclose(g_LogStdHandle);
      g_LogStdHandle = NULL;
    }

  if (g_LogErrHandle != NULL)
    {
      fclose(g_LogErrHandle);
      g_LogErrHandle = NULL;
    }

  return FALSE;
}


void log_cleanup(void)
{
  time_t rawTime;

  time(&rawTime);
  printf("Jolicloud-DisplayManager: Terminated %s", ctime(&rawTime));

  if (g_LogStdHandle != NULL)
    {
      fclose(g_LogStdHandle);
      g_LogStdHandle = NULL;
    }

  if (g_LogErrHandle != NULL)
    {
      fclose(g_LogErrHandle);
      g_LogErrHandle = NULL;
    }
}
