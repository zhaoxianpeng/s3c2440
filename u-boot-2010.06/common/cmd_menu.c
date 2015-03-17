#include <common.h>
#include <command.h>

static char awaitkey(unsigned long delay, int *error_p)
{
	int i;
	if(delay == -1)
	{
		while(1)
		{
			if(tstc()) //we got a key press
			return getc();
		}
	}
	else
	{
		for(i = 0; i < delay; i++)
		{
			if(tstc())  //we got a key press
			return getc();
			udelay(10 * 1000);
		}
	}
	if(error_p)
		*error_p = -1;
	return 0;
}

void main_menu_usage(void)
{
	printf("             ***********************************************\r\n");
	if(BootFlag == 1)
	{
		printf("             ******** S3C2440 NOR UBOOT DOWNLOAD MODE ********\r\n");
	}
	else
	{
		printf("             ******** S3C2440 NAND UBOOT DOWNLOAD MODE ********\r\n");
	}
	printf("             ***********************************************\r\n");
	printf("************************************************\r\n");
	printf("[1] Download U-Boot To Nand Flash\r\n");
	printf("[2] Download zImage Kernel Image To Nand Flash\r\n");
	printf("[3] Download yaffs2 Filesystem To Nand Flash\r\n");
	printf("[4] Format Nand Flash\r\n");
	printf("[5] Reboot\r\n");
	printf("[ESC] Quit Menu\r\n");
	printf("************************************************\r\n");
	if(BootFlag == 1)
	{
		printf("[o] Download U-Boot To Nor Flash\r\n");
	}
	printf("Enter your choice:");
}

void menu_shell(void)
{
	char c;
	char cmd_buf[200];
	while(1)
	{
		main_menu_usage();
		c = awaitkey(-1, NULL);
		printf("%c\n",c);
		switch(c)
		{
			case '1':
			{
				strcpy(cmd_buf,"usbslave 1 0x30000000; nand erase bios; nand write 0x30000000 bios");
				run_command(cmd_buf,0);
				break;
			}
			case '2':
			{
				strcpy(cmd_buf,"usbslave 1 0x30000000; nand erase kernel; nand write 0x30000000 kernel");
				run_command(cmd_buf,0);
				break;
			}
			case '3':
			{
				strcpy(cmd_buf,"usbslave 1 0x30000000; nand erase root; nand write.yaffs 0x30000000 root $(filesize)");
				run_command(cmd_buf,0);
				break;
			}
			case '4':
			{
				strcpy(cmd_buf,"nand erase");
				run_command(cmd_buf,0);
				break;
			}
			case '5':
			{
				strcpy(cmd_buf,"reset");
				run_command(cmd_buf,0);
				break;
			}
			case 0x1b:
			{
				return;
				break;
			}
			case 'o':
			{
				strcpy(cmd_buf,"usbslave 1 0x30000000; protect off all; erase 0 +0x100000;cp.b 0x30000000 0 0x100000");
				run_command(cmd_buf,0);
				break;
			}
		}
	}
}

int do_menu( cmd_tbl_t *cmdtp, /*bd_t *bd,*/ int flag, int argc, char *argv[] )
{
/*	printf("<NOT YET IMPLEMENTED>\n"); */
	menu_shell();
	return 0;
}

U_BOOT_CMD(
	menu,   1,      1,      do_menu,
	"Download Menu",
	""
);
