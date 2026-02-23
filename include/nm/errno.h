#ifndef NM_ERRNO_H
#define NM_ERRNO_H

#include <stdint.h>

#define NM_OK 0

#define NM_EFAIL 1
#define NM_EINVAL 22
#define NM_ENOENT 2
#define NM_ENOMEM 12
#define NM_EBUSY 16
#define NM_ENOSYS 38

#define NM_ERR(code) (-(int64_t)(code))

#endif
