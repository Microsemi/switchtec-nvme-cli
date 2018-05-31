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
#include "nvme-device.h"
#include "switchtec-nvme-device.h"

#include <switchtec/switchtec.h>
#include <switchtec/mrpc.h>

struct fabiov_device_manage_nvme_req
{
	struct fabiov_device_manage_req_hdr hdr;
	uint32_t nvme_sqe[16];
	uint8_t nvme_data[MRPC_MAX_DATA_LEN -
		sizeof(struct fabiov_device_manage_req_hdr) -
		(16 * 4)];
};

struct fabiov_device_manage_nvme_rsp
{
	struct fabiov_device_manage_rsp_hdr hdr;
	uint32_t nvme_cqe[4];
	uint8_t nvme_data[MRPC_MAX_DATA_LEN -
		sizeof(struct fabiov_device_manage_rsp_hdr) -
		(4 * 4)];
};

int pax_nvme_submit_admin_passthru(int fd, struct nvme_passthru_cmd *cmd)
{
	int ret;

	struct pax_nvme_device *pax;
	struct fabiov_device_manage_nvme_req req;
	struct fabiov_device_manage_nvme_rsp rsp;
	int data_len;
	int status;
	bool write = nvme_is_write((struct nvme_command *)cmd);

	memset(&req, 0, sizeof(req));
	memset(&rsp, 0, sizeof(rsp));
	pax = to_pax_nvme_device(global_device);

	req.hdr.pdfid = htole16(pax->pdfid);
	memcpy(&req.nvme_sqe, cmd, 16 * 4);

	/* sqe[9] should be 0 per spec */
	req.nvme_sqe[9] = 0;

	if (cmd->data_len > sizeof(req.nvme_data))
		data_len = sizeof(req.nvme_data);
	else
		data_len = cmd->data_len;

	memcpy(req.nvme_data, (void *)cmd->addr, data_len);

//	req.hdr.req_len = htole16(((data_len + 3) & ~3) / 4 + sizeof(req.nvme_sqe));
	req.hdr.expected_rsp_len = (sizeof(rsp.nvme_cqe) + sizeof(rsp.nvme_data))/4;

	ret = switchtec_device_manage(pax->dev,
				     (struct fabiov_device_manage_req *)&req,
				     (struct fabiov_device_manage_rsp *)&rsp);

#if 0
	printf("req:\n");
	for (int i = 0; i < data_len/4; i++) {
		printf("0x%08x ", *((int *)&req.nvme_sqe + i));
	}
	printf("\n");
	printf("rsp:\n");
	for (int i = 0; i < rsp.hdr.rsp_len; i++) {
		printf("0x%08x ", *((int *)&rsp.nvme_cqe + i));
		if (i % 16 == 15)
			printf("\n");
	}
	printf("\n");
#endif

	if (ret) {
		switchtec_perror("device_manage_cmd");
		return ret;
	}

	status = (rsp.nvme_cqe[3] & 0xfffe0000) >> 17;
	if (!status) {
		cmd->result = rsp.nvme_cqe[0];
		if (!write) {
			memcpy((uint64_t *)cmd->addr,
				rsp.nvme_data,
				(rsp.hdr.rsp_len - 4) * 4);
		}
	}

	return status;
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

