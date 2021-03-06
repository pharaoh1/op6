/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mailbox_client.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/mailbox/qmp.h>
#include <linux/uaccess.h>

#define MAX_MSG_SIZE 96 /* Imposed by the remote*/

static struct mbox_chan *chan;
static struct mbox_client *cl;

static ssize_t aop_msg_write(struct file *file, const char __user *userstr,
		size_t len, loff_t *pos)
{
	char buf[MAX_MSG_SIZE + 1] = {0};
	struct qmp_pkt pkt;
	int rc;

	if (!len || (len > MAX_MSG_SIZE))
		return len;

	rc  = copy_from_user(buf, userstr, len);
	if (rc) {
		pr_err("%s copy from user failed, rc=%d\n", __func__, rc);
		return len;
	}

	/*
	 * Controller expects a 4 byte aligned buffer
	 */
	pkt.size = (len + 0x3) & ~0x3;
	pkt.data = buf;

	if (mbox_send_message(chan, &pkt) < 0)
		pr_err("Failed to send qmp request\n");

	return len;
}

static const struct file_operations aop_msg_fops = {
	.write = aop_msg_write,
};

static int qmp_msg_probe(struct platform_device *pdev)
{
	struct dentry *file;

	cl = devm_kzalloc(&pdev->dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	cl->dev = &pdev->dev;
	cl->tx_block = true;
	cl->tx_tout = 100;
	cl->knows_txdone = false;

	chan = mbox_request_channel(cl, 0);
	if (IS_ERR(chan)) {
		dev_err(&pdev->dev, "Failed to mbox channel\n");
		return PTR_ERR(chan);
	}

	file = debugfs_create_file("aop_send_message", 0220, NULL, NULL,
			&aop_msg_fops);
	if (!file)
		goto err;
	return 0;
err:
	mbox_free_channel(chan);
	chan = NULL;
	return -ENOMEM;
}

static const struct of_device_id aop_qmp_match_tbl[] = {
	{.compatible = "qcom,debugfs-qmp-client"},
	{},
};

static struct platform_driver aop_qmp_msg_driver = {
	.probe = qmp_msg_probe,
	.driver = {
		.name = "debugfs-qmp-client",
		.owner = THIS_MODULE,
		.of_match_table = aop_qmp_match_tbl,
	},
};

builtin_platform_driver(aop_qmp_msg_driver);
