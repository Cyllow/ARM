#include <stdio.h>
#include <stdlib.h>

#include "NUC900_reg.h"
#include "wblib.h"
#include "NUC900_vpost.h"
#include "NUC900_i2c.h"

extern void Plane_War();
extern void lcdprintf(char *pcStr,...);
extern void lcdputchar(int pos_x,int pos_y,char c);
extern void System_init();

int Menu_Touch_Event = 0;//触摸事件

//IRQ中断，使能触摸事件然后再关闭IRQ中断
void Menu_irq_interrupt()
{
	Menu_Touch_Event = 1;
	sysDisableInterrupt(IRQ_GROUP0); 	
}

void main()
{
	
	while(1)
	{
		System_init();
	
		//设置IRQ中断并为其设置中断处理函数irq_interrupt()
		sysDisableInterrupt(IRQ_GROUP0); 	
		sysInstallISR(IRQ_LEVEL_1, IRQ_GROUP0, (PVOID)Menu_irq_interrupt);
		
		lcdprintf(" ");
		lcdputchar(15,7,'S');
		lcdputchar(16,7,'t');
		lcdputchar(17,7,'a');
		lcdputchar(18,7,'r');
		lcdputchar(19,7,'t');
		lcdputchar(20,7,' ');
		lcdputchar(21,7,'g');
		lcdputchar(22,7,'a');
		lcdputchar(23,7,'m');
		lcdputchar(24,7,'e');
		lcdputchar(25,7,'!');
		
		while(1)
		{
			sysEnableInterrupt(IRQ_GROUP0);//使能IRQ中断
			if(Menu_Touch_Event == 1)//有触摸事件
			{
				Menu_Touch_Event = 0;
				break;
			}
		}
		
		Plane_War();//飞机大战
		Menu_Touch_Event = 0;
	}
}