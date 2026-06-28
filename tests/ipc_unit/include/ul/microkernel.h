/* Stub for IPC unit tests */
#ifndef UL_MICROKERNEL_H
#define UL_MICROKERNEL_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
	UL_PRIV_USER   = 0,
	UL_PRIV_DRIVER = 1,
} ul_privilege_t;

typedef int32_t ul_tid_t;
typedef int32_t ul_ep_t;
typedef int32_t ul_notif_t;

#define UL_TID_INVALID   ((ul_tid_t)-1)
#define UL_EP_INVALID    ((ul_ep_t)-1)
#define UL_NOTIF_INVALID ((ul_notif_t)-1)

#define UL_OK       0
#define UL_EINVAL  (-1)
#define UL_ENOMEM  (-2)
#define UL_EPERM   (-3)
#define UL_ENOSPC  (-4)
#define UL_EDEADLK (-5)
#define UL_ESRCH   (-6)
#define UL_ETIMEOUT (-7)

#define UL_MSG_WORDS 6

typedef struct {
	uint32_t label;
	uint32_t words[UL_MSG_WORDS];
} ul_msg_t;

typedef struct {
	const char    *name;
	void         (*entry)(void *arg);
	void          *arg;
	uint8_t        priority;
	size_t         stack_size;
	ul_privilege_t privilege;
} ul_thread_attr_t;

/* syscall_abi structs needed by ep.c */
typedef struct {
	const ul_msg_t *reply;
	ul_msg_t       *next;
	ul_tid_t       *next_sender;
} ul_reply_recv_args_t;

typedef struct {
	ul_msg_t msg;
	ul_tid_t sender;
	uint32_t notif_bits;
	int      is_notif;
} ul_recv_or_notif_result_t;

#endif /* UL_MICROKERNEL_H */
