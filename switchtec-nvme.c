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
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <switchtec/switchtec.h>
#include <switchtec/fabric.h>
#include <switchtec/mrpc.h>

#include "linux/nvme_ioctl.h"

#include "nvme.h"
#include "nvme-print.h"
#include "nvme-ioctl.h"
#include "plugin.h"
#include "json.h"

#include "argconfig.h"
#include "suffix.h"
#include <sys/ioctl.h>
#define CREATE_CMD

#include "switchtec-nvme-device.h"
#include "switchtec-nvme.h"

static const char *dev = "/dev/";

static void pax_show_list_item(struct list_item list_item)
{
	long long int lba = 1 << list_item.ns.lbaf[(list_item.ns.flbas & 0x0f)].ds;
	double nsze       = le64_to_cpu(list_item.ns.nsze) * lba;
	double nuse       = le64_to_cpu(list_item.ns.nuse) * lba;

	const char *s_suffix = suffix_si_get(&nsze);
	const char *u_suffix = suffix_si_get(&nuse);
	const char *l_suffix = suffix_binary_get(&lba);

	char usage[128];
	char format[128];

	sprintf(usage,"%6.2f %2sB / %6.2f %2sB", nuse, u_suffix,
		nsze, s_suffix);
	sprintf(format,"%3.0f %2sB + %2d B", (double)lba, l_suffix,
		list_item.ns.lbaf[(list_item.ns.flbas & 0x0f)].ms);
	printf("%-26s %-*.*s %-*.*s %-9d %-26s %-16s %-.*s\n", list_item.node,
            (int)sizeof(list_item.ctrl.sn), (int)sizeof(list_item.ctrl.sn), list_item.ctrl.sn,
            (int)sizeof(list_item.ctrl.mn), (int)sizeof(list_item.ctrl.mn), list_item.ctrl.mn,
            list_item.nsid, usage, format, (int)sizeof(list_item.ctrl.fr), list_item.ctrl.fr);
}

void pax_show_list_items(struct list_item *list_items, unsigned len)
{
	unsigned i;

	printf("%-26s %-20s %-40s %-9s %-26s %-16s %-8s\n",
	    "Node", "SN", "Model", "Namespace", "Usage", "Format", "FW Rev");
	printf("%-26s %-20s %-40s %-9s %-26s %-16s %-8s\n",
            "--------------------------", "--------------------", "----------------------------------------",
            "---------", "--------------------------", "----------------", "--------");
	for (i = 0 ; i < len ; i++)
		pax_show_list_item(list_items[i]);

}

static int scan_pax_dev_filter(const struct dirent *d)
{
	char path[264];
	struct stat bd;

	if (d->d_name[0] == '.')
		return 0;

	if (strstr(d->d_name, "switchtec")) {
		snprintf(path, sizeof(path), "%s%s", dev, d->d_name);
		if (stat(path, &bd))
			return 0;
		return 1;
	}
	return 0;
}

#define NVME_CLASS	0x010802
#define CAP_PF		0x3
static int pax_get_nvme_pf_functions(struct pax_nvme_device *pax,
				     struct switchtec_gfms_db_ep_port_attached_device_function *functions,
				     int max_functions)
{
	int i, j;
	int index;
	int ret;
	int n;
	struct switchtec_gfms_db_pax_all pax_all;
	struct switchtec_gfms_db_ep_port *ep_port;
	struct switchtec_gfms_db_ep_port_attached_device_function *function;

	ret = switchtec_gfms_db_dump_pax_all(pax->dev, &pax_all);
	if (ret) {
		switchtec_perror("switchtec_gfms_db_dump_pax_all");
		return ret;
	}

	index = 0;
	for (i = 0; i < pax_all.ep_port_all.ep_port_count; i++) {
		ep_port = &pax_all.ep_port_all.ep_ports[i];
		if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_EP) {
			n = ep_port->ep_ep.ep_hdr.function_number;
			for (j = 0; j < n; j++) {
				function = &ep_port->ep_ep.functions[j];
				if (function->sriov_cap_pf == CAP_PF
				    && function->device_class == NVME_CLASS)
					memcpy(&functions[index++],
						function,
						sizeof(*function));
			}
		}
	}

	return index;
}

static int switchtec_pax_list(int argc, char **argv, struct command *command,
			      struct plugin *plugin)
{
	char path[264];
	char node[300];
	struct list_item *list_items;
	unsigned int i, j, k, n, function_n;
	unsigned int index;
	int fd = 0;
	int fmt, ret;
	int err;
	struct dirent **pax_devices;
	struct pax_nvme_device *pax;
	__u32 ns_list[1024] = {0};
	struct switchtec_gfms_db_ep_port_attached_device_function functions[1024];

	const char *desc = "Retrieve basic information for the given Microsemi device";
	struct config {
		char *moe_service;
		char *output_format;
	};

	struct config cfg = {
		.moe_service = NULL,
		.output_format = "normal",
	};

	const struct argconfig_commandline_options opts[] = {
		{"MOE-service", 'r', "IP:INST", CFG_STRING, &cfg.moe_service, required_argument, "MOE service handle: IP:instance"},
		{"output-format", 'o', "FMT", CFG_STRING, &cfg.output_format, required_argument, "Output Format: normal|json"},
		{NULL}
	};

	ret = argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));
	if (ret < 0)
		return ret;

	fmt = validate_output_format(cfg.output_format);

	if (fmt != JSON && fmt != NORMAL)
		return -EINVAL;

	if (cfg.moe_service)
		n = 1;
	else {
		n = scandir(dev, &pax_devices, scan_pax_dev_filter, alphasort);
		if (n < 0) {
			fprintf(stderr, "no NVMe device(s) detected.\n");
			return n;
		}
	}

	list_items = calloc(n * 1024, sizeof(*list_items));
	if (!list_items) {
		fprintf(stderr, "can not allocate controller list payload\n");
		return ENOMEM;
	}

	index = 0;
	for (i = 0; i < n; i++) {
		if (cfg.moe_service)
			snprintf(path, sizeof(path), "%s", cfg.moe_service);
		else
			snprintf(path, sizeof(path), "%s%s", dev, pax_devices[i]->d_name);

		pax = malloc(sizeof(struct pax_nvme_device));
		pax->device.ops = &pax_ops;
		pax->dev = switchtec_open(path);
		if (!pax->dev) {
			switchtec_perror(path);
			free(pax);
			return -ENODEV;
		}
		global_device = &pax->device;

		function_n = pax_get_nvme_pf_functions(pax, functions, 1024);

		for (j = 0; j < function_n; j++) {
			pax->pdfid = functions[j].pdfid;
			memset(ns_list, 0, sizeof(ns_list));
			err = nvme_identify_ns_list(0, 0, 1, ns_list);
			if (!err) {
				for (k = 0; k < 1024; k++)
					if (ns_list[k]) {
						sprintf(node, "0x%04hxn%d@%s", pax->pdfid, ns_list[k], path);
						pax->ns_id = ns_list[k];
						ret = get_nvme_info(fd, &list_items[index++], node);
					}
			} else if (err > 0)
				fprintf(stderr, "NVMe Status:%s(%x) NSID:%d\n",
						nvme_status_to_string(err), err, 0);
			else
				perror("id namespace list");
		}
		switchtec_close(pax->dev);
		free(pax);
	}

	if (fmt == JSON)
		json_print_list_items(list_items, index);
	else
		pax_show_list_items(list_items, index);

	free(list_items);

	return 0;
}
