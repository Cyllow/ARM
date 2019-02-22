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

int _Touch_Event = 0;//�����¼�
int Plane_x = 20;//�ɻ���X�ᣬY��̶�Ϊ14���÷ɻ�һֱ����Ļ�ײ�
char MAP[15][40] = {' '};//��ͼ
int Is_has_Bullet = 0;//�Ƿ����ӵ�
int Buttle_x = 0;//�ӵ���x����
int Buttle_y = 0;//�ӵ���y����

//IRQ�жϣ�ʹ�ܴ����¼�Ȼ���ٹر�IRQ�ж�
void irq_interrupt()
{
	_Touch_Event = 1;
	sysDisableInterrupt(IRQ_GROUP0); 	
}


//��ȡ�����¼������꣨��������
int get_touch_xy(int *x_pos, int *y_pos)
{
	int x,y;
	if(get_touch_data(&x,&y) == -1)//��ȡλ��ʧ��
		return -1;
	if (x < 400)  x = 400;
	if (y < 600)  y = 600;
					
	x = ((x - 400) * 320) / (3700 - 400);
	y = ((y - 600) * 240) / (3600 - 600);
    			
    x = x/8;	//һ���ַ���ռ8������
	y = y/16;	//һ���ַ���ռ16������
	y = 15 - y;	//����
	*x_pos = x;
	*y_pos = y;
	return 1;
}

//�Էɻ������ƶ�
void Plane_move(int i)
{
	//����ɻ���Ӱ
	lcdputchar(Plane_x - 1,14,' ');
	lcdputchar(Plane_x,14,' ');
	lcdputchar(Plane_x + 1,14,' ');
	
	//���·ɻ�����
	Plane_x = Plane_x + i;
	
	//���»����ɻ�
	lcdputchar(Plane_x - 1,14,'=');
	lcdputchar(Plane_x,14,'^');
	lcdputchar(Plane_x + 1,14,'=');
}

//��ʼ����ͼ
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

//���µ�ͼ
void MAP_update()
{
	int i,j;
	static monster = 0;//���ɵл���λ��
	
	//����ͼ����Ԫ�������ƶ�һ��
	for(i = 13;i >= 0;i--)
	{
		for(j = 0; j < 40;j++)
		{
			MAP[i + 1][j] = MAP[i][j];
		}
	}
	
	//����ͼ�ĵ�һ��ȫ��Ϊ�ո�
	for(i = 0;i < 40;i++)
	{
		MAP[0][i] = ' ';
	}
	MAP[0][monster] = '|';//�ڵ�һ�����ɵл�
	monster = (monster + 7) % 40;//����Ҫ���ɵл���λ��
}

//������ͼ
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

//�жϷɻ��Ƿ�����
int Is_Plane_die()
{
	if(MAP[14][Plane_x - 1] == '|')//�ɻ���������˵л�
		return 1;
	else if(MAP[14][Plane_x] == '|')//�ɻ��м䲿�������˵л�
		return 1;
	else if(MAP[14][Plane_x + 1] == '|')//�ɻ��ұ������˵л�
		return 1;
	else
		return 0;
}

//�ж��ӵ��Ƿ���ел�
int Is_Buttle_kill_monster()
{
	if(Is_has_Bullet == 1 && MAP[Buttle_y][Buttle_x] == '|')//��ǰ�ӵ�λ���ел�
		return 1;
	else if(Is_has_Bullet == 1 && MAP[Buttle_y + 1][Buttle_x] == '|')//��ǰ�ӵ�λ�������ел�
		return 1;
	else
		return 0;
}

//��Ϸ����
void Game_over()
{
	int i,j;
	long long count;
	for(i = 0;i < 40;i++)//����
	{
		for(j = 0;j < 15;j++)
		{
			lcdputchar(i,j,' ');
		}
	}
	//��LCD�м����Game over
	lcdputchar(16,7,'G');
	lcdputchar(17,7,'a');
	lcdputchar(18,7,'m');
	lcdputchar(19,7,'e');
	lcdputchar(20,7,' ');
	lcdputchar(21,7,'o');
	lcdputchar(22,7,'v');
	lcdputchar(23,7,'e');
	lcdputchar(24,7,'r');
	
	//�ȴ�һ��ʱ��
	for(count = 0;count < 1000 * 500;count++)
		j = 0;
	
	//����
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
	
	//ϵͳ��ʼ����i2c...��
	System_init();
	
	//����IRQ�жϲ�Ϊ�������жϴ�����irq_interrupt()
	sysDisableInterrupt(IRQ_GROUP0); 	
	sysInstallISR(IRQ_LEVEL_1, IRQ_GROUP0, (PVOID)irq_interrupt);
	
	//��ʼ����ͼ
	MAP_init();
	
	//�����ɻ���ʼ��λ��
	lcdputchar(Plane_x - 1,14,'=');
	lcdputchar(Plane_x,14,'^');
	lcdputchar(Plane_x + 1,14,'=');
	
	while(1)
	{
		sysEnableInterrupt(IRQ_GROUP0);//ʹ��IRQ�ж�
		if (_Touch_Event == 1)//�д����¼�
		{
			if(get_touch_xy(&x_pos,&y_pos))//��ȡ����λ��
			{
				if(x_pos < 20 && Plane_x - 1 != 0)//���µ�λ��Ϊ��Ļ����
					Plane_move(-1);					//�ɻ������ƶ�һ��
				else if(x_pos >= 20 && Plane_x + 1 != 39)//����λ��Ϊ��Ļ���Ұ��
					Plane_move(1);					//�ɻ������ƶ�һ��
			}
			_Touch_Event = 0;
		}
		
		
		count++;
		if(count == 1000*10)//ÿ��һ��ʱ��������ˢ��
		{
			count = 0;
			MAP_update();	//���µ�ͼ
			MAP_printf();	//����µ�ͼ
			
			//��ӡ�ɻ�λ��
			lcdputchar(Plane_x - 1,14,'=');	
			lcdputchar(Plane_x,14,'^');
			lcdputchar(Plane_x + 1,14,'=');
			
			if(Is_Plane_die())//�ɻ�����
			{
				Game_over();
				break;
			}
			
			if(Is_has_Bullet)//���ӵ�
			{
				Buttle_y--;	//�����ӵ�����
				if(Is_Buttle_kill_monster())//�ӵ����ел�
				{
					Is_has_Bullet = 0;	//���ӵ���
					//�ڵ�ͼ������л�
					MAP[Buttle_y][Buttle_x] = ' ';
					MAP[Buttle_y + 1][Buttle_x] = ' ';
					lcdputchar(Buttle_x,Buttle_y,' ');
					lcdputchar(Buttle_x,Buttle_y + 1,' ');
				}
				else
				{
					if(Buttle_y > 0)//�ӵ���û����Ļ
						lcdputchar(Buttle_x,Buttle_y,'^');
					else//�ӵ�����Ļ��Χ
						Is_has_Bullet = 0;
				}
			}
			else//���ӵ�ʱ
			{
				Is_has_Bullet = 1;
				Buttle_x = Plane_x;//�ڷɻ�����һ�С�ͬ������һ���ӵ�
				Buttle_y = 13;
				lcdputchar(Buttle_x,Buttle_y,'^');//����ӵ�
			}
		}
	}
}