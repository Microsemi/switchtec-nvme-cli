#ifndef _NVME_DEVICE_H
#define _NVME_DEVICE_H

#include "nvme-ioctl.h"

struct nvme_device_ops {
	int (*nvme_submit_admin_passthru)(int fd, struct nvme_passthru_cmd *cmd);
	int (*nvme_get_nsid)(int fd);
	int (*nvme_io)(int fd, struct nvme_user_io *io);
	int (*nvme_subsystem_reset)(int fd);
	int (*nvme_reset_controller)(int fd);
	int (*nvme_ns_rescan)(int fd);
	int (*nvme_submit_passthru)(int fd, int ioctl_cmd, struct nvme_passthru_cmd *cmd);
	int (*nvme_submit_io_passthru)(int fd, struct nvme_passthru_cmd *cmd);
	int (*is_blk)(void);
};

#define NVME_DEVICE_TYPE_RC	0
#define NVME_DEVICE_TYPE_PAX	1

struct nvme_device {
	int type;
	struct nvme_device_ops *ops;
};

extern struct nvme_device *global_device;

#endif
