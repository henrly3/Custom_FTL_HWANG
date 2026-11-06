// SPDX-License-Identifier: GPL-2.0
/*
 * myblk: a minimal RAM-backed block device driver (blk-mq).
 *
 * Step 1 of the FTL project: the "disk" is just a vmalloc'ed buffer.
 * Every read/write goes through myblk_transfer(); in step 2 this is
 * where the FTL (logical -> physical page mapping, GC, ...) plugs in.
 */
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/vmalloc.h>
#include <linux/version.h>

#define MYBLK_NAME        "myblk"
#define MYBLK_SECTOR_SIZE 4096                 /* logical block size */

static unsigned long capacity_mb = 64;
module_param(capacity_mb, ulong, 0444);
MODULE_PARM_DESC(capacity_mb, "Disk size in MiB (default 64)");

struct myblk_dev {
	int major;
	u8 *data;                    /* backing storage ("NAND" later) */
	size_t size;                 /* bytes */
	struct blk_mq_tag_set tag_set;
	struct gendisk *disk;
};

static struct myblk_dev *dev;

/* Copy one segment. FTL hook: translate 'pos' here in step 2. */
static blk_status_t myblk_transfer(struct myblk_dev *d, loff_t pos,
				   void *buf, unsigned int len, bool is_write)
{
	if (pos + len > d->size)
		return BLK_STS_IOERR;

	if (is_write)
		memcpy(d->data + pos, buf, len);
	else
		memcpy(buf, d->data + pos, len);
	return BLK_STS_OK;
}

static blk_status_t myblk_queue_rq(struct blk_mq_hw_ctx *hctx,
				   const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	struct myblk_dev *d = rq->q->queuedata;
	loff_t pos = (loff_t)blk_rq_pos(rq) << SECTOR_SHIFT;
	blk_status_t status = BLK_STS_OK;
	struct req_iterator iter;
	struct bio_vec bv;
	bool is_write;

	blk_mq_start_request(rq);

	switch (req_op(rq)) {
	case REQ_OP_READ:
		is_write = false;
		break;
	case REQ_OP_WRITE:
		is_write = true;
		break;
	default:
		status = BLK_STS_NOTSUPP;
		goto out;
	}

	rq_for_each_segment(bv, rq, iter) {
		void *buf = bvec_kmap_local(&bv);

		status = myblk_transfer(d, pos, buf, bv.bv_len, is_write);
		kunmap_local(buf);
		if (status != BLK_STS_OK)
			break;
		pos += bv.bv_len;
	}
out:
	blk_mq_end_request(rq, status);
	return BLK_STS_OK;
}

static const struct blk_mq_ops myblk_mq_ops = {
	.queue_rq = myblk_queue_rq,
};

static const struct block_device_operations myblk_fops = {
	.owner = THIS_MODULE,
};

static int __init myblk_init(void)
{
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->size = capacity_mb * 1024 * 1024;
	dev->data = vzalloc(dev->size);
	if (!dev->data) {
		ret = -ENOMEM;
		goto free_dev;
	}

	dev->major = register_blkdev(0, MYBLK_NAME);
	if (dev->major < 0) {
		ret = dev->major;
		goto free_data;
	}

	dev->tag_set.ops = &myblk_mq_ops;
	dev->tag_set.nr_hw_queues = 1;
	dev->tag_set.queue_depth = 128;
	dev->tag_set.numa_node = NUMA_NO_NODE;
	ret = blk_mq_alloc_tag_set(&dev->tag_set);
	if (ret)
		goto unregister;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)
	{
		struct queue_limits lim = {
			.logical_block_size  = MYBLK_SECTOR_SIZE,
			.physical_block_size = MYBLK_SECTOR_SIZE,
		};
		dev->disk = blk_mq_alloc_disk(&dev->tag_set, &lim, dev);
	}
#else
	dev->disk = blk_mq_alloc_disk(&dev->tag_set, dev);
#endif
	if (IS_ERR(dev->disk)) {
		ret = PTR_ERR(dev->disk);
		goto free_tags;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 9, 0)
	blk_queue_logical_block_size(dev->disk->queue, MYBLK_SECTOR_SIZE);
	blk_queue_physical_block_size(dev->disk->queue, MYBLK_SECTOR_SIZE);
#endif

	dev->disk->major = dev->major;
	dev->disk->first_minor = 0;
	dev->disk->minors = 1;
	dev->disk->fops = &myblk_fops;
	dev->disk->private_data = dev;
	snprintf(dev->disk->disk_name, DISK_NAME_LEN, MYBLK_NAME "0");
	set_capacity(dev->disk, dev->size >> SECTOR_SHIFT);  /* in 512B sectors */

	ret = add_disk(dev->disk);
	if (ret)
		goto put_disk;

	pr_info(MYBLK_NAME ": /dev/%s ready, %lu MiB\n",
		dev->disk->disk_name, capacity_mb);
	return 0;

put_disk:
	put_disk(dev->disk);
free_tags:
	blk_mq_free_tag_set(&dev->tag_set);
unregister:
	unregister_blkdev(dev->major, MYBLK_NAME);
free_data:
	vfree(dev->data);
free_dev:
	kfree(dev);
	return ret;
}

static void __exit myblk_exit(void)
{
	del_gendisk(dev->disk);
	put_disk(dev->disk);
	blk_mq_free_tag_set(&dev->tag_set);
	unregister_blkdev(dev->major, MYBLK_NAME);
	vfree(dev->data);
	kfree(dev);
	pr_info(MYBLK_NAME ": unloaded\n");
}

module_init(myblk_init);
module_exit(myblk_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal RAM-backed blk-mq block device (FTL project step 1)");
