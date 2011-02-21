#ifndef _JS_DEVICE_CPU_H_
# define _JS_DEVICE_CPU_H_

gboolean device_cpu_init(void);

void device_cpu_cleanup();


const char* device_cpu_model_name_get(void);


#endif /* _JS_DEVICE_CPU_H_ */
