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
#include <SI32_SPI_A_Type.h>
#include <SI32_CLKCTRL_A_Type.h>
#include <SI32_PBSTD_A_Type.h>
#include <SI32_DMACTRL_A_Type.h>
#include <SI32_DMADESC_A_Type.h>
#include <SI32_DMAXBAR_A_Type.h>
#include <SI32_DMAXBAR_A_Support.h>
// application
#include "mySPI0.h"
#include "myCpu.h"

// DMA Channel Descriptors (primary and alternate)
// - Alignment and order are required by the DMACTRL module.
// - Alternate descriptors are required for ping-pong and scatter-gather cycles only.
SI32_DMADESC_A_Type desc_pri[SI32_DMACTRL_NUM_CHANNELS]  __attribute__ ((aligned SI32_DMADESC_PRI_ALIGN));
SI32_DMADESC_A_Type desc_alt[SI32_DMACTRL_NUM_CHANNELS]  __attribute__ ((aligned SI32_DMADESC_ALT_ALIGN));


uint32_t rx_ch, tx_ch;

uint32_t SPI_Input_Data[2] = {0x00000000, 0x00000000};
uint32_t SPI_Output_Data[2] = {0x00000000, 0x00000000} ;

#define SPI0_DMA_NUM_WORDS 2

//==============================================================================
//MODE FUNCTIONS
//==============================================================================
//------------------------------------------------------------------------------
// mySPI0_run_master_mode()
//
// This function uses the SPI0 to write two words to the SPI Slave.  The first
// word is a "write" and the second word is a "read".  The slave should 
// increment the value of the first word by "+2" and send it back when the
// master reads the second word.  A verification of the word written and 
// the word read ensures communication between master and slave.
//------------------------------------------------------------------------------
void mySPI0_run_master_mode(void)
{
   static uint32_t spi_data_out = 0;
   static uint32_t spi_data_in = 0;
 
   // Increment the SPI Data Out
   spi_data_out++;
   
   // Generate NSS Falling Edge, reset FIFOs
   SI32_SPI_A_clear_nss (SI32_SPI_0);
   SI32_SPI_A_flush_rx_fifo(SI32_SPI_0);   
   SI32_SPI_A_flush_tx_fifo(SI32_SPI_0);
   

   // Write a word to the slave, ignore data shifted in
   SI32_SPI_A_clear_all_interrupts(SI32_SPI_0);
   SI32_SPI_A_write_tx_fifo_u32(SI32_SPI_0, spi_data_out);
   while(0 == SI32_SPI_A_is_shift_register_empty_interrupt_pending(SI32_SPI_0)); 
   while (SI32_SPI_A_get_rx_fifo_count(SI32_SPI_0) < 4); 
   SI32_SPI_A_flush_rx_fifo(SI32_SPI_0);  


   // Read the response from the slave, shift out zeros
   SI32_SPI_A_clear_all_interrupts(SI32_SPI_0);
   SI32_SPI_A_write_tx_fifo_u32(SI32_SPI_0, 0x00000000);
   //while(0 == SI32_SPI_A_is_shift_register_empty_interrupt_pending(SI32_SPI_0)); 
   while (SI32_SPI_A_get_rx_fifo_count(SI32_SPI_0) < 4);
   spi_data_in = SI32_SPI_A_read_rx_fifo_u32(SI32_SPI_0);


   // Generate NSS Rising Edge
   SI32_SPI_A_set_nss (SI32_SPI_0);
   
 
   // Update the Debug Terminal
   printf("SPI0 wrote: %d, SPI0 Read: %d", spi_data_out, spi_data_in);
   if(spi_data_in == spi_data_out + 0x02)
   {
      printf(" Verified!\n");
   } else
   {
      printf(" Failed!!!\n");
   }
}

//------------------------------------------------------------------------------
// mySPI0_run_dma_master_mode()
//
// This function uses the DMA to write two words to the SPI Slave.  The first
// word is a "write" and the second word is a "read".  The slave should 
// increment the value of the first word by "+2" and send it back when the
// master reads the second word.  A verification of the word written and 
// the word read ensures communication between master and slave.
//------------------------------------------------------------------------------
void mySPI0_run_dma_master_mode(void)
{
   // Increment the SPI Data Out
   SPI_Output_Data[0]++;

   printf("SPI0 DMA Transfer Size: %d words, Sent: %d, ", SPI0_DMA_NUM_WORDS, SPI_Output_Data[0]);
   

   // 2. Setup and enable the DMA controller and DMAXBAR
   SI32_CLKCTRL_A_enable_apb_to_modules_1(SI32_CLKCTRL_0, SI32_CLKCTRL_A_APBCLKG1_MISC1);
   SI32_CLKCTRL_A_enable_ahb_to_dma_controller(SI32_CLKCTRL_0);
   SI32_DMACTRL_A_write_baseptr(SI32_DMACTRL_0, (uint32_t)desc_pri);
   SI32_DMACTRL_A_enable_module(SI32_DMACTRL_0);

   // 3. Route DMA signals from the peripheral function to a DMA channel
   //    See SI32_DMAXBAR_Support.h for a complete list of mappings
   rx_ch = SI32_DMAXBAR_A_select_channel_peripheral(SI32_DMAXBAR_0, SI32_DMAXBAR_CHAN13_SPI0_RX);
   tx_ch = SI32_DMAXBAR_A_select_channel_peripheral(SI32_DMAXBAR_0, SI32_DMAXBAR_CHAN14_SPI0_TX);

   // 4. Configure DMA descriptors for the basic cycle
   //    See SI32_DMADESC_Support.h for a list of common config values
   SI32_DMADESC_A_configure(&desc_pri[rx_ch], 
        SI32_SPI_0_RX_ENDPOINT, SPI_Input_Data, SPI0_DMA_NUM_WORDS, SI32_DMADESC_A_CONFIG_WORD_RX); //RX -> Input_Data
   SI32_DMADESC_A_configure(&desc_pri[tx_ch], 
       SPI_Output_Data, SI32_SPI_0_TX_ENDPOINT, SPI0_DMA_NUM_WORDS, SI32_DMADESC_A_CONFIG_WORD_TX);  //TX <- Output_Data


   // Generate NSS Falling Edge
   SI32_SPI_A_clear_nss (SI32_SPI_0);

   // 5. Arm the DMA channel
   SI32_DMACTRL_A_enable_channel(SI32_DMACTRL_0, rx_ch);
   SI32_DMACTRL_A_enable_channel(SI32_DMACTRL_0, tx_ch);
   
   // 6. Run the complete peripheral DMA cycle
   SI32_SPI_A_enable_dma_requests(SI32_SPI_0);

   // Wait until the shift register empties.
   SI32_SPI_A_clear_underrun_interrupt(SI32_SPI_0);
   while(!SI32_SPI_A_is_underrun_interrupt_pending(SI32_SPI_0));

   //for(i = 0; i < 100000; i++);
   //SI32_SPI_A_flush_rx_fifo(SI32_SPI_0);

   // Generate NSS Rising Edge
   SI32_SPI_A_set_nss (SI32_SPI_0);

   printf("Received: %d", SPI_Input_Data[1]);

   if(SPI_Input_Data[1] == SPI_Output_Data[0] + 0x02)
   {
      printf(" Verified!\n");
   } else
   {
      printf(" Failed!!!\n");
   }
   
}

//-eof--------------------------------------------------------------------------
