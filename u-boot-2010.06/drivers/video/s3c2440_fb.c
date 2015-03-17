#include <common.h>
 
#if defined(CONFIG_VIDEO_S3C2440)
 
#include <video_fb.h>
#include "videomodes.h"
#include <asm/arch/s3c24x0_cpu.h>
#include <asm/io.h>
/*
 * Export Graphic Device
 */
GraphicDevice smi;
 
#define VIDEO_MEM_SIZE  0x200000

#define MVAL			(0)
#define MVAL_USED 		(0)	  //0=each frame   1=rate by MVAL
#define INVVDEN			(1)	  //0=normal       1=inverted
#define BSWP			(0)	  //Byte swap control
#define HWSWP			(1)	  //Half word swap control

#define VBPD			(12)
#define VFPD			(4)
#define VSPW			(10)
#define HBPD			(43)
#define HFPD			(8)
#define HSPW			(1)

#define CLKVAL_TFT		(4)


void lcd_enable(void)
{
	struct s3c24x0_gpio * const gpio = s3c24x0_get_base_gpio();
	struct s3c24x0_lcd * const lcd = s3c24x0_get_base_lcd();

	gpio->GPGUP = gpio->GPGUP & ((~(1 << 4)) | (1 << 4));
	gpio->GPGCON = gpio->GPGCON & ((~( 3 << 8)) | ( 3 << 8));
	gpio->GPGDAT = gpio->GPGDAT | (1 << 4 );

	lcd->LCDCON5 = lcd->LCDCON5 & ((~( 1 << 3)) | (1 << 3));	// PWREN
	lcd->LCDCON5 = lcd->LCDCON5 & ((~( 1 << 5)) | (0 << 5));	// INVPWREN

	lcd->LCDCON1 |= 1;
	
	gpio->GPBUP = gpio->GPBUP & ((~(1 << 1)) | (1 << 1));
	gpio->GPBCON = gpio->GPBCON & ((~( 3 << 2)) | ( 3 << 2));
	gpio->GPBDAT = gpio->GPBDAT | (1 << 1 );
}
 
/*******************************************************************************
 *
 * Init video chip with common Linux graphic modes (lilo)
 */
void *video_hw_init (void)
{
    struct s3c24x0_lcd * const lcd = s3c24x0_get_base_lcd();
    GraphicDevice *pGD = (GraphicDevice *)&smi;
    int videomode;
	int bppmode;
    unsigned long t1, hsynch, vsynch;
    char *penv;
    int tmp, i, bits_per_pixel;
    struct ctfb_res_modes *res_mode;
    struct ctfb_res_modes var_mode;

    tmp = 0;
 
	videomode = CONFIG_SYS_DEFAULT_VIDEO_MODE;
	/* get video mode via environment */
	if ((penv = getenv ("videomode")) != NULL) {
         /* deceide if it is a string */
		if (penv[0] <= '9') {
			videomode = (int) simple_strtoul (penv, NULL, 16);
			tmp = 1;
		}
	}
	else
	{
		tmp = 1;
	}
	if (tmp) {
		/* parameter are vesa modes */
		/* search params */
		for (i = 0; i < VESA_MODES_COUNT; i++) {
			if (vesa_modes[i].vesanr == videomode)
			break;
		}
	 	if (i == VESA_MODES_COUNT) {
			printf ("no VESA Mode found, switching to mode 0x%x ", CONFIG_SYS_DEFAULT_VIDEO_MODE);
			i = 0;
		}
		res_mode = (struct ctfb_res_modes *) &res_mode_init[vesa_modes[i].resindex];
		bits_per_pixel = vesa_modes[i].bits_per_pixel;
	} 
	else
	{
		res_mode = (struct ctfb_res_modes *) &var_mode;
		bits_per_pixel = video_get_params (res_mode, penv);
	}
 
	/* calculate hsynch and vsynch freq (info only) */
	t1 = (res_mode->left_margin + res_mode->xres + res_mode->right_margin + res_mode->hsync_len) / 8;
	t1 *= 8;
	t1 *= res_mode->pixclock;
	t1 /= 1000;
	hsynch = 1000000000L / t1;
	t1 *= (res_mode->upper_margin + res_mode->yres + res_mode->lower_margin + res_mode->vsync_len);
	t1 /= 1000;
	vsynch = 1000000000L / t1;

	/* fill in Graphic device struct */
	sprintf (pGD->modeIdent, "%dx%dx%d %ldkHz %ldHz", res_mode->xres, res_mode->yres, bits_per_pixel, (hsynch / 1000), (vsynch / 1000));

	pGD->winSizeX = res_mode->xres;
	pGD->winSizeY = res_mode->yres;
	pGD->plnSizeX = res_mode->xres;
	pGD->plnSizeY = res_mode->yres;
	 
	switch (bits_per_pixel)
	{
		case 8:
			pGD->gdfBytesPP = 1;
			pGD->gdfIndex = GDF__8BIT_INDEX;
			bppmode = 11;
		    break;
		case 16:
			pGD->gdfBytesPP = 2;
			pGD->gdfIndex = GDF_16BIT_565RGB;
			bppmode = 12;
		    break;
		case 24:
		    pGD->gdfBytesPP = 3;
		    pGD->gdfIndex = GDF_24BIT_888RGB;
			bppmode = 13;
		    break;
	}

	pGD->frameAdrs = LCD_VIDEO_ADDR;
	pGD->memSize = VIDEO_MEM_SIZE;

	lcd->LCDCON1 = (CLKVAL_TFT << 8) | (MVAL_USED << 7) | (3 << 5) | (bppmode << 1) | 0;
	lcd->LCDCON2 = (VBPD << 24) | (pGD->winSizeY-1 << 14) | (VFPD << 6) | (VSPW);
	lcd->LCDCON3 = (HBPD << 19) | (pGD->winSizeX-1 << 8) | (HFPD);
	lcd->LCDCON4 = (MVAL << 8) | (HSPW);
	lcd->LCDCON5 = (1 << 11) | (0 << 10) | (1 << 9) | (1 << 8) | (0 << 7) | (0 << 6) | (1 << 3) | (BSWP << 1) | (HWSWP);

	lcd->LCDINTMSK |= (3);
	lcd->LPCSEL &= (~7);
	lcd->TPAL = 0x0;

	writel((pGD->frameAdrs >> 1), &lcd->LCDSADDR1); 
 
	/* This marks the end of the frame buffer. */
	writel((((readl(&lcd->LCDSADDR1))&0x1fffff) + (pGD->winSizeX+0) * pGD->winSizeY), &lcd->LCDSADDR2); 
	writel((pGD->winSizeX & 0x7ff), &lcd->LCDSADDR3); 
 
	/* Clear video memory */
	memset((void *)pGD->frameAdrs, 0, pGD->memSize);

	lcd_enable();
 
	return ((void*)&smi);
}
 
void video_set_lut (unsigned int index,      /* color number */
                unsigned char r, /* red */
                unsigned char g, /* green */
                unsigned char b  /* blue */
		)
 {
 }
 
 #endif /* CONFIG_VIDEO_S3C2440 */
