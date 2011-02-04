#ifndef _CONFIG_H_
# define _CONFIG_H_

gboolean config_init(void);
void config_cleanup(void);

gboolean config_load(const char* configPath);


/* DO NOT FREE RETURNED VALUES
 */
const char* config_lockfile_path_get(void);
const char* config_logfile_path_get(void);

const char* config_xauth_path_get(void);
const char* config_xauthfile_path_get(void);
const char* config_sessreg_path_get(void);
const char* config_xserver_path_get(void);
const char* config_xserverargs_get(void);

const char* config_theme_url_get(void);

const char* config_user_path_get(void);
const char* config_user_logincmd_get(void);

gboolean config_guestmode_enabled(void);
const char* config_guestmode_login_get(void);
const char* config_guestmode_group_get(void);
const char* config_guestmode_logincmd_get(void);

gboolean config_autologin_enabled(void);
const char* config_autologin_login_get(void);

#endif /* _CONFIG_H_ */
