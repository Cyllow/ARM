#include <stdio.h>
#include <string.h>
#include "NUC900_reg.h"
#include "wblib.h"
#include "NUC900_vpost.h"
#include "NUC900_i2c.h"


static int volatile LCD_BUFF_BASE;


void dl()
{
	long long i,j=0;
	for(i=0;i<1000*700;i++)
	{
		j+=1;
	}
	
	
}

VOID re()
{
	short *ptr;
	int i;
	
	ptr = (short*)LCD_BUFF_BASE;

    for(i=0;i<320*240;i++)
    {
    	*ptr++ = 0;
    }
}


void diag(char a[])
{  
    LCDFORMATEX lcdformatex;
    UINT32 		*pframbuf=NULL;
    int			i;int j;int size=0;int k=0;
    
   
     

	
	lcdformatex.ucVASrcFormat = VA_SRC_RGB565;
    vpostLCMInit(&lcdformatex);
	pframbuf = vpostGetFrameBuffer();
	LCD_BUFF_BASE = (int) pframbuf;



	
   	 
		
	for(j=1;j<=240;j++)
	{
		for(i=1;i<=320;i+=2)
   	 {
    	
    	if(a[k]=='0')
    	{
    		size=0;
    	}
    	else
    	{
    		size=0xfff;
    	}
    	k+=2;
    	*pframbuf++ =size;
    	
   	 }
   	 
	}
	
          
	outpw(REG_AIC_GEN, inpw(REG_AIC_GEN) | 0x01);     
   	
	
    
}







