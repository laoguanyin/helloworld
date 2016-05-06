/******************************************************************************
*    Copyright (c) 2009-2011 by Hisi.
*    All rights reserved.
* ***
*    Create By Czyong
******************************************************************************/

#include <common.h>
#include <watchdog.h>
#include <asm/errno.h>
#include <malloc.h>

#include <nand_logif.h>

#ifdef CONFIG_CMD_NAND

/*****************************************************************************/
/*
 * phyaddress    - NAND partition start address, from this address we count
 *                 bad block. The address should be alignment with block size.
 * logiclength   - The length without bad block. It should be alignment with
 *                 block size.
 *
 * return        - The length with bad block.
 */
static unsigned long long logic_to_phylength(nand_info_t *nand,
	unsigned long long phyaddress, unsigned long long logiclength)
{
	unsigned long long len_incl_bad = 0;
	unsigned long long len_excl_bad = 0;

	while (len_excl_bad < logiclength) {
		if (!nand_block_isbad(nand, phyaddress))
			len_excl_bad += nand->erasesize;

		len_incl_bad += nand->erasesize;
		phyaddress   += nand->erasesize;

		if (phyaddress >= nand->size) {
			printf("Out of nand flash range.\n");
			break;
		}
	}
	return len_incl_bad;
}
/*****************************************************************************/
/*
 * address   - NAND partition start address. It should be alignemnt with
 *             block size.
 * length    - NAND partition length. It should be alignment with block size
 *
 * return    - return a handle if success. read/write/earse use this handle.
 */
nand_logic_t *nand_logic_open(unsigned long long address,
	unsigned long long length)
{
	nand_info_t  *nand;
	nand_logic_t *nand_logic;

	/* the following commands operate on the current device */
	if (nand_curr_device < 0
		|| nand_curr_device >= CONFIG_SYS_MAX_NAND_DEVICE
		|| !nand_info[nand_curr_device].name) {
		printf("No devices available\n");
		return NULL;
	}
	nand = &nand_info[nand_curr_device];

	/* Reject open, which are not block aligned */
	if ((address & (nand->erasesize - 1))
		|| (length & (nand->erasesize - 1))) {
		printf("Attempt to open non block aligned, "
			"nand blocksize: 0x%08x, address: 0x%08llx,"
			" length: 0x%08llx\n",
			nand->erasesize, address, length);
		return NULL;
	}

	if ((address > nand->size)
		|| (length > nand->size)
		|| ((address + length) > nand->size)) {
		printf("Attempt to open outside the flash area, "
			"nand chipsize: 0x%08llx, address: 0x%08llx,"
			" length: 0x%08llx\n",
			nand->size, address, length);
		return NULL;
	}

	if ((nand_logic = malloc(sizeof(nand_logic_t))) == NULL) {
		printf("Out of memory.\n");
		return NULL;
	}

	nand_logic->nand      = nand;
	nand_logic->address   = address;
	nand_logic->length    = length;
	nand_logic->erasesize = nand->erasesize;

	return nand_logic;
}
/*****************************************************************************/

void nand_logic_close(nand_logic_t *nand_logic)
{
	free(nand_logic);
}
/*****************************************************************************/
/*
 * offset  - NAND erase logic start address. You don't case bad block.
 *           It should be alignment with NAND block size.
 * length  - NAND erase length. You don't case bad block.
 *           It should be alignment with NAND block size.
 *
 * return  - 0: success.
 *           other: fail.
 * NOTES:
 *        If erase fail, it will mark bad block.
 *        Do NOT modify the printf string, it may be used by pc tools.
 */
int nand_logic_erase(nand_logic_t *nand_logic, unsigned long long offset,
	unsigned long long length)
{
	struct erase_info erase;
	unsigned long long phylength;
	nand_info_t *nand = nand_logic->nand;

	if ((offset & (nand->erasesize - 1))
		|| (length & (nand->erasesize - 1))) {
		printf("Attempt to erase non block aligned, "
			"nand blocksize: 0x%08x, offset: 0x%08llx,"
			" length: 0x%08llx\n",
			nand->erasesize, offset, length);
		return -1;
	}

	phylength = logic_to_phylength(nand,
		nand_logic->address, (offset + length));
	/*
	 * If the erase real length (phylength) out of paratition,
	 * we only erase the paratition length. We are not check the phylength
	 * out of paratition length (nand_logic->length)
	 */
	if ((offset > nand_logic->length)
		|| (length > nand_logic->length)
		|| ((offset + length) > nand_logic->length))
	{
		printf("Attempt to erase out of the flash paratition, "
			"paratition size: 0x%08llx, offset: 0x%08llx, "
			"length: 0x%08llx, phylength:  0x%08llx\n",
			nand_logic->length, offset, length, phylength);
		return -1;
	}

	phylength = logic_to_phylength(nand, nand_logic->address, offset);

	memset(&erase, 0, sizeof(erase));
	erase.mtd  = nand;
	erase.len  = nand->erasesize;
	erase.addr = nand_logic->address + phylength;

	for (; length > 0; erase.addr += nand->erasesize) {
		WATCHDOG_RESET ();

		if (erase.addr >= (nand_logic->address + nand_logic->length))
			break;

		int ret = nand->block_isbad (nand, erase.addr);
		if (ret > 0) {
			printf("\rSkipping bad block at  "
				"0x%08llx                   "
				"                         \n",
				erase.addr);
			continue;
		} else if (ret < 0) {
			printf("\n%s: MTD get bad block at 0x%08llx"
				" failed: %d\n",
				nand->name, erase.addr, ret);
			return -1;
		}

		printf("\rErasing at 0x%08llx", erase.addr);
		if ((ret = nand->erase(nand, &erase)) != 0) {

			printf("\n%s: MTD Erase at 0x%08llx failure: %d, ",
				nand->name, erase.addr, ret);

			if (nand->block_markbad && (ret == -EIO)) {
				printf("Mark bad block.");
				ret = nand->block_markbad(nand, erase.addr);
				if (ret < 0) {
					printf("\n%s: MTD block_markbad at"
						" 0x%08llx failed: %d, aborting\n",
						nand->name, erase.addr, ret);
					return -1;
				}
			}
			printf("\n");
		}

		length -= nand->erasesize;
	}
	printf("\n");
	return 0;
}
/*****************************************************************************/
/*
 * offset  - NAND write logic start address. You don't case bad block.
 *           It should be alignment with NAND page size.
 * length  - NAND write logic length. You don't case bad block.
 *           It should be alignment with NAND page size.
 * buf     - data buffer.
 *           Notes: if withoob = 1, the buf length > length, it include oob
 *                  length. withoob = 0, the buf length = length.
 * withoob - If write yaffs2 data, withoob=1, otherwise withoob = 0.
 *
 * return  - 0: success.
 *           other: fail.
 * NOTES:
 *    There is a restrict in uboot origin code, read/write should be
 *    alignment with block size, not page size.
 *    If you read/write data meet a bad block, such as: read the the 4 page
 *    of a bad block, it will skip to the next good block start address(the
 *    0 page of a block), you real want the 4 page, not 0 page.
 *    After this function (logic_to_phylength), it was impossible of the
 *    read/write first block is bad block, so we can't meet this uboot
 *    restrict.
 *
 */
int nand_logic_write(nand_logic_t *nand_logic, unsigned long long offset,
	unsigned int length, unsigned char *buf, int withoob)
{
	unsigned long long phylength;
	unsigned long long phyaddress;
	nand_info_t *nand = nand_logic->nand;

	/* Reject write, which are not page aligned */
	if ((offset & (nand->writesize - 1))
		|| (length & (nand->writesize - 1))) {
		printf("Attempt to write non page aligned data, "
			"nand page size: 0x%08x, offset:"
			" 0x%08llx, length: 0x%08x\n",
			nand->writesize, offset, length);
		return -1;
	}

	/*
	 * There is a restrict in uboot origin code, read/write should be
	 * alignment with block size, not page size.
	 * If you read/write data meet a bad block, such as: read the the 4 page
	 * of a bad block, it will skip to the next good block start address(the
	 * 0 page of a block), you real want the 4 page, not 0 page.
	 * After this function (logic_to_phylength), it was impossible of the
	 * read/write first block is bad block, so we can't meet this uboot
	 * restrict.
	 */
	phylength = logic_to_phylength(nand, nand_logic->address,
		(offset + length + nand->erasesize - 1)
			& (~(nand_logic->erasesize - 1)));
	if ((offset > nand_logic->length)
		|| (length > nand_logic->length)
		|| (phylength > nand_logic->length)) {
		printf("Attempt to write outside the flash handle area, "
			"flash handle size: 0x%08llx, offset: 0x%08llx, "
			"length: 0x%08x, phylength:  0x%08llx\n",
			nand_logic->length, offset, length, phylength);
		return -1;
	}

	phylength = logic_to_phylength(nand, nand_logic->address,
		(offset + nand->erasesize - 1) & (~(nand_logic->erasesize - 1)));
	if (offset & (nand_logic->erasesize - 1)) {
		phyaddress = phylength - nand->erasesize
			+ (offset & (nand_logic->erasesize - 1))
			+ nand_logic->address;
	} else {
		phyaddress = phylength + nand_logic->address;
	}

	if (withoob) {
		length = length / nand->writesize
			* (nand->writesize + nand->oobsize);
		return nand_write_yaffs_skip_bad(nand_logic->nand,
			phyaddress, &length, buf);
	} else {
		return nand_write_skip_bad(nand_logic->nand,
			phyaddress, &length, buf);
	}
}
/*****************************************************************************/
/*
 * offset  - NAND read logic start address. You don't case bad block.
 *           It should be alignment with NAND page size.
 * length  - NAND read logic length. You don't case bad block.
 *           It should be alignment with NAND page size.
 * buf     - data buffer.
 *           Notes: if withoob = 1, the buf length > length, it include oob
 *                  length. withoob = 0, the buf length = length.
 * withoob - If read yaffs2 data, withoob=1, otherwise withoob = 0.
 *
 * return  - 0: success.
 *           other: fail.
 * NOTES:
 *    There is a restrict in uboot origin code, read/write should be
 *    alignment with block size, not page size.
 *    If you read/write data meet a bad block, such as: read the the 4 page
 *    of a bad block, it will skip to the next good block start address(the
 *    0 page of a block), you real want the 4 page, not 0 page.
 *    After this function (logic_to_phylength), it was impossible of the
 *    read/write first block is bad block, so we can't meet this uboot
 *    restrict.
 *
 */
int nand_logic_read(nand_logic_t *nand_logic, unsigned long long offset,
	unsigned int length, unsigned char *buf, int withoob)
{
	unsigned long long phylength;
	unsigned long long phyaddress;
	nand_info_t *nand = nand_logic->nand;

	/* Reject read, which are not page aligned */
	if ((offset & (nand->writesize - 1))
		|| (length & (nand->writesize - 1))) {
		printf("Attempt to read non page aligned data, "
			"nand page size: 0x%08x, offset:"
			" 0x%08llx, length: 0x%08x\n",
			nand->writesize, offset, length);
		return -1;
	}

	/*
	 * There is a restrict in uboot origin code, read/write should be
	 * alignment with block size, not page size.
	 * If you read/write data meet a bad block, such as: read the the 4 page
	 * of a bad block, it will skip to the next good block start address(the
	 * 0 page of a block), you real want the 4 page, not 0 page.
	 * After this function (logic_to_phylength), it was impossible of the
	 * read/write first block is bad block, so we can't meet this uboot
	 * restrict.
	 */
	phylength = logic_to_phylength(nand, nand_logic->address,
		(offset + length + nand->erasesize - 1)
			& (~(nand_logic->erasesize - 1)));
	if ((offset > nand_logic->length)
		|| (length > nand_logic->length)
		|| (phylength > nand_logic->length)) {
		printf("Attempt to read outside the flash handle area, "
			"flash handle size: 0x%08llx, offset: 0x%08llx, "
			"length: 0x%08x, phylength:  0x%08llx\n",
			nand_logic->length, offset, length, phylength);
		return -1;
	}

	phylength = logic_to_phylength(nand, nand_logic->address,
		(offset + nand->erasesize - 1) & (~(nand_logic->erasesize - 1)));
	if (offset & (nand_logic->erasesize - 1))
		phyaddress = phylength - nand->erasesize +
		(offset & (nand_logic->erasesize - 1)) + nand_logic->address;
	else
		phyaddress = phylength + nand_logic->address;

	if (withoob) {
		unsigned long long block_offset;
		unsigned long long read_length;

		while (length > 0) {
			block_offset = phyaddress & (nand->erasesize - 1);

			WATCHDOG_RESET ();

			if (nand_block_isbad (nand, phyaddress
				& ~(nand_logic->erasesize - 1))) {
				printf("Skipping bad block 0x%08llx\n",
					phyaddress & ~(nand_logic->erasesize - 1));
				phyaddress += nand->erasesize - block_offset;
				continue;
			}

			if (length < (nand->erasesize - block_offset))
				read_length = length;
			else
				read_length = nand->erasesize - block_offset;

			while (read_length > 0) {
				int ret;
				struct mtd_oob_ops ops;

				memset(&ops, 0, sizeof(ops));
				ops.datbuf = buf;
				ops.oobbuf = buf + nand->writesize;
				ops.len = nand->writesize;
				ops.ooblen = nand->oobsize;
				ops.mode = MTD_OOB_RAW;

				ret = nand->read_oob(nand, phyaddress, &ops);
				if (ret < 0) {
					printf("Error (%d) reading page"
						" 0x%08llx\n",
						ret, phyaddress);
					return -1;
				}
				phyaddress  += nand->writesize;
				read_length -= nand->writesize;
				length      -= nand->writesize;
				buf += nand->writesize + nand->oobsize;
			}
		}
		return 0;
	} else {
		return nand_read_skip_bad(nand, phyaddress, &length, buf);
	}
}
/*****************************************************************************/

#endif /* CONFIG_CMD_NAND */
