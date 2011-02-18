#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>
#include "ui.h"
#include "config.h"
#include <string.h>


static GtkWidget* g_uiMainWindow = NULL;
static GtkWidget* g_uiWebView = NULL;

static char* g_uiUsername = NULL;
static char* g_uiPassword = NULL;
static JSGlobalContextRef g_uiJSGlobalContext = NULL;
static JSValueRef g_uiJSCallbackValue = NULL;
static JSObjectRef g_uiJSCallback = NULL;

static JSValueRef g_uiJSCapslockCallbackValue = NULL;
static JSObjectRef g_uiJSCapslockCallback = NULL;

static sign_in_callback g_uiSignInCallback = NULL;


static gboolean _ui_main_window_delete_event(GtkWidget* widget, GdkEvent* event,
					     void* context);
static void _ui_keymap_state_changed(GdkKeymap* keymap, void* context);

static void _uibind_objects(WebKitWebView* webkitWebView,
			    WebKitWebFrame* webkitWebFrame,
			    JSGlobalContextRef context,
			    JSObjectRef windowObject,
			    void* data);



gboolean ui_init(sign_in_callback signinCallback)
{
  GdkDisplay* display;
  GdkScreen* screen;
  GdkKeymap* keymap;
  gint screenWidth;
  gint screenHeight;
  GdkColor colorBlack = { 0, };

  if (signinCallback == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: UI is being initialized without a callback\n");
      return FALSE;
    }

  g_uiSignInCallback = signinCallback;

  display = gdk_display_get_default();
  screen = gdk_display_get_default_screen(display);
  screenWidth = gdk_screen_get_width(screen);
  screenHeight = gdk_screen_get_height(screen);

  keymap = gdk_keymap_get_for_display(display);
  if (keymap != NULL)
    g_signal_connect(G_OBJECT(keymap), "state-changed",
		     G_CALLBACK(_ui_keymap_state_changed), NULL);

  g_uiMainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_modify_bg(g_uiMainWindow, GTK_STATE_NORMAL, &colorBlack);
  gtk_window_set_default_size(GTK_WINDOW(g_uiMainWindow), screenWidth, screenHeight);
  gtk_window_fullscreen(GTK_WINDOW(g_uiMainWindow));

  /* AlwaysOnTop
   */
  gtk_window_set_keep_above(GTK_WINDOW(g_uiMainWindow), TRUE);


  /* skip taskbar & pager
   */
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_uiMainWindow), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(g_uiMainWindow), TRUE);

  /* remove decoration
   */
  gtk_window_set_decorated(GTK_WINDOW(g_uiMainWindow), FALSE);

  /* disable close button (should not be available since we don't have any decoration!)
   */
  gtk_window_set_deletable(GTK_WINDOW(g_uiMainWindow), FALSE);

  g_signal_connect(G_OBJECT(g_uiMainWindow), "delete-event",
		   G_CALLBACK(_ui_main_window_delete_event), NULL);

  /* FIXME:
     - disable context menu (it has been already disabled from within the theme)
  */

  g_uiWebView = webkit_web_view_new();
  gtk_widget_modify_bg(g_uiWebView, GTK_STATE_NORMAL, &colorBlack);
  g_signal_connect(G_OBJECT(g_uiWebView),
  		   "window-object-cleared",
  		   G_CALLBACK(_uibind_objects),
  		   NULL);

  gtk_container_add(GTK_CONTAINER(g_uiMainWindow), g_uiWebView);

  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(g_uiWebView),
			   config_theme_url_get());

  fprintf(stderr, "Jolicloud-DisplayManager: theme url [%s]\n",
	  config_theme_url_get());

  gtk_widget_show_all(g_uiMainWindow);

  return TRUE;
}


void ui_cleanup(void)
{
  if (g_uiMainWindow == NULL)
    return;

  if (g_uiJSCallbackValue != NULL)
    {
      JSValueUnprotect(g_uiJSGlobalContext, g_uiJSCallbackValue);
      g_uiJSCallbackValue = NULL;
      g_uiJSCallback = NULL;
    }

  if (g_uiJSCapslockCallbackValue != NULL)
    {
      JSValueUnprotect(g_uiJSGlobalContext, g_uiJSCapslockCallbackValue);
      g_uiJSCapslockCallbackValue = NULL;
      g_uiJSCapslockCallback = NULL;
    }

  g_uiJSGlobalContext = NULL;

  gtk_widget_destroy(g_uiMainWindow);

  g_uiMainWindow = NULL;
  g_uiWebView = NULL;

  if (g_uiUsername != NULL)
    {
      memset(g_uiUsername, 0, strlen(g_uiUsername));
      g_free(g_uiUsername);
      g_uiUsername = NULL;
    }

  if (g_uiPassword != NULL)
    {
      memset(g_uiPassword, 0, strlen(g_uiPassword));
      g_free(g_uiPassword);
      g_uiPassword = NULL;
    }
}


void ui_show(void)
{
  if (g_uiMainWindow != NULL)
    gtk_widget_show(g_uiMainWindow);
}


void ui_hide(void)
{
  if (g_uiMainWindow != NULL)
    gtk_widget_hide(g_uiMainWindow);
}


const char* ui_username_get(void)
{
  return g_uiUsername;
}


const char* ui_password_get(void)
{
  return g_uiPassword;
}


void ui_report_status(STATUS_CODE statusCode)
{
  JSValueRef jsStatusCode = NULL;

  if (g_uiJSCallback == NULL || g_uiJSGlobalContext == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to report status code to UI since nothing is initialized\n");
      return;
    }

  jsStatusCode = JSValueMakeNumber(g_uiJSGlobalContext, (double)statusCode);
  if (jsStatusCode == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Internal JavaScriptCore error\n");
      return;
    }

  JSObjectCallAsFunction(g_uiJSGlobalContext,
			 g_uiJSCallback,
			 NULL,
			 1, &jsStatusCode,
			 NULL);
}



/* privates
 */

static gboolean _ui_main_window_delete_event(GtkWidget* widget, GdkEvent* event,
					     void* context)
{
  /* prevent user from closing the window
   */
  return TRUE;
}


static void _ui_keymap_state_changed(GdkKeymap* keymap, void* context)
{
  JSValueRef jsCapslockStatus = NULL;

  if (keymap == NULL || g_uiJSCapslockCallback == NULL || g_uiJSGlobalContext == NULL)
    return;

  jsCapslockStatus = JSValueMakeBoolean(g_uiJSGlobalContext,
					(bool)(gdk_keymap_get_caps_lock_state(keymap)));
  if (jsCapslockStatus == NULL)
    return;

  JSObjectCallAsFunction(g_uiJSGlobalContext,
			 g_uiJSCapslockCallback,
			 NULL,
			 1, &jsCapslockStatus,
			 NULL);
}




#define MAX_ENTRY_LENGTH 255


static JSClassRef g_jolicloudClass;


static char* js_value_to_string(JSContextRef jsContext,
				const JSValueRef value,
				size_t maxSize)
{
  JSStringRef stringRef = NULL;
  char* dest = NULL;

  if (JSValueGetType(jsContext, value) != kJSTypeString)
    return NULL;

  stringRef = JSValueToStringCopy(jsContext, value, NULL);
  if (stringRef == NULL)
    return NULL;

  dest = g_malloc(sizeof(char) * (maxSize + 1));
  if (dest != NULL)
    {
      memset(dest, 0, sizeof(char) * (maxSize + 1));
      JSStringGetUTF8CString(stringRef, dest, maxSize);
    }

  JSStringRelease(stringRef);

  return dest;
}


static JSObjectRef js_value_to_function(JSContextRef jsContext,
					const JSValueRef value)
{
  JSObjectRef dest = NULL;

  if (JSValueGetType(jsContext, value) != kJSTypeObject)
    return dest;

  dest = JSValueToObject(jsContext, value, NULL);
  if (dest == NULL)
    return dest;

  if (JSObjectIsFunction(jsContext, dest))
    return dest;

  return NULL;
}


static gboolean _ui_async_call_sign_in(void* context)
{
  g_uiSignInCallback();

  if (g_uiUsername != NULL)
    {
      memset(g_uiUsername, 0, strlen(g_uiUsername));
      g_free(g_uiUsername);
      g_uiUsername = NULL;
    }

  if (g_uiPassword != NULL)
    {
      memset(g_uiPassword, 0, strlen(g_uiPassword));
      g_free(g_uiPassword);
      g_uiPassword = NULL;
    }

  return FALSE;
}


static JSValueRef
js_jolicloud_sign_in(JSContextRef jsContext,
		     JSObjectRef function,
		     JSObjectRef thisObject,
		     size_t argumentCount,
		     const JSValueRef arguments[],
		     JSValueRef* exception)
{
  char* username = NULL;
  char* password = NULL;
  JSObjectRef callback = NULL;

  if (argumentCount != 3)
    goto end;

  username = js_value_to_string(jsContext, arguments[0], MAX_ENTRY_LENGTH);
  password = js_value_to_string(jsContext, arguments[1], MAX_ENTRY_LENGTH);
  callback = js_value_to_function(jsContext, arguments[2]);

  if (username == NULL || password == NULL || callback == NULL)
    {
      if (username != NULL)
	{
	  memset(username, 0, strlen(username));
	  g_free(username);
	}
      if (password != NULL)
	{
	  memset(password, 0, strlen(password));
	  g_free(password);
	}

      goto end;
    }

  g_uiUsername = username;
  g_uiPassword = password;
  g_uiJSCallbackValue = arguments[2];
  g_uiJSCallback = callback;

  JSValueProtect(jsContext, arguments[2]);

  g_timeout_add_seconds(0, _ui_async_call_sign_in, NULL);

 end:
  return JSValueMakeNull(jsContext);
}


static JSValueRef
js_jolicloud_sign_in_as_guest(JSContextRef jsContext,
			      JSObjectRef function,
			      JSObjectRef thisObject,
			      size_t argumentCount,
			      const JSValueRef arguments[],
			      JSValueRef* exception)
{
  JSObjectRef callback = NULL;
  const char* guestLogin = NULL;

  if (argumentCount != 1)
    goto end;

  callback = js_value_to_function(jsContext, arguments[0]);
  guestLogin = config_guestmode_login_get();

  if (callback == NULL || guestLogin == NULL)
    goto end;

  g_uiUsername = g_strdup(guestLogin);
  g_uiPassword = g_strdup("");

  if (g_uiUsername == NULL || g_uiPassword == NULL)
    {
      if (g_uiUsername != NULL)
	g_free(g_uiUsername);
      if (g_uiPassword != NULL)
	g_free(g_uiPassword);
      goto end;
    }

  g_uiJSCallbackValue = arguments[0];
  g_uiJSCallback = callback;

  JSValueProtect(jsContext, arguments[0]);

  g_timeout_add_seconds(0, _ui_async_call_sign_in, NULL);

 end:
  return JSValueMakeNull(jsContext);
}


static JSValueRef
js_jolicloud_add_event_listener(JSContextRef jsContext,
				JSObjectRef function,
				JSObjectRef thisObject,
				size_t argumentCount,
				const JSValueRef arguments[],
				JSValueRef* exception)
{
  char* eventName = NULL;
  JSObjectRef callback = NULL;

  if (argumentCount != 3)
    goto end;

  eventName = js_value_to_string(jsContext, arguments[0], MAX_ENTRY_LENGTH);
  callback = js_value_to_function(jsContext, arguments[1]);

  if (eventName == NULL || callback == NULL || strcasecmp(eventName, "capslock"))
    {
      if (eventName != NULL)
	g_free(eventName);
      goto end;
    }

  g_uiJSCapslockCallbackValue = arguments[1];
  g_uiJSCapslockCallback = callback;

  _ui_keymap_state_changed(gdk_keymap_get_for_display(gdk_display_get_default()), NULL);

  JSValueProtect(jsContext, arguments[1]);

  g_free(eventName);

 end:
  return JSValueMakeNull(jsContext);
}


static JSValueRef js_jolicloud_guestmode_get(JSContextRef jsContext,
					     JSObjectRef thisObject,
					     JSStringRef propertyName,
					     JSValueRef* exception)
{
  gboolean guestmodeEnabled;

  if (config_guestmode_enabled()
      && config_guestmode_logincmd_get() != NULL
      && config_guestmode_group_get() != NULL)
    {
      guestmodeEnabled = TRUE;
    }
  else
    {
      guestmodeEnabled = FALSE;
    }

  return JSValueMakeBoolean(jsContext, (bool)guestmodeEnabled);
}


static const JSStaticValue jolicloudValues[] =
  {
    { "guestmode", js_jolicloud_guestmode_get, NULL, kJSPropertyAttributeReadOnly },
    { NULL, NULL, NULL, 0 }
  };

static const JSStaticFunction jolicloudFunctions[] =
  {
    { "signIn", js_jolicloud_sign_in, kJSPropertyAttributeReadOnly },
    { "signInAsGuest", js_jolicloud_sign_in_as_guest, kJSPropertyAttributeReadOnly },
    { "addEventListener", js_jolicloud_add_event_listener, kJSPropertyAttributeReadOnly },
    { NULL, NULL, 0 }
  };

static const JSClassDefinition jolicloudDefinition =
  {
    0,
    kJSClassAttributeNone,
    "JolicloudClass",
    NULL,
    jolicloudValues,
    jolicloudFunctions,
  };


static void _uibind_objects(WebKitWebView* webkitWebView,
			    WebKitWebFrame* webkitWebFrame,
			    JSGlobalContextRef context,
			    JSObjectRef windowObject,
			    void* data)
{
  JSObjectRef jolicloudObject;

  g_uiJSGlobalContext = context;

  g_jolicloudClass = JSClassCreate(&jolicloudDefinition);
  jolicloudObject = JSObjectMake(context, g_jolicloudClass, NULL);
  JSObjectSetProperty(context,
		      JSContextGetGlobalObject(context),
		      JSStringCreateWithUTF8CString("jolicloud"),
		      jolicloudObject, kJSPropertyAttributeNone, NULL);
}
