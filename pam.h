#ifndef _PAM_H_
# define _PAM_H_

#include <security/pam_appl.h>
#ifdef __LIBPAM_VERSION
# include <security/pam_misc.h>
#endif // __LIBPAM_VERSION

typedef enum PAM_ITEM_TYPE
  {
    PAM_ITEM_SERVICE = PAM_SERVICE,
    PAM_ITEM_USER = PAM_USER,
    PAM_ITEM_USER_PROMPT = PAM_USER_PROMPT,
    PAM_ITEM_TTY = PAM_TTY,
    PAM_ITEM_REQUESTOR = PAM_RUSER,
    PAM_ITEM_HOST = PAM_RHOST,
    PAM_ITEM_CONV = PAM_CONV

  } PAM_ITEM_TYPE;


typedef enum PAM_CREDENTIAL_ITEM_TYPE
  {
    PAM_CREDENTIAL_ITEM_USER = 0,
    PAM_CREDENTIAL_ITEM_PASSWORD

  } PAM_CREDENTIAL_ITEM_TYPE;

typedef void (*pam_callback)(PAM_CREDENTIAL_ITEM_TYPE credentialItemType, char** item);

gboolean pam_init(const char* serviceName,
		  pam_callback callback);

void pam_cleanup(void);


int pam_last_status(void);


gboolean pam_item_set(PAM_ITEM_TYPE itemType, const void* value);
gboolean pam_item_get(PAM_ITEM_TYPE itemType, const void** value);

gboolean pam_auth(void);

gboolean pam_session_open(void);
gboolean pam_session_close(void);

#endif /* _PAM_H_ */
