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

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include "nvme-device.h"
#include "switchtec-nvme-device.h"

#include <switchtec/switchtec.h>
#include <switchtec/fabric.h>
#include <switchtec/mrpc.h>
#include <switchtec/errors.h>

#define INVALID_MRPC_CMD (SWITCHTEC_ERRNO_MRPC_FLAG_BIT | ERR_CMD_INVALID)

struct switchtec_device_manage_nvme_req
{
	struct switchtec_device_manage_req_hdr hdr;
	uint32_t nvme_sqe[16];
	uint8_t nvme_data[MRPC_MAX_DATA_LEN -
		sizeof(struct switchtec_device_manage_req_hdr) -
		(16 * 4)];
};

struct switchtec_device_manage_nvme_rsp
{
	struct switchtec_device_manage_rsp_hdr hdr;
	uint32_t nvme_cqe[4];
	uint8_t nvme_data[SWITCHTEC_DEVICE_MANAGE_MAX_RESP - (4 * 4)];
};

struct switchtec_admin_passthru_nvme_req {
	uint32_t nvme_sqe[16];
	uint8_t nvme_data[SWITCHTEC_NVME_ADMIN_PASSTHRU_MAX_DATA_LEN -
				(16 * 4)];
};

struct switchtec_admin_passthru_nvme_rsp {
	uint32_t nvme_cqe[4];
	uint8_t nvme_data[4096];
};

int pax_nvme_submit_admin_passthru_1k_rsp(int fd,
					  struct nvme_passthru_cmd *cmd)
{
	int ret;

	struct pax_nvme_device *pax;
	struct switchtec_device_manage_nvme_req req;
	struct switchtec_device_manage_nvme_rsp rsp;
	size_t data_len = 0;
	int status;

	memset(&req, 0, sizeof(req));
	memset(&rsp, 0, sizeof(rsp));
	pax = to_pax_nvme_device(global_device);

	req.hdr.pdfid = htole16(pax->pdfid);
	memcpy(&req.nvme_sqe, cmd, 16 * 4);

	/* sqe[9] should be 0 per spec */
	req.nvme_sqe[9] = 0;

	if (cmd->opcode & 0x1) {
		if (cmd->data_len > sizeof(req.nvme_data))
			data_len = sizeof(req.nvme_data);
		else
			data_len = cmd->data_len;

		memcpy(req.nvme_data, (void *)cmd->addr, data_len);
	}

	req.hdr.expected_rsp_len = (sizeof(rsp.nvme_cqe) + sizeof(rsp.nvme_data));

	ret = switchtec_device_manage(pax->dev,
				     (struct switchtec_device_manage_req *)&req,
				     (struct switchtec_device_manage_rsp *)&rsp);
	if (ret)
		return ret;

	status = (rsp.nvme_cqe[3] & 0xfffe0000) >> 17;
	if (!status) {
		cmd->result = rsp.nvme_cqe[0];
		if (cmd->addr) {
			memcpy((uint64_t *)cmd->addr,
				rsp.nvme_data,
				rsp.hdr.rsp_len - 4 * 4);
		}
	}

	return status;
}

int pax_nvme_submit_admin_passthru_4k_rsp(int fd,
					  struct nvme_passthru_cmd *cmd)
{
	int ret;

	struct pax_nvme_device *pax;
	struct switchtec_admin_passthru_nvme_req req;
	struct switchtec_admin_passthru_nvme_rsp rsp;
	size_t data_len = 0;
	size_t rsp_len;
	int status;

	memset(&req, 0, sizeof(req));
	memset(&rsp, 0, sizeof(rsp));
	pax = to_pax_nvme_device(global_device);

	memcpy(&req.nvme_sqe, cmd, 16 * 4);

	/* sqe[9] should be 0 per spec */
	req.nvme_sqe[9] = 0;

	if (cmd->opcode & 0x1) {
		if (cmd->data_len > sizeof(req.nvme_data))
			data_len = sizeof(req.nvme_data);
		else
			data_len = cmd->data_len;

		memcpy(req.nvme_data, (void *)cmd->addr, data_len);
	}

	data_len = data_len + 16 * 4;

	rsp_len = (sizeof(rsp.nvme_cqe) + cmd->data_len);

	ret = switchtec_nvme_admin_passthru(pax->dev, pax->pdfid,
					    data_len, &req,
					    &rsp_len, &rsp);
	if (ret)
		return ret;

	status = (rsp.nvme_cqe[3] & 0xfffe0000) >> 17;
	if (!status) {
		cmd->result = rsp.nvme_cqe[0];
		if (cmd->addr) {
			memcpy((uint64_t *)cmd->addr,
				rsp.nvme_data,
				rsp_len - 4 * 4);
		}
	}

	return status;
}

int pax_nvme_submit_admin_passthru(int fd, struct nvme_passthru_cmd *cmd)
{
	int ret;

	ret = pax_nvme_submit_admin_passthru_4k_rsp(fd, cmd);
	if (ret && errno == INVALID_MRPC_CMD)
		ret = pax_nvme_submit_admin_passthru_1k_rsp(fd, cmd);

	return ret;
}

int pax_nvme_io(int fd, struct nvme_user_io *io)
{
	fprintf(stderr, "%s not implemented.\n", __FUNCTION__);
	return -1;
}

int pax_nvme_subsystem_reset(int fd)
{
	fprintf(stderr, "%s not implemented.\n", __FUNCTION__);
	return -1;
}

int pax_nvme_reset_controller(int fd)
{
	fprintf(stderr, "%s not implemented.\n", __FUNCTION__);
	return -1;
}

int pax_nvme_ns_rescan(int fd)
{
	fprintf(stderr, "%s not implemented.\n", __FUNCTION__);
	return -1;
}

int pax_nvme_submit_passthru(int fd, int ioctl_cmd, struct nvme_passthru_cmd *cmd)
{
	fprintf(stderr, "%s not implemented.\n", __FUNCTION__);
	return -1;
}

int pax_nvme_submit_io_passthru(int fd, struct nvme_passthru_cmd *cmd)
{
	fprintf(stderr, "%s not implemented.\n", __FUNCTION__);
	return -1;
}

int pax_is_blk(void)
{
	struct pax_nvme_device *pax;
	pax = to_pax_nvme_device(global_device);

	return pax->is_blk;
}

int pax_nvme_get_nsid(int fd)
{
	struct pax_nvme_device *pax;
	pax = to_pax_nvme_device(global_device);

	return pax->ns_id;
}

struct nvme_device_ops pax_ops = {
	.nvme_submit_admin_passthru = pax_nvme_submit_admin_passthru,
	.nvme_get_nsid = pax_nvme_get_nsid,
	.nvme_io = pax_nvme_io,
	.nvme_subsystem_reset = pax_nvme_subsystem_reset,
	.nvme_reset_controller = pax_nvme_reset_controller,
	.nvme_ns_rescan = pax_nvme_ns_rescan,
	.nvme_submit_passthru = pax_nvme_submit_passthru,
	.nvme_submit_io_passthru = pax_nvme_submit_io_passthru,
	.is_blk = pax_is_blk,
};

