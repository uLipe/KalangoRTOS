/* Stub for unit tests */
#ifndef UL_MICROKERNEL_H
#define UL_MICROKERNEL_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
	UL_PRIV_USER   = 0,
	UL_PRIV_DRIVER = 1,
	UL_PRIV_KERNEL = 2,
} ul_privilege_t;

typedef int32_t ul_tid_t;
typedef int32_t ul_ep_t;
typedef int32_t ul_notif_t;

#define UL_TID_INVALID  ((ul_tid_t)-1)
#define UL_OK		 0
#define UL_EINVAL	-1
#define UL_ENOMEM	-2
#define UL_EPERM	-3
#define UL_ENOSPC	-4
#define UL_EDEADLK	-5
#define UL_ESRCH	-6
#define UL_ETIMEOUT	-7

typedef struct {
	const char	*name;
	void		(*entry)(void *arg);
	void		*arg;
	uint8_t		 priority;
	size_t		 stack_size;
	ul_privilege_t	 privilege;
} ul_thread_attr_t;

#endif /* UL_MICROKERNEL_H */
