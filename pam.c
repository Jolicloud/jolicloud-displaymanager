#include <glib/gtypes.h>
#include "pam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static struct pam_conv g_pamConversation = { 0, };
static pam_handle_t* g_pamHandle = NULL;
static pam_callback g_pamCallback = NULL;
static int g_pamLastStatus = PAM_SUCCESS;


static int _pam_conversation(int numMessage,
			     const struct pam_message** messages,
			     struct pam_response** response,
			     void* context);



gboolean pam_init(const char* serviceName,
		  pam_callback callback)
{
  g_pamConversation.conv = _pam_conversation;
  g_pamConversation.appdata_ptr = NULL;

  if (callback == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: pam_init() called with null callback\n");
      return FALSE;
    }

  g_pamLastStatus = pam_start(serviceName, NULL, &g_pamConversation, &g_pamHandle);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Initialization of pam failed. Error %d\n",
	      g_pamLastStatus);
      return FALSE;
    }

  g_pamCallback = callback;

  return TRUE;
}


void pam_cleanup(void)
{
  g_pamLastStatus = pam_end(g_pamHandle, g_pamLastStatus);
  g_pamHandle = NULL;
  g_pamCallback = NULL;
}


int pam_last_status(void)
{
  return g_pamLastStatus;
}


gboolean pam_item_set(PAM_ITEM_TYPE itemType, const void* value)
{
  g_pamLastStatus = pam_set_item(g_pamHandle, itemType, value);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to define a pam item. ItemType %d. Error %d\n",
	      itemType, g_pamLastStatus);
      return FALSE;
    }

  return TRUE;
}


gboolean pam_item_get(PAM_ITEM_TYPE itemType, const void** value)
{
  g_pamLastStatus = pam_get_item(g_pamHandle, itemType, value);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to retrieve a pam item. ItemType %d. Error %d\n",
	      itemType, g_pamLastStatus);
      return FALSE;
    }

  return TRUE;
}


gboolean pam_auth(void)
{
  g_pamLastStatus = pam_authenticate(g_pamHandle, 0);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: pam authentication failed. Error %d\n",
	      g_pamLastStatus);
      return FALSE;
    }

  g_pamLastStatus = pam_acct_mgmt(g_pamHandle, PAM_SILENT);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: pam account managment failed. Error %d\n",
	      g_pamLastStatus);
      return FALSE;
    }

  return TRUE;
}


gboolean pam_session_open(void)
{
  g_pamLastStatus = pam_setcred(g_pamHandle, PAM_ESTABLISH_CRED);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to define pam credentials. Error %d\n",
	      g_pamLastStatus);
      return FALSE;
    }

  g_pamLastStatus = pam_open_session(g_pamHandle, 0);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to open session with pan. Error %d\n",
	      g_pamLastStatus);
      pam_setcred(g_pamHandle, PAM_DELETE_CRED);
      return FALSE;
    }

  return TRUE;
}


gboolean pam_session_close(void)
{
  g_pamLastStatus = pam_close_session(g_pamHandle, 0);

  g_pamLastStatus = pam_setcred(g_pamHandle, PAM_DELETE_CRED);
  if (g_pamLastStatus != PAM_SUCCESS)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to delete pam credentials. Error %d\n",
	      g_pamLastStatus);
      return FALSE;
    }

  return TRUE;
}



/* privates
 */

static int _pam_conversation(int numMessage,
			     const struct pam_message** messages,
			     struct pam_response** responses,
			     void* context)
{
  int ret = PAM_SUCCESS;
  int i;

  *responses = (struct pam_response *)malloc(numMessage * sizeof(struct pam_response));
  if (*responses == NULL)
    return PAM_SYSTEM_ERR;

  memset(*responses, 0, numMessage * sizeof(struct pam_response));

  for (i = 0; i < numMessage; ++i)
    {
      switch (messages[i]->msg_style)
        {
        case PAM_PROMPT_ECHO_ON:
	  g_pamCallback(PAM_CREDENTIAL_ITEM_USER, &responses[i]->resp);
	  break;

        case PAM_PROMPT_ECHO_OFF:
	  g_pamCallback(PAM_CREDENTIAL_ITEM_PASSWORD, &responses[i]->resp);
	  break;

        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
	  fprintf(stderr, "Jolicloud-DisplayManager: Pam message '%s'\n", messages[i]->msg);
	  break;
        }
    }

  return ret;
}
