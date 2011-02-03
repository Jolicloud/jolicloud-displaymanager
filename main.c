#include <glib/gtypes.h>
#include "displaymanager.h"

int main(int ac, char** av)
{
  /* if dm_init failed it should report error directly in its log file
   */
  if (dm_init(ac, av) == TRUE)
    {
      dm_run();

      dm_cleanup();
    }

  return 0;
}
