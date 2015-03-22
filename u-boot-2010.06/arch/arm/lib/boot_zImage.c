#include <common.h>
#include <command.h>
#include <image.h>
#include <u-boot/zlib.h>
#include <asm/byteorder.h>
#include <asm/arch-s3c24x0/s3c2410.h>
#include "asm/mach-types.h"

#define LINUX_KERNEL_OFFSET 0x8000
#define LINUX_PARAM_OFFSET 0x100
#define LINUX_PAGE_SIZE 0x00001000
#define LINUX_PAGE_SHIFT 12
#define LINUX_ZIMAGE_MAGIC 0x016f2818
#define DRAM_SIZE 0x04000000
extern int nand_read_ll(unsigned char*, unsigned long, int);
/*
*Disable IRQs
*/
#define local_irq_disable() \
	({ \
	unsigned long temp; \
	__asm__ __volatile__( \
	"mrs %0, cpsr @ local_irq_disable\n" \
	" orr %0, %0, #128\n" \
	" msr cpsr_c, %0" \
	: "=r" (temp) \
	: \
	: "memory", "cc"); \
	})
static inline void cpu_arm920_cache_clean_invalidate_all(void)
{
	__asm__(
	" mov r1, #0\n"
	" mov r1, #7 << 5\n" /* 8 segments */
	"1: orr r3, r1, #63 << 26\n" /* 64 entries */
	"2: mcr p15, 0, r3, c7, c14, 2\n" /* clean & invalidate D index */
	" subs r3, r3, #1 << 26\n"
	" bcs 2b\n" /* entries 64 to 0 */
	" subs r1, r1, #1 << 5\n"
	" bcs 1b\n" /* segments 7 to 0 */
	" mcr p15, 0, r1, c7, c5, 0\n" /* invalidate I cache */
	" mcr p15, 0, r1, c7, c10, 4\n" /* drain WB */
	);
}
void cache_clean_invalidate(void)
{
	cpu_arm920_cache_clean_invalidate_all();
}
static inline void cpu_arm920_tlb_invalidate_all(void)
{
	__asm__(
	"mov r0, #0\n"
	"mcr p15, 0, r0, c7, c10, 4\n" /* drain WB */
	"mcr p15, 0, r0, c8, c7, 0\n" /* invalidate I & D TLBs */
	);
}
void tlb_invalidate(void)
{
	cpu_arm920_tlb_invalidate_all();
}
void call_linux(long a0, long a1, long a2)
{
	local_irq_disable();
	cache_clean_invalidate();
	tlb_invalidate();
	__asm__(
	"mov r0, %0\n"
	"mov r1, %1\n"
	"mov r2, %2\n"
	"mov ip, #0\n"
	"mcr p15, 0, ip, c13, c0, 0\n" /* zero PID */
	"mcr p15, 0, ip, c7, c7, 0\n" /* invalidate I,D caches */
	"mcr p15, 0, ip, c7, c10, 4\n" /* drain write buffer */
	"mcr p15, 0, ip, c8, c7, 0\n" /* invalidate I,D TLBs */
	"mrc p15, 0, ip, c1, c0, 0\n" /* get control register */
	"bic ip, ip, #0x0001\n" /* disable MMU */
	"mcr p15, 0, ip, c1, c0, 0\n" /* write control register */
	"mov pc, r2\n"
	"nop\n"
	"nop\n"
	: /* no outpus */
	: "r" (a0), "r" (a1), "r" (a2)
	: "r0","r1","r2","ip"
	);
}
/*
* pram_base: base address of linux paramter
*/
static void setup_linux_param(ulong param_base)
{
	struct param_struct *params = (struct param_struct *)param_base;
	char *linux_cmd;
	// printf("Setup linux parameters at 0x%08lx\n", param_base);
	memset(params, 0, sizeof(struct param_struct));
	params->u1.s.page_size = LINUX_PAGE_SIZE;
	params->u1.s.nr_pages = (DRAM_SIZE >> LINUX_PAGE_SHIFT);
	/* set linux command line */
	linux_cmd = getenv ("bootargs");
	if (linux_cmd == NULL) {
		printf("Wrong magic: could not found linux command line\n");
	} else {
		memcpy(params->commandline, linux_cmd, strlen(linux_cmd) + 1);
		// printf("linux command line is: \"%s\"\n", linux_cmd);
	}
}
/*
* dst: destination address
* src: source
* size: size to copy
* mt: type of storage device
*/
static inline int copy_kernel_img(ulong dst, const char *src, size_t size)
{
	int ret = 0;
	ret = nand_read_ll((unsigned char *)dst,
	(unsigned long)src, (int)size);
	return ret;
}
int boot_zImage(ulong from, size_t size)
{
	int ret;
	ulong boot_mem_base; /* base address of bootable memory */
	ulong to;
	ulong mach_type;
	boot_mem_base = 0x30000000;
	/* copy kerne image */
	to = boot_mem_base + LINUX_KERNEL_OFFSET;
	printf("Copy linux kernel from 0x%08lx to 0x%08lx, size = 0x%08lx ... ",
	from, to, size);
	ret = copy_kernel_img(to, (char *)from, size);
	if (ret) {
		printf("failed\n");
		return -1;
	} else {
		printf("Copy Kernel to SDRAM done,");
	}
	if (*(ulong *)(to + 9*4) != LINUX_ZIMAGE_MAGIC) {
		printf("Warning: this binary is not compressed linux kernel image\n");
		printf("zImage magic = 0x%08lx\n", *(ulong *)(to + 9*4));
	} else {
		// printf("zImage magic = 0x%08lx\n", *(ulong *)(to + 9*4));
		;
	}
	/* Setup linux parameters and linux command line */
	setup_linux_param(boot_mem_base + LINUX_PARAM_OFFSET);
	/* Get machine type */
	mach_type = MACH_TYPE_MINI2440;
	// printf("MACH_TYPE = %d\n", mach_type);
	/* Go Go Go */
	printf("NOW, Booting Linux......\n");
	call_linux(0, mach_type, to);
	return 0;
}
int do_boot_zImage (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	boot_zImage(KERNEL_OFFSET,KERNEL_SIZE);
	return 0;
}
U_BOOT_CMD(
	boot_zImage, 3, 0, do_boot_zImage,
	"boot_zImage - boot Linux 's zImage\n",
	" - boot Linux 's zImage"
);
int boot_noos(ulong from, size_t size)
{
	int ret;
	ulong boot_mem_base; /* base address of bootable memory */
	ulong to;
	boot_mem_base = 0x30000000;
	/* copy kerne image */
	to = boot_mem_base;
	printf("Copy code from 0x%08lx to 0x%08lx, size = 0x%08lx ... ",
	from, to, size);
	ret = copy_kernel_img(to, (char *)from, size);
	if (ret) {
		printf("failed\n");
		return -1;
	} else {
		printf("Copy code to SDRAM done,");
	}
	/* Go Go Go */
	printf("NOW, Booting code......\n");
	run_command("go 0x30000000", 0);
	return 0;
}
int do_boot_noos (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	boot_noos(0x200000,0x200000);
	return 0;
}
U_BOOT_CMD(
	boot_noos, 3, 0, do_boot_noos,
	"boot_noos - boot User Program\n",
	" - boot User Program"
);

int download_kernel_img(ulong to){
	char cmd[64] = {0};
	char bootfile[] = "zImage";

	sprintf(cmd, "tftpboot 0x%x %s", to, bootfile);
	run_command(cmd, 0);

	return 0;
}

int do_net_boot(ulong from, size_t size)
{
	int ret;
	ulong boot_mem_base; /* base address of bootable memory */
	ulong to;
	ulong mach_type;
	boot_mem_base = 0x30000000;
	/* copy kerne image */
	to = boot_mem_base + LINUX_KERNEL_OFFSET;
	printf("Download linux kernel from network to 0x%08lx ... ", to);
	ret = download_kernel_img(to);
	if (ret) {
		printf("failed\n");
		return -1;
	} else {
		printf("Copy Kernel to SDRAM done,");
	}
	if (*(ulong *)(to + 9*4) != LINUX_ZIMAGE_MAGIC) {
		printf("Warning: this binary is not compressed linux kernel image\n");
		printf("zImage magic = 0x%08lx\n", *(ulong *)(to + 9*4));
	} else {
		// printf("zImage magic = 0x%08lx\n", *(ulong *)(to + 9*4));
		;
	}
	/* Setup linux parameters and linux command line */
	setup_linux_param(boot_mem_base + LINUX_PARAM_OFFSET);
	/* Get machine type */
	mach_type = MACH_TYPE_MINI2440;
	// printf("MACH_TYPE = %d\n", mach_type);
	/* Go Go Go */
	printf("NOW, Booting Linux......\n");
	call_linux(0, mach_type, to);
	return 0;
}
U_BOOT_CMD(
	image_net, 3, 0, do_net_boot,
	"image_net - boot kernel from network\n",
	" - boot kernel from network"
);
