#ifndef _SESSION_H_
# define _SESSION_H_


typedef void (*session_callback)(void);


gboolean session_init(session_callback sessionStartedCallback,
		      session_callback sessionClosedCallback);
void session_cleanup(void);


gboolean session_run(const struct passwd* passwdEntry);


#endif /* _SESSION_H_ */
