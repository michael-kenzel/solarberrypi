#ifndef INCLUDED_IT8951
#define INCLUDED_IT8951

#include <linux/types.h>
#include <linux/ioctl.h>

#define IT8951_IOC_MAGIC 0xEE

#define IT8951_IOCTL_RESET _IO(IT8951_IOC_MAGIC, 0x01)
#define IT8951_IOCTL_COMMAND _IOW(IT8951_IOC_MAGIC, 0x02, __u16)

#endif
