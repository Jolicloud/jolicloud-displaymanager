#include <glib.h>
#include "datetime.h"
#include <sys/time.h>


#ifndef USEC_PER_DAY
# define UNIX_EPOCH_START     719163
# define UNIX_TO_INSTANT(unix)					\
  (((unix) + UNIX_EPOCH_START * SEC_PER_DAY) * USEC_PER_SECOND)
# define USEC_PER_SECOND      (G_GINT64_CONSTANT (1000000))
# define USEC_PER_MINUTE      (G_GINT64_CONSTANT (60000000))
# define USEC_PER_HOUR        (G_GINT64_CONSTANT (3600000000))
# define USEC_PER_DAY         (G_GINT64_CONSTANT(86400000000))
# define SEC_PER_DAY          (G_GINT64_CONSTANT (86400))
#endif


void datetime_current_get(DateTime* dt)
{
  struct timeval tv = { 0, };
  guint64 total = 0;

  gettimeofday(&tv, NULL);

  total = tv.tv_usec + UNIX_TO_INSTANT(tv.tv_sec);

  dt->days = total / USEC_PER_DAY;
  dt->usec = total % USEC_PER_DAY;
}

int datetime_hour_get(DateTime* dt)
{
  return (int)(dt->usec / USEC_PER_HOUR);
}


int datetime_minute_get(DateTime* dt)
{
  return (int)((dt->usec % USEC_PER_HOUR) / USEC_PER_MINUTE);
}


int datetime_second_get(DateTime* dt)
{
  return (int)((dt->usec % USEC_PER_MINUTE) / USEC_PER_SECOND);
}


int datetime_microsecond_get(DateTime* dt)
{
  return (int)(dt->usec % USEC_PER_SECOND);
}
