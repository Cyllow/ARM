#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NUC900_reg.h"
#include "wblib.h"
#include "NUC900_vpost.h"
#include "NUC900_i2c.h"

extern void lcdprintf(char *pcStr,...);
extern int diag_touch_screen(void);
extern void lcdputchar(int pos_x,int pos_y,char c);
extern int  get_touch_data(int *x_pos, int *y_pos);

extern void System_init();

int _Touch_Event = 0;//触摸事件
int Plane_x = 20;//飞机的X轴，Y轴固定为14，让飞机一直在屏幕底部
char MAP[15][40] = {' '};//地图
int Is_has_Bullet = 0;//是否有子弹
int Buttle_x = 0;//子弹的x坐标
int Buttle_y = 0;//子弹的y坐标

//IRQ中断，使能触摸事件然后再关闭IRQ中断
void irq_interrupt()
{
	_Touch_Event = 1;
	sysDisableInterrupt(IRQ_GROUP0); 	
}


//获取触摸事件的坐标（已修正）
int get_touch_xy(int *x_pos, int *y_pos)
{
	int x,y;
	if(get_touch_data(&x,&y) == -1)//获取位置失败
		return -1;
	if (x < 400)  x = 400;
	if (y < 600)  y = 600;
					
	x = ((x - 400) * 320) / (3700 - 400);
	y = ((y - 600) * 240) / (3600 - 600);
    			
    x = x/8;	//一个字符宽占8个像素
	y = y/16;	//一个字符高占16个像素
	y = 15 - y;	//矫正
	*x_pos = x;
	*y_pos = y;
	return 1;
}

//对飞机进行移动
void Plane_move(int i)
{
	//清除飞机残影
	lcdputchar(Plane_x - 1,14,' ');
	lcdputchar(Plane_x,14,' ');
	lcdputchar(Plane_x + 1,14,' ');
	
	//更新飞机坐标
	Plane_x = Plane_x + i;
	
	//重新画出飞机
	lcdputchar(Plane_x - 1,14,'=');
	lcdputchar(Plane_x,14,'^');
	lcdputchar(Plane_x + 1,14,'=');
}

//初始化地图
void MAP_init()
{
	int i,j;
	for(i = 0;i < 15;i++)
	{
		for(j = 0;j < 40;j++)
		{
			MAP[i][j] = ' ';
		}
	}
}

//更新地图
void MAP_update()
{
	int i,j;
	static monster = 0;//生成敌机的位置
	
	//将地图所有元素向下移动一行
	for(i = 13;i >= 0;i--)
	{
		for(j = 0; j < 40;j++)
		{
			MAP[i + 1][j] = MAP[i][j];
		}
	}
	
	//将地图的第一行全置为空格
	for(i = 0;i < 40;i++)
	{
		MAP[0][i] = ' ';
	}
	MAP[0][monster] = '|';//在第一行生成敌机
	monster = (monster + 7) % 40;//更新要生成敌机的位置
}

//画出地图
void MAP_printf()
{
	int i,j;
	for(i = 0;i < 15;i++)
	{
		for(j = 0;j < 40;j++)
		{
			lcdputchar(j,i,MAP[i][j]);
		}
	}
}

//判断飞机是否死亡
int Is_Plane_die()
{
	if(MAP[14][Plane_x - 1] == '|')//飞机左边碰到了敌机
		return 1;
	else if(MAP[14][Plane_x] == '|')//飞机中间部分碰到了敌机
		return 1;
	else if(MAP[14][Plane_x + 1] == '|')//飞机右边碰到了敌机
		return 1;
	else
		return 0;
}

//判定子弹是否打中敌机
int Is_Buttle_kill_monster()
{
	if(Is_has_Bullet == 1 && MAP[Buttle_y][Buttle_x] == '|')//当前子弹位置有敌机
		return 1;
	else if(Is_has_Bullet == 1 && MAP[Buttle_y + 1][Buttle_x] == '|')//当前子弹位置下面有敌机
		return 1;
	else
		return 0;
}

//游戏结束
void Game_over()
{
	int i,j;
	long long count;
	for(i = 0;i < 40;i++)//清屏
	{
		for(j = 0;j < 15;j++)
		{
			lcdputchar(i,j,' ');
		}
	}
	//在LCD中间输出Game over
	lcdputchar(16,7,'G');
	lcdputchar(17,7,'a');
	lcdputchar(18,7,'m');
	lcdputchar(19,7,'e');
	lcdputchar(20,7,' ');
	lcdputchar(21,7,'o');
	lcdputchar(22,7,'v');
	lcdputchar(23,7,'e');
	lcdputchar(24,7,'r');
	
	//等待一段时间
	for(count = 0;count < 1000 * 500;count++)
		j = 0;
	
	//清屏
	for(i = 0;i < 40;i++)
	{
		for(j = 0;j < 15;j++)
		{
			lcdputchar(i,j,' ');
		}
	}
}



void Plane_War()
{
	int x_pos,y_pos;
	long long count = 0;
	lcdprintf(" ");
	
	//系统初始化（i2c...）
	System_init();
	
	//设置IRQ中断并为其设置中断处理函数irq_interrupt()
	sysDisableInterrupt(IRQ_GROUP0); 	
	sysInstallISR(IRQ_LEVEL_1, IRQ_GROUP0, (PVOID)irq_interrupt);
	
	//初始化地图
	MAP_init();
	
	//画出飞机初始化位置
	lcdputchar(Plane_x - 1,14,'=');
	lcdputchar(Plane_x,14,'^');
	lcdputchar(Plane_x + 1,14,'=');
	
	while(1)
	{
		sysEnableInterrupt(IRQ_GROUP0);//使能IRQ中断
		if (_Touch_Event == 1)//有触摸事件
		{
			if(get_touch_xy(&x_pos,&y_pos))//获取触摸位置
			{
				if(x_pos < 20 && Plane_x - 1 != 0)//按下的位置为屏幕左半边
					Plane_move(-1);					//飞机往左移动一格
				else if(x_pos >= 20 && Plane_x + 1 != 39)//按下位置为屏幕的右半边
					Plane_move(1);					//飞机往右移动一格
			}
			_Touch_Event = 0;
		}
		
		
		count++;
		if(count == 1000*10)//每隔一段时间再重新刷新
		{
			count = 0;
			MAP_update();	//更新地图
			MAP_printf();	//输出新地图
			
			//打印飞机位置
			lcdputchar(Plane_x - 1,14,'=');	
			lcdputchar(Plane_x,14,'^');
			lcdputchar(Plane_x + 1,14,'=');
			
			if(Is_Plane_die())//飞机死亡
			{
				Game_over();
				break;
			}
			
			if(Is_has_Bullet)//有子弹
			{
				Buttle_y--;	//更新子弹坐标
				if(Is_Buttle_kill_monster())//子弹打中敌机
				{
					Is_has_Bullet = 0;	//无子弹了
					//在地图中清除敌机
					MAP[Buttle_y][Buttle_x] = ' ';
					MAP[Buttle_y + 1][Buttle_x] = ' ';
					lcdputchar(Buttle_x,Buttle_y,' ');
					lcdputchar(Buttle_x,Buttle_y + 1,' ');
				}
				else
				{
					if(Buttle_y > 0)//子弹还没出屏幕
						lcdputchar(Buttle_x,Buttle_y,'^');
					else//子弹出屏幕范围
						Is_has_Bullet = 0;
				}
			}
			else//无子弹时
			{
				Is_has_Bullet = 1;
				Buttle_x = Plane_x;//在飞机的上一行、同列生成一颗子弹
				Buttle_y = 13;
				lcdputchar(Buttle_x,Buttle_y,'^');//输出子弹
			}
		}
	}
}