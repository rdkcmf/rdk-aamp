#include <pthread.h>

int pthread_join(pthread_t thread, void **retval)
{
	return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
	return 0;
}

int pthread_setname_np(pthread_t thread, const char *name)
{
	return 0;
}
