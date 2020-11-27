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

#include "nvme-models.h"
#include "switchtec-nvme-device.h"
#include "switchtec-nvme.h"

struct list_item {
	char                node[1024];
	struct nvme_id_ctrl ctrl;
	int                 nsid;
	struct nvme_id_ns   ns;
	unsigned            block;
};

struct nvme_device *global_device = NULL;

int is_blk(void);

int get_nvme_info(int fd, struct list_item *item, const char *node)
{
	int err;

	err = nvme_identify_ctrl(fd, &item->ctrl);
	if (err)
		return err;
	item->nsid = nvme_get_nsid(fd);
	if (item->nsid <= 0)
		return item->nsid;
	err = nvme_identify_ns(fd, item->nsid,
			       1, &item->ns);
	if (err)
		return err;
	strcpy(item->node, node);
	item->block = is_blk();
	return 0;
}

static void format(char *formatter, size_t fmt_sz, char *tofmt, size_t tofmtsz)
{

	fmt_sz = snprintf(formatter,fmt_sz, "%-*.*s",
		 (int)tofmtsz, (int)tofmtsz, tofmt);
	/* trim() the obnoxious trailing white lines */
	while (fmt_sz) {
		if (formatter[fmt_sz - 1] != ' ' && formatter[fmt_sz - 1] != '\0') {
			formatter[fmt_sz] = '\0';
			break;
		}
		fmt_sz--;
	}
}

void json_print_list_items(struct list_item *list_items, unsigned len)
{
	struct json_object *root;
	struct json_array *devices;
	struct json_object *device_attrs;
	char formatter[41] = { 0 };
	int index, i = 0;
	char *product;
	long long int lba;
	double nsze;
	double nuse;

	root = json_create_object();
	devices = json_create_array();
	for (i = 0; i < len; i++) {
		device_attrs = json_create_object();

		json_object_add_value_string(device_attrs,
					     "DevicePath",
					     list_items[i].node);

		format(formatter, sizeof(formatter),
			   list_items[i].ctrl.fr,
			   sizeof(list_items[i].ctrl.fr));

		json_object_add_value_string(device_attrs,
					     "Firmware",
					     formatter);

		if (sscanf(list_items[i].node, "/dev/nvme%d", &index) == 1)
			json_object_add_value_int(device_attrs,
						  "Index",
						  index);

		format(formatter, sizeof(formatter),
		       list_items[i].ctrl.mn,
		       sizeof(list_items[i].ctrl.mn));

		json_object_add_value_string(device_attrs,
					     "ModelNumber",
					     formatter);

		product = nvme_product_name(index);

		json_object_add_value_string(device_attrs,
					     "ProductName",
					     product);

		format(formatter, sizeof(formatter),
		       list_items[i].ctrl.sn,
		       sizeof(list_items[i].ctrl.sn));

		json_object_add_value_string(device_attrs,
					     "SerialNumber",
					     formatter);

		json_array_add_value_object(devices, device_attrs);

		lba = 1 << list_items[i].ns.lbaf[(list_items[i].ns.flbas & 0x0f)].ds;
		nsze = le64_to_cpu(list_items[i].ns.nsze) * lba;
		nuse = le64_to_cpu(list_items[i].ns.nuse) * lba;
		json_object_add_value_uint(device_attrs,
					  "UsedBytes",
					  nuse);
		json_object_add_value_uint(device_attrs,
					  "MaximiumLBA",
					  le64_to_cpu(list_items[i].ns.nsze));
		json_object_add_value_uint(device_attrs,
					  "PhysicalSize",
					  nsze);
		json_object_add_value_uint(device_attrs,
					  "SectorSize",
					  lba);

		free((void*)product);
	}
	if (i)
		json_object_add_value_array(root, "Devices", devices);
	json_print_object(root, NULL);
}

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
#define CAP_VF		0x0

static int pax_enum_ep(struct switchtec_gfms_db_ep_port_ep *ep,
	struct switchtec_gfms_db_ep_port_attached_device_function *functions,
	int max_functions)
{
	int n;
	int j;
	int index = 0;
	struct switchtec_gfms_db_ep_port_attached_device_function *function;

	n = ep->ep_hdr.function_number;
	for (j = 0; j < n; j++) {
		function = &ep->functions[j];
		if (function->sriov_cap_pf == CAP_PF &&
		    function->device_class == NVME_CLASS) {
			memcpy(&functions[index++], function,
			       sizeof(*function));

			if (index >= max_functions)
				return index;
		}
	}

	return index;
}

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
	struct switchtec_gfms_db_ep_port_ep *ep;

	ret = switchtec_fab_gfms_db_dump_pax_all(pax->dev, &pax_all);
	if (ret)
		return -1;

	index = 0;
	for (i = 0; i < pax_all.ep_port_all.ep_port_count; i++) {
		ep_port = &pax_all.ep_port_all.ep_ports[i];
		if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_EP) {
			ep = &ep_port->ep_ep;
			ret = pax_enum_ep(ep, functions + index,
					  max_functions - index);
			index += ret;
		} else if (ep_port->port_hdr.type ==
			   SWITCHTEC_GFMS_DB_TYPE_SWITCH) {
			n = ep_port->port_hdr.ep_count;

			for (j = 0; j < n; j++) {
				ep = ep_port->ep_switch.switch_eps + j;
				ret = pax_enum_ep(ep, functions + index,
						  max_functions - index);

				index += ret;
			}
		}
	}

	return index;
}

static int pax_build_nvme_pf_list(struct pax_nvme_device *pax,
	struct switchtec_gfms_db_ep_port_attached_device_function *functions,
	int function_n,
	struct list_item *list_items,
	char *path)
{
	int j;
	int k;
	int ret;
	int idx = 0;
	char node[300];
	__u32 ns_list[1024];

	for (j = 0; j < function_n; j++) {
		pax->pdfid = functions[j].pdfid;
		ret = switchtec_ep_tunnel_status(pax->dev, pax->pdfid,
						 &pax->channel_status);
		if (ret)
			switchtec_perror("Getting EP tunnel status");

		if (pax->channel_status == SWITCHTEC_EP_TUNNEL_DISABLED)
			switchtec_ep_tunnel_enable(pax->dev, pax->pdfid);

		ret = nvme_identify_ns_list(0, 0, 1, ns_list);
		if (!ret) {
			for (k = 0; k < 1024; k++)
				if (ns_list[k]) {
					sprintf(node, "0x%04hxn%d@%s",
						pax->pdfid, ns_list[k], path);
					pax->ns_id = ns_list[k];
					ret = get_nvme_info(0,
							    &list_items[idx++],
							    node);
				}
		} else if (ret > 0) {
			fprintf(stderr, "NVMe Status:%s(%x) NSID:%d\n",
					nvme_status_to_string(ret), ret, 0);
		} else {
			perror("id namespace list");
		}

		if (pax->channel_status == SWITCHTEC_EP_TUNNEL_DISABLED)
			switchtec_ep_tunnel_disable(pax->dev, pax->pdfid);
	}

	return idx;
}

static int pax_get_nvme_pf_list(struct pax_nvme_device *pax,
				struct list_item *list_items,
				char *path)
{
	int i;
	int ret;
	int count = 0;
	uint8_t r_type;
	struct switchtec_fab_topo_info topo_info;
	struct switchtec_gfms_db_fabric_general fg;
	struct switchtec_gfms_db_ep_port_attached_device_function funcs[1024];

	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		topo_info.port_info_list[i].phys_port_id = 0xff;

	ret = switchtec_set_pax_id(pax->dev, SWITCHTEC_PAX_ID_LOCAL);
	if (ret)
		return -1;

	ret = switchtec_topo_info_dump(pax->dev, &topo_info);
	if (ret)
		return -2;

	ret = switchtec_fab_gfms_db_dump_fabric_general(pax->dev, &fg);
	if (ret)
		return -3;

	for (i = 0; i < 16; i++) {
		if (topo_info.route_port[i] == 0xff && fg.hdr.pax_idx != i)
			continue;

		r_type = fg.body.pax_idx[i].reachable_type;

		if (fg.hdr.pax_idx == i ||
		    r_type == SWITCHTEC_GFMS_DB_REACH_UC ||
		    r_type == SWITCHTEC_GFMS_DB_REACH_BC) {
			ret = switchtec_set_pax_id(pax->dev, i);
			if (ret)
				continue;

			ret = pax_get_nvme_pf_functions(pax, funcs + count,
							1024 - count);
			if (ret <= 0)
				continue;

			ret = pax_build_nvme_pf_list(pax, funcs + count, ret,
						     list_items + count, path);
			count += ret;
		}
	}

	switchtec_set_pax_id(pax->dev, SWITCHTEC_PAX_ID_LOCAL);
	return count;
}

static int switchtec_pax_list(int argc, char **argv, struct command *command,
			      struct plugin *plugin)
{
	char path[264];
	struct list_item *list_items;
	unsigned int i;
	unsigned int n;
	unsigned int index;
	int fmt, ret;
	struct dirent **pax_devices;
	struct pax_nvme_device *pax;

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

	ret = argconfig_parse(argc, argv, desc, opts);
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

		ret = pax_get_nvme_pf_list(pax, list_items + index, path);
		if (ret > 0)
			index += ret;

		global_device = NULL;
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

#define NOT_FOUND	0
#define IS_PF		1
#define IS_VF		2
static int pax_check_function_pdfid(struct switchtec_gfms_db_ep_port_ep *ep,
				    uint16_t pdfid)
{
	int n;
	int j;
	int ret = NOT_FOUND;
	struct switchtec_gfms_db_ep_port_attached_device_function *function;

	n = ep->ep_hdr.function_number;
	for (j = 0; j < n; j++) {
		function = &ep->functions[j];
		if (function->device_class == NVME_CLASS &&
		    function->pdfid == pdfid) {
			if(function->sriov_cap_pf == CAP_PF)
				return IS_PF;
			else if (function->sriov_cap_pf == CAP_VF)
				return IS_VF;
		}
	}

	return ret;
}

static int pax_check_ep_pdfid(struct switchtec_dev *dev, uint16_t pdfid)
{
	int i, j;
	int ret = NOT_FOUND;
	int n;
	struct switchtec_gfms_db_pax_all pax_all;
	struct switchtec_gfms_db_ep_port *ep_port;
	struct switchtec_gfms_db_ep_port_ep *ep;

	ret = switchtec_fab_gfms_db_dump_pax_all(dev, &pax_all);
	if (ret)
		return NOT_FOUND;

	for (i = 0; i < pax_all.ep_port_all.ep_port_count; i++) {
		ep_port = &pax_all.ep_port_all.ep_ports[i];
		if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_EP) {
			ep = &ep_port->ep_ep;
			ret = pax_check_function_pdfid(ep, pdfid);
			if (ret != NOT_FOUND)
				return ret;
		} else if (ep_port->port_hdr.type ==
			   SWITCHTEC_GFMS_DB_TYPE_SWITCH) {
			n = ep_port->port_hdr.ep_count;

			for (j = 0; j < n; j++) {
				ep = ep_port->ep_switch.switch_eps + j;
				ret = pax_check_function_pdfid(ep, pdfid);
				if (ret != NOT_FOUND)
					return ret;
			}
		}
	}

	return NOT_FOUND;
}

static int pax_check_pdfid_type(struct switchtec_dev *dev, uint16_t pdfid)
{
	int i;
	int ret = NOT_FOUND;
	uint8_t r_type;
	struct switchtec_fab_topo_info topo_info;
	struct switchtec_gfms_db_fabric_general fg;

	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		topo_info.port_info_list[i].phys_port_id = 0xff;

	ret = switchtec_set_pax_id(dev, SWITCHTEC_PAX_ID_LOCAL);
	if (ret)
		return NOT_FOUND;

	ret = switchtec_topo_info_dump(dev, &topo_info);
	if (ret)
		return NOT_FOUND;

	ret = switchtec_fab_gfms_db_dump_fabric_general(dev, &fg);
	if (ret)
		return NOT_FOUND;

	for (i = 0; i < 16; i++) {
		if (topo_info.route_port[i] == 0xff && fg.hdr.pax_idx != i)
			continue;

		r_type = fg.body.pax_idx[i].reachable_type;

		if (fg.hdr.pax_idx == i ||
		    r_type == SWITCHTEC_GFMS_DB_REACH_UC) {
			ret = switchtec_set_pax_id(dev, i);
			if (ret)
				continue;

			ret = pax_check_ep_pdfid(dev, pdfid);
			if (ret != NOT_FOUND)
				break;
		}
	}

	switchtec_set_pax_id(dev, SWITCHTEC_PAX_ID_LOCAL);
	return ret;
}

#define PCIE_CAPS_OFFSET		0x34
#define PCIE_CAPS_OFFSET_MASK		0xfc
#define PCIE_CAP_ID			0x10
#define PCIE_DEVICE_CTRL_OFFSET		0x08
#define PCIE_DEVICE_RESET_FLAG		0x8000
#define PCIE_NEXT_CAP_OFFSET		0x01

int ask_if_sure(int always_yes)
{
	char buf[10];
	char *ret;

	if (always_yes)
		return 0;

	fprintf(stderr, "Do you want to continue? [y/N] ");
	fflush(stderr);
	ret = fgets(buf, sizeof(buf), stdin);

	if (!ret)
		goto abort;

	if (strcmp(buf, "y\n") == 0 || strcmp(buf, "Y\n") == 0)
		return 0;

abort:
	fprintf(stderr, "Abort.\n");
	errno = EINTR;
	return -errno;
}

static int switchtec_vf_reset(int argc, char **argv, struct command *command,
			      struct plugin *plugin)
{
	int ret = 0;
	uint8_t offset;
	uint16_t pdfid;
	uint8_t cap_id = 0;
	uint16_t flags;
	char device_str[64];
	char pdfid_str[64];
	struct switchtec_dev *dev;
	const char *desc = "Perform a Function Level Reset (FLR) on a Virtual Function";
	const char *force = "The \"I know what I'm doing\" flag, skip confirmation before sending command";
	struct config {
		int force;
	} cfg = {};

	const struct argconfig_commandline_options opts[] = {
		OPT_FLAG("force", 'f', &cfg.force, force),
		{NULL}
	};

	ret = argconfig_parse(argc, argv, desc, opts);
	if (ret < 0)
		return ret;

	if (optind >= argc) {
		fprintf(stderr,
			"vf-reset: DEVICE is required for this command!\n");
		return -1;
	}

	if (sscanf(argv[optind], "%2049[^@]@%s", pdfid_str, device_str) < 2) {
		fprintf(stderr, "vf-reset: invalid device %s\n", argv[optind]);
		fprintf(stderr,
			"\nNOTE: This command only supports devices in the form of 'PDFID@device',\n"
			"where 'device' is your local device file or MOE service handle.\n"
			"Example for local device: 0x3b01@/dev/switchtec0\n"
			"Example for MOE access: 0x3b01@192.168.1.10:0\n");
		fprintf(stderr,
			"\nFor RC device, use the reset command provided by your OS instead.\n"
			"Example: echo 1 > /sys/bus/pci/devices/${bdf_vf}/reset\n");
		return -1;
	}

	ret = sscanf(pdfid_str, "%hx", &pdfid);
	if (!ret) {
		fprintf(stderr, "vf-reset: invalid device %s\n", argv[optind]);
		return -1;
	}

	if (!cfg.force) {
		fprintf(stderr,
			"WARNING: This will reset the Virtual Function for %s\n\n",
			 argv[optind]);
	}
	ret = ask_if_sure(cfg.force);
	if (ret)
		return ret;

	dev = switchtec_open(device_str);
	if (!dev) {
		switchtec_perror(device_str);
		return -ENODEV;
	}

	ret = pax_check_pdfid_type(dev, pdfid);
	if (ret == IS_PF) {
		fprintf(stderr,
			"vf-reset error: the given device %s is a Physical Function\n",
			argv[optind]);
		goto close;
	} else if (ret == NOT_FOUND) {
		fprintf(stderr,
			"vf-reset error: cannot find function with the given device name: %s\n",
			argv[optind]);
		goto close;
	}

	ret = switchtec_set_pax_id(dev, SWITCHTEC_PAX_ID_LOCAL);
	if (ret) {
		switchtec_perror("vf-reset");
		goto close;
	}

	ret = switchtec_ep_csr_read8(dev, pdfid, PCIE_CAPS_OFFSET, &offset);
	if (ret) {
		switchtec_perror("vf-reset");
		goto close;
	}

	offset &= PCIE_CAPS_OFFSET_MASK;
	while (offset) {
		ret = switchtec_ep_csr_read8(dev, pdfid, offset, &cap_id);
		if (ret) {
			switchtec_perror("vf-reset");
			goto close;
		}

		if (cap_id == PCIE_CAP_ID)
			break;

		ret = switchtec_ep_csr_read8(dev, pdfid,
					     offset + PCIE_NEXT_CAP_OFFSET,
					     &offset);
		if (ret) {
			switchtec_perror("vf-reset");
			goto close;
		}
	}

	if(!offset) {
		fprintf(stderr,
			"vf-reset: cannot find PCIe Capability Structure for the given device: %s\n",
			argv[optind]);
		goto close;
	}

	offset += PCIE_DEVICE_CTRL_OFFSET;
	ret = switchtec_ep_csr_read16(dev, pdfid, offset, &flags);
	if (ret) {
		switchtec_perror("vf-reset");
		goto close;
	}

	flags |= PCIE_DEVICE_RESET_FLAG;
	ret = switchtec_ep_csr_write32(dev, pdfid, flags, offset);
	if (ret) {
		switchtec_perror("vf-reset");
		goto close;
	}
close:
	switchtec_close(dev);
	return ret;
}
