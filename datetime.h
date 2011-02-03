#ifndef _DATETIME_H_
# define _DATETIME_H_


typedef struct _DateTime
{
  guint64 days;
  guint64 usec;
} DateTime;


void datetime_current_get(DateTime* dt);

int datetime_hour_get(DateTime* dt);
int datetime_minute_get(DateTime* dt);
int datetime_second_get(DateTime* dt);
int datetime_microsecond_get(DateTime* dt);


#endif /* _DATETIME_H_ */
