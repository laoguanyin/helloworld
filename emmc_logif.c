/******************************************************************************
*    Copyright (c) 2009-2011 by Hisi.
*    All rights reserved.
* ***
*    Create by HiHi
*
******************************************************************************/

#include <common.h>
#include <watchdog.h>
#include <asm/errno.h>
#include <malloc.h>

#include <emmc_logif.h>

#ifdef CONFIG_CMD_MMC
/*****************************************************************************/

emmc_logic_t *emmc_logic_open(unsigned long long address, unsigned long long length)
{
	emmc_logic_t *emmc_logic;
	struct mmc *mmc;
	int err = 0;

	mmc = find_mmc_device(0);
	if (!mmc)
	{
		printf("no MCI available\n");
		return NULL;
	}

	err = mmc_init(mmc);
	if (err)
	{
		printf("no eMMC available\n");
		return NULL;
	}

	/* Reject open, which are not block aligned */
	if ((address & (mmc->write_bl_len - 1)) || (length & (mmc->write_bl_len - 1)))
	{
		printf("Attempt to open non block aligned, "
			"emmc blocksize: 0x%08x, address: 0x%08llx, length: 0x%08llx\n",
			mmc->write_bl_len, address, length);
		return NULL;
	}

	if ((address > mmc->capacity)
		|| (length > mmc->capacity)
		|| ((address + length) > mmc->capacity))
	{
		printf("Attempt to open outside the flash area, "
			"emmc chipsize: 0x%08llx, address: 0x%08llx, length: 0x%08llx\n",
			mmc->capacity, address, length);
		return NULL;
	}

	if ((emmc_logic = malloc(sizeof(emmc_logic_t))) == NULL)
	{
		printf("no many memory.\n");
		return NULL;
	}

	emmc_logic->mmc      = mmc;
	emmc_logic->address   = address;
	emmc_logic->length    = length;
	emmc_logic->blocksize = mmc->write_bl_len;

	return emmc_logic;
}
/*****************************************************************************/

void emmc_logic_close(emmc_logic_t *emmc_logic)
{
	free(emmc_logic);
}
/*****************************************************************************/

int emmc_logic_write
(
 emmc_logic_t *emmc_logic,
 unsigned long long offset,    /* should be alignment with emmc block size */
 unsigned int length,          /* should be alignment with emmc block size */
 unsigned char *buf
 )
{
	unsigned long blk, cnt, ret;

	/* Reject write, which are not block aligned */
	if ((offset & (emmc_logic->blocksize - 1)) || (length & (emmc_logic->blocksize - 1)))
	{
		printf("Attempt to write non block aligned data, "
			"emmc block size: 0x%08llx, offset: 0x%08llx, length: 0x%08x\n",
			emmc_logic->blocksize, offset, length);
		return -1;
	}

	if ((offset > emmc_logic->length)
		|| (length > emmc_logic->length)
		|| ((offset + length) > emmc_logic->length))
	{
		printf("Attempt to write outside the flash handle area, "
			"flash handle size: 0x%08llx, offset: 0x%08llx, "
			"length: 0x%08x\n",
			emmc_logic->length, offset, length);
		return -1;
	}

	blk = (emmc_logic->address + offset) / emmc_logic->blocksize;
	cnt = length / emmc_logic->blocksize;
	ret = emmc_logic->mmc->block_dev.block_write(0, blk, cnt, buf);

	return (ret == cnt) ? 0 : ret;
}
/*****************************************************************************/

int emmc_logic_read
(
 emmc_logic_t *emmc_logic,
 unsigned long long offset,    /* should be alignment with emmc block size */
 unsigned int length,          /* should be alignment with emmc block size */
 unsigned char *buf
)
{
	unsigned long blk, cnt, ret;

	/* Reject read, which are not block aligned */
	if ((offset & (emmc_logic->blocksize - 1)) || (length & (emmc_logic->blocksize - 1)))
	{
		printf("Attempt to write non block aligned data, "
			"emmc block size: 0x%08llx, offset: 0x%08llx, length: 0x%08x\n",
			emmc_logic->blocksize, offset, length);
		return -1;
	}

	if ((offset > emmc_logic->length)
		|| (length > emmc_logic->length)
		|| ((offset + length) > emmc_logic->length))
	{
		printf("Attempt to write outside the flash handle area, "
			"flash handle size: 0x%08llx, offset: 0x%08llx, "
			"length: 0x%08x\n",
			emmc_logic->length, offset, length);
		return -1;
	}

	blk = (emmc_logic->address + offset) / emmc_logic->blocksize;
	cnt = length / emmc_logic->blocksize;
	ret = emmc_logic->mmc->block_dev.block_read(0, blk, cnt, buf);

	return (ret == cnt) ? 0 : ret;
}
/*****************************************************************************/
#endif /* CONFIG_CMD_MMC */
