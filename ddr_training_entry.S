#include <config.h>

.section .image,#alloc

#ifdef CONFIG_DDR_TRAINING_HI3716MV300
.globl hi3716mv300_ddr_training_data_start
hi3716mv300_ddr_training_data_start:
.incbin "../arch/arm/cpu/godbox/ddr_training/hi3716mv300/ddr_training.bin"
.globl hi3716mv300_ddr_training_data_end
hi3716mv300_ddr_training_data_end:
#endif /* CONFIG_DDR_TRAINING_HI3716MV300 */

