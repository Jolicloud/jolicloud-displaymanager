#include <glib/gtypes.h>
#include "locker.h"
#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>


static gboolean lock_update(const char* lockPath)
{
  FILE* lockHandle = NULL;
  pid_t pid = 0;

  lockHandle = fopen(lockPath, "w");
  if (lockHandle == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to create lock file '%s' [%s]\n",
	      lockPath, strerror(errno));
      return FALSE;
    }

  pid = getpid();

  if (fwrite(&pid, 1, sizeof(pid_t), lockHandle) != sizeof(pid_t))
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to update lock file '%s' [%s]\n",
	      lockPath, strerror(errno));
      fclose(lockHandle);
      return FALSE;
    }

  fclose(lockHandle);
  return TRUE;
}


gboolean lock_init(void)
{
  const char* lockPath = config_lockfile_path_get();
  FILE* lockHandle = NULL;
  pid_t pid = 0;
  size_t bytesRead = 0;

  lockHandle = fopen(lockPath, "r");
  if (lockHandle == NULL)
    return lock_update(lockPath);

  /* it's a bit tricky, should figure out a better way
   */
  bytesRead = fread(&pid, 1, sizeof(pid_t), lockHandle);
  fclose(lockHandle);

  if (bytesRead == sizeof(pid_t) && pid > 0)
    {
      int ret;

      ret = kill(pid, 0);
      if (ret == 0 || (ret == -1 && errno == EPERM))
	{
	  fprintf(stderr, "Jolicloud-DisplayManager: Another instance is already running (pid %d)\n", pid);
	  return FALSE;
	}
    }

  return lock_update(lockPath);
}


void lock_cleanup(void)
{
  const char* lockPath = config_lockfile_path_get();

  if (remove(lockPath) == -1)
    fprintf(stderr, "Jolicloud-DisplayManager: Unable to remove lock file '%s' [%s]\n",
	    lockPath, strerror(errno));
}
