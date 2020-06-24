/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2018, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include "linux/nvme_ioctl.h"
#include "nvme-device.h"
#include "nvme-ioctl.h"

extern struct stat nvme_stat;

int rc_nvme_submit_admin_passthru(int fd, struct nvme_passthru_cmd *cmd)
{
	return ioctl(fd, NVME_IOCTL_ADMIN_CMD, cmd);
}

int rc_nvme_io(int fd, struct nvme_user_io *io)
{
	return ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io);
}

int rc_is_blk(void)
{
	return S_ISBLK(nvme_stat.st_mode);
}

int rc_nvme_get_nsid(int fd)
{
	return ioctl(fd, NVME_IOCTL_ID);
}

int rc_nvme_subsystem_reset(int fd)
{
	return ioctl(fd, NVME_IOCTL_SUBSYS_RESET);
}

int rc_nvme_reset_controller(int fd)
{
	return ioctl(fd, NVME_IOCTL_RESET);
}

int rc_nvme_ns_rescan(int fd)
{
	return ioctl(fd, NVME_IOCTL_RESCAN);
}

int rc_nvme_submit_passthru(int fd, int ioctl_cmd, struct nvme_passthru_cmd *cmd)
{
	return ioctl(fd, ioctl_cmd, cmd);
}

int rc_nvme_submit_io_passthru(int fd, struct nvme_passthru_cmd *cmd)
{
	return ioctl(fd, NVME_IOCTL_IO_CMD, cmd);
}

struct nvme_device_ops rc_ops = {
	.nvme_submit_admin_passthru = rc_nvme_submit_admin_passthru,
	.nvme_get_nsid = rc_nvme_get_nsid,
	.nvme_io = rc_nvme_io,
	.nvme_subsystem_reset = rc_nvme_subsystem_reset,
	.nvme_reset_controller = rc_nvme_reset_controller,
	.nvme_ns_rescan = rc_nvme_ns_rescan,
	.nvme_submit_passthru = rc_nvme_submit_passthru,
	.nvme_submit_io_passthru = rc_nvme_submit_io_passthru,
	.is_blk = rc_is_blk,
};

