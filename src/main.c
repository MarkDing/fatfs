//------------------------------------------------------------------------------
// Copyright (c) 2012 by Silicon Laboratories. 
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Silicon Laboratories End User 
// License Agreement which accompanies this distribution, and is available at
// http://developer.silabs.com/legal/version/v10/License_Agreement_v10.htm
// Original content and implementation provided by Silicon Laboratories.
//------------------------------------------------------------------------------
// library
#include <stdio.h>
// hal
#include <si32_device.h>
// application
#include "gModes.h"
#include "mySPI0.h"
#include "myCpu.h"
#include "diskio.h"
#include "ff.h"
//==============================================================================
// myApplication.
//==============================================================================
uint8_t sec_buf[512];
FATFS Fatfs;		/* File system object */
FIL Fil;			/* File object */
BYTE Buff[128];		/* File read buffer */

void die (		/* Stop with dying message */
	FRESULT rc	/* FatFs return value */
)
{
	printf("Failed with rc=%u.\n", rc);
	for (;;) ;
}
DWORD get_fattime (void)
{
	return	  ((DWORD)(2013 - 1980) << 25)	/* Year = 2012 */
			| ((DWORD)1 << 21)				/* Month = 1 */
			| ((DWORD)1 << 16)				/* Day_m = 1*/
			| ((DWORD)0 << 11)				/* Hour = 0 */
			| ((DWORD)0 << 5)				/* Min = 0 */
			| ((DWORD)0 >> 1);				/* Sec = 0 */
}
void fatfs_test()
{
	FRESULT rc;				/* Result code */
	DIR dir;				/* Directory object */
	FILINFO fno;			/* File information object */
	UINT bw, br, i;

	f_mount(0, &Fatfs);		/* Register volume work area (never fails) */

	printf("\nOpen an existing file (test.abp).\n");
	rc = f_open(&Fil, "TEST.ABP", FA_READ);
	if (rc) die(rc);

	printf("\nType the file content.\n");
	//for (;;)
	do
	{
		rc = f_read(&Fil, Buff, sizeof Buff, &br);	/* Read a chunk of file */
		if (rc || !br) break;			/* Error or end of file */
		for (i = 0; i < br/4; i++)		/* Type the data */
			putchar(Buff[i]);
	}while(0);
	if (rc) die(rc);

	printf("\nClose the file.\n");
	rc = f_close(&Fil);
	if (rc) die(rc);

	printf("\nCreate a new file (hello.txt).\n");
	rc = f_open(&Fil, "HELLO.TXT", FA_WRITE | FA_CREATE_ALWAYS);
	if (rc) die(rc);

	printf("\nWrite a text data. (Hello world!)\n");
	rc = f_write(&Fil, "Hello world!\r\n", 14, &bw);
	if (rc) die(rc);
	printf("%u bytes written.\n", bw);

	printf("\nClose the file.\n");
	rc = f_close(&Fil);
	if (rc) die(rc);

	printf("\nOpen root directory.\n");
	rc = f_opendir(&dir, "");
	if (rc) die(rc);

	printf("\nDirectory listing...\n");
	for (;;) {
		rc = f_readdir(&dir, &fno);		/* Read a directory item */
		if (rc || !fno.fname[0]) break;	/* Error or end of dir */
		if (fno.fattrib & AM_DIR)
			printf("   <dir>  %s\n", fno.fname);
		else
			printf("%8lu  %s\n", fno.fsize, fno.fname);
	}
	if (rc) die(rc);

	printf("\nTest completed.\n");
	while(1);
}
int main()
{
   #define USE_DMA 0
   // enter one of the operating modes for this application (myModes.c)
   #if (USE_DMA == 0)
      gModes_enter_master_mode();
   #elif (USE_DMA == 1)
      gModes_enter_dma_master_mode();
   #endif

   fatfs_test();
   disk_initialize(0);
   disk_read(0,sec_buf,0,1);
   while(1);

   // perform the following tasks forever
   while (1)
   {
      #if (USE_DMA == 0)
         mySPI0_run_master_mode();
      #elif (USE_DMA == 1)
         mySPI0_run_dma_master_mode();
      #endif
   }
}

//-eof--------------------------------------------------------------------------
