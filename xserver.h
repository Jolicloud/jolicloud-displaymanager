#ifndef _XSERVER_H_
# define _XSERVER_H_

typedef void (*xserver_callback)(void);

gboolean xserver_init(const char* display, xserver_callback xserverTerminated);

void xserver_cleanup(void);

pid_t xserver_pid_get(void);

void xserver_rewatch(void);

#endif /* _XSERVER_H_ */
