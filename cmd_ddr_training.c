/******************************************************************************
*    Copyright (c) 2009-2012 by Hisi.
*    All rights reserved.
* ***
*    Create by Czyong. 2012-09-17
*
******************************************************************************/

#include <common.h>
#include <command.h>
#include <asm/io.h>

struct regval_t {
	unsigned int reg;
	unsigned int val;
};

struct ddrtr_result_t {
	unsigned int count;
#define DDR_TRAINING_MAX_VALUE       20
	struct regval_t reg[20];
	char data[512];
};

typedef struct ddrtr_result_t *(*ddrtr_t)(unsigned int start,
	unsigned int length);

#ifdef CONFIG_DDR_TRAINING_HI3716MV300
extern int  hi3716mv300_ddr_training_result(unsigned int TRAINING_ADDR);
extern char hi3716mv300_ddr_training_data_start[];
extern char hi3716mv300_ddr_training_data_end[];
#endif

#define DDR_TRAINING_ENV                       "ddrtr"

static struct ddrtr_result_t ddrtr_result;
extern int do_saveenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

/*****************************************************************************/

static int ddr_training_result(unsigned int TRAINING_ADDR)
{
	long long chipid = get_chipid();

 #ifdef CONFIG_DDR_TRAINING_HI3716MV300
	if (chipid == _HI3716M_V300)
		return hi3716mv300_ddr_training_result(TRAINING_ADDR);
#endif

	printf("DDR training is unsupport.\n");

	return -1;
}
/*****************************************************************************/

static void * get_ddrtr_entry(void)
{
	char *src_ptr = NULL;
	char *dst_ptr;
	unsigned int length = 0;
	long long chipid = get_chipid();

#ifdef CONFIG_DDR_TRAINING_HI3716MV300
	if (chipid == _HI3716M_V300) {
		src_ptr = hi3716mv300_ddr_training_data_start;
		dst_ptr = (char *)(0x0001000);
		length  = hi3716mv300_ddr_training_data_end - src_ptr;
	}
#endif

	if (!src_ptr || !length) {
		printf("DDR training is unsupport.\n");
		return NULL;
	}

	memcpy(dst_ptr, src_ptr, length);
	return (void *) dst_ptr;
}
/*****************************************************************************/

static char *dump_ddrtr_result(struct ddrtr_result_t *result, char flags)
{
	int ix;
	char *ptr;
	static char buf[DDR_TRAINING_MAX_VALUE * 22] = {0};

	ptr = buf;
	buf[0] = '\0';
	for (ix = 0; ix < result->count; ix++) {
		result->reg[ix].val = readl(result->reg[ix].reg);
		ptr += sprintf(ptr, "0x%08x=0x%08x%c",
			result->reg[ix].reg,
			result->reg[ix].val,
			flags);
	}
	return buf;
}
/*****************************************************************************/
#ifdef CONFIG_DDR_TRAINING_STARTUP

static int get_ddrtr_result_by_env(struct ddrtr_result_t *result)
{
	unsigned int ix;
	char *str = getenv(DDR_TRAINING_ENV);

	result->count = 0;
	if (!str)
		return -1;
	while (*str == ' ')
		str++;

	for (ix = 0; *str && ix < DDR_TRAINING_MAX_VALUE; ix++) {
		result->reg[ix].reg = simple_strtoul(str, &str, 16);
		if (!*str++)
			break;
		result->reg[ix].val = simple_strtoul(str, &str, 16);
		while (*str == ' ')
			str++;
	}
	result->count = ix;

	return ((result->count > 0) ? 0 : -1);
}
#endif /* CONFIG_DDR_TRAINING_STARTUP */
/*****************************************************************************/

int ddr_training(void)
{
	ddrtr_t entry;
	unsigned int start, end;
	struct ddrtr_result_t *result;

	start = get_ddr_free(&end, 0x100000);
	end += start;

	entry = (ddrtr_t)get_ddrtr_entry();
	if (!entry)
		return -1;

	printf ("## DDR training entry: 0x%08X, "
		"training area: 0x%08X - 0x%08X\n",
		(unsigned int)entry, start, end);

	asm("mcr p15, 0, r0, c7, c5, 0");
	asm("mcr p15, 0, r0, c7, c10, 4");

	result = entry(start, (end - start));
	if (!result) {
		printf("## DDR training fail, reset system.\n");
		reset_cpu(0);
		return 0;
	}
	memcpy((void*)&ddrtr_result, result, sizeof(ddrtr_result));

	printf ("## DDR training terminated.\n");

	ddr_training_result((unsigned int)(&(ddrtr_result.data)));

	printf("\nDDR Training Registers and Value:\n");
	printf(dump_ddrtr_result(result, '\n'));

	return 0;
}
/*****************************************************************************/

int check_ddr_training(void)
{
#ifdef CONFIG_DDR_TRAINING_STARTUP
	int ix;
	char *s = getenv("unddrtr");

	if (s && (*s == 'y' || *s == 'Y'))
		return 0;


	if (get_ddrtr_result_by_env(&ddrtr_result)) {
		/* ddr training function will set value to ddr register. */

		if (ddr_training())
			return 0;

		setenv(DDR_TRAINING_ENV,
			dump_ddrtr_result(&ddrtr_result, ' '));
		return do_saveenv(NULL, 0, 0, NULL);
	}

	printf("Set training value to DDR controller\n");
	for (ix = 0; ix < ddrtr_result.count; ix++) {
		writel(ddrtr_result.reg[ix].val,
			ddrtr_result.reg[ix].reg);
	}
#endif /* CONFIG_DDR_TRAINING_STARTUP */

	return 0;
}
/*****************************************************************************/

int do_ddr_training(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc < 2 || strcmp(argv[1], "training")) {
		cmd_usage(cmdtp);
		return 1;
	}

	if (ddr_training())
		return 0;

#ifdef CONFIG_DDR_TRAINING_STARTUP
	setenv(DDR_TRAINING_ENV, dump_ddrtr_result(&ddrtr_result, ' '));
#endif /* CONFIG_DDR_TRAINING_STARTUP */

	return 0;
}

U_BOOT_CMD(
	ddr, CONFIG_SYS_MAXARGS, 1,	do_ddr_training,
	"ddr training function",
	"training - do DDR training and display training result\n"
);
