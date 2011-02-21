#ifndef _UI_H_
# define _UI_H_


typedef enum STATUS_CODE
  {
    STATUS_FAILURE = -2,
    STATUS_INTERNAL_ERROR = -1,

    STATUS_SUCCESS = 0,

  } STATUS_CODE;


typedef void (*ui_callback)(void);


gboolean ui_init(ui_callback readyCallback,
		 ui_callback signinCallback);

void ui_cleanup(void);


void ui_show(void);
void ui_hide(void);


void ui_report_status(STATUS_CODE statusCode);


const char* ui_username_get(void);
const char* ui_password_get(void);



#endif /* _UI_H_ */
