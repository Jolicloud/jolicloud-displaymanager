#include <glib.h>
#include <gio/gio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char* g_cpuModelName = NULL;


gboolean device_cpu_init(void)
{
  GFile* fin = NULL;
  GFileInputStream* input = NULL;
  GDataInputStream* dataInput = NULL;
  GError* error = NULL;
  char* buffer = NULL;
  gsize bufferSize;
  gboolean tokenFound = FALSE;

  if (g_cpuModelName != NULL)
    return TRUE;

  fin = g_file_new_for_path("/proc/cpuinfo");
  if (fin == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to initialize a GFile\n");
      return FALSE;
    }

  input = g_file_read(fin, NULL, &error);
  if (input == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Failed while open '/proc/cpuinfo' [%s]\n", error->message);
      goto onError;
    }

  dataInput = g_data_input_stream_new(G_INPUT_STREAM(input));
  if (dataInput == NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Unable to initialize a data stream\n");
      goto onError;
    }

  while (tokenFound == FALSE && (buffer = g_data_input_stream_read_line(dataInput, &bufferSize, NULL, &error)) != NULL)
    {
      if (*buffer)
	{
	  gchar** tokens = g_strsplit(buffer, ":", 0);

	  if (tokens != NULL)
	    {
	      if (tokens[0] != NULL && tokens[1] != NULL)
		{
		  gchar* key = g_strstrip(tokens[0]);
		  gchar* value = g_strstrip(tokens[1]);

		  if (!g_strcasecmp(key, "model name") && value != NULL)
		    {
		      g_cpuModelName = g_strdup(value);
		      tokenFound = TRUE;
		    }
		}

	      g_strfreev(tokens);
	    }
	}

      g_free(buffer);
    }

  if (error != NULL)
    {
      fprintf(stderr, "Jolicloud-DisplayManager: Failed to read '/proc/cpuinfo' [%s]\n", error->message);
      goto onError;
    }

  g_object_unref(dataInput);
  g_input_stream_close(G_INPUT_STREAM(input), NULL, NULL);
  g_object_unref(fin);

  return TRUE;

 onError:

  if (error != NULL)
    g_error_free(error);

  if (dataInput != NULL)
    g_object_unref(dataInput);

  if (input != NULL)
    g_input_stream_close(G_INPUT_STREAM(input), NULL, NULL);

  if (fin != NULL)
    g_object_unref(fin);

  return FALSE;
}


void device_cpu_cleanup(void)
{
  if (g_cpuModelName != NULL)
    {
      g_free(g_cpuModelName);
      g_cpuModelName = NULL;
    }
}


const char* device_cpu_model_name_get(void)
{
  return g_cpuModelName;
}



/* int main(int ac, char** av) */
/* { */
/*   g_type_init(); */

/*   device_cpu_init(); */

/*   printf("model name [%s]\n", device_cpu_model_name_get()); */

/*   device_cpu_cleanup(); */

/*   return 0; */
/* } */
