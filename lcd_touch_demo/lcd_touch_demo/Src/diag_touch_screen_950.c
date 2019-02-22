#include <stdio.h>
#include <string.h>

#include "NUC900_reg.h"
#include "wblib.h"
#include "NUC900_vpost.h"
#include "NUC900_i2c.h"

#define I2C_NUMBER				2

#define I2C_FIFO_LEN			4
#define I2C_MAX_BUF_LEN			450
#define I2C_MAX_DATA_LEN		(I2C_MAX_BUF_LEN - 10)


#define GPIOH_DIR			(GPIO_BA+0x54)
#define GPIOH_DATAIN		(GPIO_BA+0x5C)

#define SYNC_TYPE

#define LEN                  8
#define SHIFT                3
#define AXIS_X1             60
#define AXIS_Y1             60 
#define AXIS_X2            260
#define AXIS_Y2             60   
#define AXIS_X3             60
#define AXIS_Y3            180
#define AXIS_X4            260
#define AXIS_Y4            180

#define SYS_CLOCK			14318000

static int volatile LCD_BUFF_BASE;
#undef sysprintf
#define sysprintf printf


enum tsc2007_pd {
  PD_POWERDOWN = 0, /* penirq */
  PD_IREFOFF_ADCON = 1, /* no penirq */
  PD_IREFON_ADCOFF = 2, /* penirq */
  PD_IREFON_ADCON = 3, /* no penirq */
  PD_PENIRQ_ARM = PD_IREFON_ADCOFF,
  PD_PENIRQ_DISARM = PD_IREFON_ADCON
};

enum tsc2007_m {
  M_12BIT = 0,
  M_8BIT = 1
};

enum tsc2007_cmd {
  MEAS_TEMP0 = 0,
  MEAS_VBAT1 = 1,
  MEAS_IN1 = 2,
  MEAS_TEMP1 = 4,
  MEAS_VBAT2 = 5,
  MEAS_IN2 = 6,
  ACTIVATE_NX_DRIVERS = 8,
  ACTIVATE_NY_DRIVERS = 9,
  ACTIVATE_YNX_DRIVERS = 10,
  SETUP = 11,
  MEAS_XPOS = 12,
  MEAS_YPOS = 13,
  MEAS_Z1POS = 14,
  MEAS_Z2POS = 15
};

#define TSC2007_CMD(cn,pdn,m) (((cn) << 4) | ((pdn) << 2) | ((m) << 1))

static int first_time_test = 1;

char 	i2c_buff[1024];
volatile int  i2c_state, i2c_data_len, i2c_data_pos;
static volatile int  i2c_interrupt = 0;

static int bNackValid;

static int _Touch_Event = 0;

static int i2c_init(void);
VOID set_target(INT16 Center_X, INT16 Center_Y);
VOID reset_screen(VOID);
INT  ts_location_test(VOID);
INT  line_draw_test(VOID);
INT  get_touch_data(INT *x_pos, INT *y_pos);


static void delay_ticks(int tick)
{
	volatile int  t0;

	for (t0 = 0; t0 < tick * 1000; t0 ++)
	;	
}



void nirq0_irq()
{
	_Touch_Event = 1;
	sysDisableInterrupt(IRQ_GROUP0); 	
}


void System_init()
{
	LCDFORMATEX lcdformatex;
    UINT32 		*pframbuf=NULL;
    int			i;
#if 0	
	WB_PLL_T 			sysClock;
	
	sysClock.pll0 = PLL_200MHZ;				//PLL0 output clock
	sysClock.pll1 = PLL_100MHZ;				//PLL1 output clock
	sysClock.cpu_src = CPU_FROM_PLL0;		//Select CPU clock source
	sysClock.ahb_clk = AHB_CPUCLK_1_2;		//Select AHB clock divider
	sysClock.apb_clk = APB_AHB_1_2;			//Select APB clock divider
	sysSetPLLConfig(&sysClock);				//Call system function call
#endif

	sysSetTimerReferenceClock (TIMER0, SYS_CLOCK);
	sysStartTimer(TIMER0, 100, PERIODIC_MODE);
	
	lcdformatex.ucVASrcFormat = VA_SRC_RGB565;
    vpostLCMInit(&lcdformatex);
	pframbuf = vpostGetFrameBuffer();
	LCD_BUFF_BASE = (int) pframbuf;
    for (i=0; i < 320*240; i++)
    {
       *pframbuf++ = 0;
    }
	/* install nIRQ0 interrupt */
	sysDisableInterrupt(IRQ_GROUP0); 	
	sysInstallISR(IRQ_LEVEL_1, IRQ_GROUP0, (PVOID)nirq0_irq);
	sysSetLocalInterrupt(ENABLE_IRQ);                            

	/* enable nIRQ0 group interrupt */
	outpw(REG_AIC_GEN, inpw(REG_AIC_GEN) | 0x01);  
            
    i2c_init();
}


INT diag_touch_screen(void)
{  
    LCDFORMATEX lcdformatex;
    UINT32 		*pframbuf=NULL;
    int			i;
#if 0	
	WB_PLL_T 			sysClock;
	
	sysClock.pll0 = PLL_200MHZ;				//PLL0 output clock
	sysClock.pll1 = PLL_100MHZ;				//PLL1 output clock
	sysClock.cpu_src = CPU_FROM_PLL0;		//Select CPU clock source
	sysClock.ahb_clk = AHB_CPUCLK_1_2;		//Select AHB clock divider
	sysClock.apb_clk = APB_AHB_1_2;			//Select APB clock divider
	sysSetPLLConfig(&sysClock);				//Call system function call
#endif

	sysSetTimerReferenceClock (TIMER0, SYS_CLOCK);
	sysStartTimer(TIMER0, 100, PERIODIC_MODE);
	
	lcdformatex.ucVASrcFormat = VA_SRC_RGB565;
    vpostLCMInit(&lcdformatex);
	pframbuf = vpostGetFrameBuffer();
	LCD_BUFF_BASE = (int) pframbuf;
    for (i=0; i < 320*240; i++)
    {
       *pframbuf++ = 0;
    }	  

	if (!first_time_test)
		goto go_test;
   
    /* set GPIOH[3:0] for nIRQ[3:0] */
    //outpw(REG_MFSEL, inpw(REG_MFSEL) | 0x1000000);      

	/* install nIRQ0 interrupt */
	sysDisableInterrupt(IRQ_GROUP0); 	
	sysInstallISR(IRQ_LEVEL_1, IRQ_GROUP0, (PVOID)nirq0_irq);
	sysSetLocalInterrupt(ENABLE_IRQ);                            

	/* enable nIRQ0 group interrupt */
	outpw(REG_AIC_GEN, inpw(REG_AIC_GEN) | 0x01);  
            
    i2c_init();

	/* nIRQ0 must keep disable until tc2007 power down */
	//sysEnableInterrupt(IRQ_GROUP0); 	

go_test:
    
	if (ts_location_test() == 0)
	{
		sysprintf("Touch screen test OK.\n");
		//sysprintf("Press any key to continue next test...\n");
		//sysGetChar();
	}
	else
	{
		sysprintf("Press any key to exit test...\n");
		sysGetChar();
		return -1;
	}
	
	//line_draw_test();
	
    return 0;
}



static void i2c_Disable(void)	/* Disable i2c core and interrupt */
{
	outpw(REG_IIC0_CS, 0);
}


static void i2c_Enable(void)	/* Enable i2c core and interrupt */
{
	outpw(REG_IIC0_CS, 0x03);
}


static int i2c_IsBusFree(void)
{	
	if (((inpw(REG_IIC0_SWR) & 0x18) == 0x18) &&  //SDR and SCR keep high 
		((inpw(REG_IIC0_CS) & 0x0400) == 0))
	{  	
		//I2C_BUSY is false
		return 1;
	}
	else
	{		
		return 0;
	}

}


static int i2c_Command(int cmd)
{
	if (cmd & I2C_CMD_WRITE)
		bNackValid = 1;
	else
		bNackValid = 0;

	outpw(REG_IIC0_CMDR, cmd);

	return 0;

}

static int i2c_CalculateAddress(int mode)
{
	unsigned int addr;

	addr = 0x48;
	
	if (mode == I2C_STATE_WRITE)
		addr = ((addr << 1) & 0xfe);
	else
		addr = ((addr << 1) & 0xfe) | I2C_READ;

	i2c_buff[0] = addr & 0xff;
	return 0;	
}


static int i2c_read(unsigned char *buf, int count)
{	
	if (count == 0)
		return 0;
	
	if (!i2c_IsBusFree())
	{
		sysprintf("bus error\n");
		return -1;
	}
	
	if (count > I2C_MAX_BUF_LEN - 10)
		count = I2C_MAX_BUF_LEN - 10;

	i2c_state = I2C_STATE_READ;
	i2c_data_pos = 1;
	i2c_data_len = 1 + count + 1;/* plus 1 unused char */

	i2c_interrupt = 0;
	
	i2c_CalculateAddress(I2C_STATE_READ);
	i2c_Enable();
	outpw(REG_IIC0_TxR, i2c_buff[0] & 0xff	);
	i2c_Command(I2C_CMD_START | I2C_CMD_WRITE);

	while (!i2c_interrupt && (i2c_state != I2C_STATE_NOP)) ;

	i2c_Disable();

	memcpy(buf, i2c_buff + 2, count);
		
	return count;
}


static int i2c_write(unsigned char *buf, int count)
{	
	if (!i2c_IsBusFree())
		return -1;
	
	if (count > I2C_MAX_DATA_LEN)
		count = I2C_MAX_DATA_LEN;

	memcpy(i2c_buff + 1 , buf, count);
	
	i2c_state = I2C_STATE_WRITE;
	i2c_data_pos = 1;
	i2c_data_len = 1 + count;
	
	i2c_interrupt = 0;
	
	i2c_CalculateAddress(I2C_STATE_WRITE);		//put the first data byte into i2c_buff
	i2c_Enable();
	outpw(REG_IIC0_TxR, i2c_buff[0] & 0xff);	/* send first byte */
	i2c_Command(I2C_CMD_START | I2C_CMD_WRITE);

	while (!i2c_interrupt && (i2c_state != I2C_STATE_NOP)) ;
	
	i2c_Disable();

	return count;
}


void i2c_irq()
{
	int 	csr, val;

	//sysprintf("i2c interrupt.\n");
	
	i2c_interrupt = 1;

	csr = inpw(REG_IIC0_CS);
	csr |= 0x04;		/* mark interrupt flag */
	outpw(REG_IIC0_CS, csr);
		
	if (i2c_state == I2C_STATE_NOP)
	{
		sysprintf("i2c_irq - I2C_STATE_NOP\n");
		return;
	}

	//sysprintf("bNackValid = %d\n", bNackValid);

	if ((csr & 0x800) && bNackValid)
	{					/* NACK only valid in WRITE */
		sysprintf("NACK Error\n");
		i2c_Command(I2C_CMD_STOP);
		goto wake_up_quit;
	}
	else if (csr & 0x200)
	{					/* Arbitration lost */
		sysprintf("Arbitration lost\n");
		i2c_Command(I2C_CMD_STOP);
		goto wake_up_quit;
	}
	else if (!(csr & 0x100))
	{					/* transmit complete */
		if (i2c_data_pos < 1)
		{	
			/* send address state */
			//sysprintf("irq - Send Address\n");
			val = i2c_buff[i2c_data_pos++] & 0xff;
			outpw(REG_IIC0_TxR, val);
			i2c_Command(I2C_CMD_WRITE);
		}
		else if (i2c_state == I2C_STATE_READ)
		{	
			i2c_buff[i2c_data_pos++] = inpw(REG_IIC0_RxR) & 0xff;

			//sysprintf("Read Pos : [%02x]   Data : [%02x]\n", i2c_data_pos, i2c_buff[i2c_data_pos-1]);

			if (i2c_data_pos < i2c_data_len)
			{
				//sysprintf("Read Next\n");
				if (i2c_data_pos == i2c_data_len - 1)	/* last character */
					i2c_Command(I2C_CMD_READ |
									I2C_CMD_STOP |
									I2C_CMD_NACK);
				else
					i2c_Command(I2C_CMD_READ);
			}
			else
			{
				//sysprintf("Read Over \n");
				goto wake_up_quit;
			}
		}
		else if (i2c_state == I2C_STATE_WRITE)
		{	/* write data */
			if (i2c_data_pos < i2c_data_len)
			{
				//sysprintf("Write Pos : [%02x]   Data : [%02x]\n", i2c_data_pos, i2c_buff[i2c_data_pos]);

				val = i2c_buff[i2c_data_pos];

				outpw(REG_IIC0_TxR, val);
				
				if (i2c_data_pos == i2c_data_len - 1)	/* last character */
					i2c_Command(I2C_CMD_WRITE| I2C_CMD_STOP);				
				else
					i2c_Command(I2C_CMD_WRITE);

				i2c_data_pos ++;
			}
			else
			{
				//sysprintf("Write Over\n");
				goto wake_up_quit;
			}
		}

	}
	
	return;
	
wake_up_quit:
	i2c_state = I2C_STATE_NOP;
	//sysprintf("Wake up \n");
}



static int tsc2007_powerdown(void)
{
	//char 	c;
	unsigned char 	data[2];
	int 	count = 0;
		
	data[0] = TSC2007_CMD(MEAS_XPOS, PD_POWERDOWN, M_12BIT);
	//i2c_set_dev_address(0x48);
	//i2c_set_sub_address(c, 0);
	if (i2c_write(&data[0], 1) < 0)
		return 0;	
		
	return count;
}

static int i2c_SetSpeed(int sp)
{
	UINT32  reg;

	if ((sp != 100) && (sp != 400))
		return -1;

	reg = I2C_INPUT_CLOCK/(sp * 5) -1;
	outpw(REG_IIC0_DIVIDER, reg & 0xffff);
	return 0;
}


/* init i2c_dev after open */
static int i2c_ResetI2C(void)
{	
	return i2c_SetSpeed(100);
}


static int i2c_init(void)	/* init I2C channel */
{
	int  t0;
	
	outpw(REG_CLKEN, inpw(REG_CLKEN) | 0xC0000000);
	
	outpw(REG_MFSEL, ((inpw(REG_MFSEL) | 0x4000) & 0xFFFF4FFF));
	
	outpw(REG_AIC_GEN, inpw(REG_AIC_GEN) | 0x400000);

	sysInstallISR(IRQ_LEVEL_1, IRQ_I2C_GROUP, (PVOID)i2c_irq);
	sysSetLocalInterrupt(ENABLE_IRQ);                            
	sysEnableInterrupt(IRQ_I2C_GROUP); 	

	/* enable I2C group interrupt */
	outpw(REG_AIC_GEN, inpw(REG_AIC_GEN) | 0x0C000000);  
	
	if (i2c_ResetI2C())
	{
		sysprintf("\n\nFailed to init I2C!!\n");
		return -1;
	}
	
	t0 = sysGetTicks(TIMER0);
	while (sysGetTicks(TIMER0) - t0 < 30)
		;
	
	tsc2007_powerdown();
	
	return 0;	
}


VOID set_target(INT16 Center_X, INT16 Center_Y)
{
    //Center_X = 60;
    //Center_Y = 60; 
    INT16 X,Y;

	Center_Y = 240 - Center_Y;

    for (X = (Center_X-20); X <= (Center_X+20); X++)
    {
       Y = Center_Y - 20;
       *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xF800;
       Y = Center_Y + 20;
       *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xF800;
    }
    
    for (Y = (Center_Y-20); Y <= (Center_Y+20); Y++)
    {
       X = Center_X - 20;
       *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xF800;
       X = Center_X + 20;
       *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xF800;
    }    
    
    for (X = (Center_X-20+1); X < (Center_X+20); X++)
    {
       for (Y = (Center_Y-20+1); Y < (Center_Y+20); Y++)
       {
           *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xFFFF;
       }
    }
    
    for (X = (Center_X-5); X <= (Center_X+5); X++)
    {
       Y = Center_Y;
       *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xF800;
    }
    
    for (Y = (Center_Y-5); Y <= (Center_Y+5); Y++)
    {
       X = Center_X;
       *(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xF800;
    }    

}

VOID reset_screen(VOID)
{
	short *ptr;
	int i;
	
	ptr = (short*)LCD_BUFF_BASE;

    for(i=0;i<320*240;i++)
    {
    	*ptr++ = 0;
    }
}


// get x position(should disable PENIRQ before executing this command)
int tsc2007_read_xpos(void)
{ 
	unsigned char data[2];
	int count, value;
		
	data[0] = TSC2007_CMD(MEAS_XPOS, PD_PENIRQ_DISARM, M_12BIT);

	if (i2c_write(&data[0], 1) < 0)
		return 0;
		
	delay_ticks(100);
			
	if ((count = i2c_read(&data[0] ,2)) < 0)
		return 0;
	
	value = data[0];
	value <<= 4;
	value += (data[1] >> 4);
		
	return value;
}

//get y position(should disable PENIRQ before executing this command)
int tsc2007_read_ypos(void)
{
	unsigned char data[2];
	int count, value;
	
	data[0] = TSC2007_CMD(MEAS_YPOS, PD_PENIRQ_DISARM, M_12BIT);

	if (i2c_write(&data[0], 1) < 0)
		return 0;

	delay_ticks(100);
	
	if ((count = i2c_read(&data[0],2)) < 0)
		return 0;
		
	value = data[0];
	value <<= 4;
	value += (data[1] >> 4);
		
	return value;
}

//get pressure value(should disable PENIRQ before executing this command)
int tsc2007_read_zpos(void)
{
	unsigned char data[2];
	int count, value;
	
	data[0] = TSC2007_CMD(MEAS_Z1POS, PD_PENIRQ_DISARM, M_12BIT);

	if (i2c_write(&data[0], 1) < 0)
		return 0;

	delay_ticks(100);
	
	if ((count = i2c_read(&data[0],2)) < 0)
		return 0;
		
	value = data[0];
	value <<= 4;
	value += (data[1] >> 4);
	
	return value;
}



INT  get_touch_data(INT *x_pos, INT *y_pos)
{
	int  z_pos;

	sysDisableInterrupt(IRQ_GROUP0); 	
	
	z_pos = tsc2007_read_xpos();

	delay_ticks(10);

	*x_pos = tsc2007_read_ypos();

	delay_ticks(10);

	*y_pos = tsc2007_read_zpos();

	delay_ticks(10);
	
	tsc2007_powerdown();

	delay_ticks(10);

	sysEnableInterrupt(IRQ_GROUP0); 

	if ((z_pos < 500) && (*x_pos > 800))
		return -1;
		
	//sysprintf("(%d %d %d)\n", *x_pos, *y_pos, z_pos);
	
	return 0;
}


INT  ts_location_test()
{
	INT   location[4][2] = { AXIS_X1, AXIS_Y1, AXIS_X2, AXIS_Y2, AXIS_X3, AXIS_Y3, AXIS_X4, AXIS_Y4 };
	INT   loc_idx;
	INT   x_pos, y_pos;
	INT   t0;
	
#if defined(NUC951)
	sysprintf("H/W Setting for touch screen test: Switch13 OFF. Switch14 ON\n");
#endif	
	
	for (loc_idx = 0; loc_idx < 4; loc_idx++)
	{
		reset_screen();

    	sysprintf("Please touch the %dst target on touch panel.\n", loc_idx+1);
    	sysprintf("If this test can not detect a touch inside the target in 60 seconds, the test failed.\n");
		sysprintf("\n\n\n");
		
		set_target(location[loc_idx][0], location[loc_idx][1]);
		
		t0 = sysGetTicks(TIMER0);
		while (1)
		{
			sysEnableInterrupt(IRQ_GROUP0);
			
			if (_Touch_Event == 1)
			{
				if (get_touch_data(&x_pos, &y_pos) == 0)
				{
					if (x_pos < 400)  x_pos = 400;
					if (y_pos < 600)  y_pos = 600;
					
					x_pos = ((x_pos - 400) * 320) / (3700 - 400);
					y_pos = ((y_pos - 600) * 240) / (3600 - 600);
    			
    				if ((x_pos <= (location[loc_idx][0] + 20)) && 
    					(x_pos >= (location[loc_idx][0] - 20)) && 
    					(y_pos <= (location[loc_idx][1] + 20)) && 
    					(y_pos >= (location[loc_idx][1] - 20)))
    				{
    					sysprintf("%dst taget test OK.\n\n", loc_idx+1);
    					break;
    				}	
    				else
    				{
    					//sysprintf("Touch on (%d %d), which did not located in (%d %d) ~ (%d %d)\n", 
    				    //	     x_pos, y_pos, location[loc_idx][0] - 20, location[loc_idx][1] - 20,
    				     //   	location[loc_idx][0] + 20, location[loc_idx][1] + 20); 
    				}
    			}
    			_Touch_Event = 0;
    		}
			
			if (sysGetTicks(TIMER0) - t0 > 6000)
			{
				sysprintf("Test time-out, it failed!!\n\n");
				return -1;
			}
		}
	}

	reset_screen();
	
	sysprintf("\n\nAll target test OK.\n\n");
	return 0;
}


INT  line_draw_test(VOID)
{
	INT   x_pos, y_pos, X, Y;
	INT   t0;
	
	reset_screen();

   	sysprintf("Free to draw on the touch panel ...\n");
   	sysprintf("This test will exit after 60 seconds.\n");
   	
	t0 = sysGetTicks(TIMER0);
   	while (1)
   	{
		sysEnableInterrupt(IRQ_GROUP0);
			
		if (_Touch_Event == 1)
		{
			if (get_touch_data(&x_pos, &y_pos) == 0)
			{
				if (x_pos < 500)  x_pos = 500;
				if (y_pos < 600)  y_pos = 600;
					
				x_pos = ((x_pos - 500) * 320) / (3600 - 500);
				y_pos = ((y_pos - 600) * 240) / (3600 - 600);
				
				if (x_pos < 2) x_pos = 2;
				if (x_pos > 317) x_pos = 317; 
				if (y_pos < 2) y_pos = 2;
				if (y_pos > 237) y_pos = 237;

    			for (X = (x_pos - 2); X < (x_pos + 2); X++)
    			{
       				for (Y = (y_pos - 2); Y < (y_pos + 2); Y++)
       				{
           				*(short*)(LCD_BUFF_BASE+(Y*320+X)*2) = (short)0xFFFF;
       				}
    			}
				
    			
    			_Touch_Event = 0;
    		}
    	}

		if (sysGetTicks(TIMER0) - t0 > 6000)
		{
			break;;
		}
   	}
   	
   	reset_screen();
   	return 0;
}


