/**********************
*	AVR自动键
*   by BG4UVR
*	2007.06.08
**********************/

/*
更新历史：
2007.06.09
1、在8515芯片完成主算法结构。
2007.09.23
1、开始进行移植到TINY13下的工作。1.2MHZ的默认时钟。定时器0工作在10ms
2007.09.30
1、更正词之间的间隔时间7为5.
2007.10.04
1、加长内置的呼叫语长度，方便长呼号的朋友有足够的空间来更改。
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>	//空间分配定义头文件

//按键端口
#define 	DI		PB0		//点输入
#define 	DA		PB1		//划输入
#define 	KEY		PB2		//自动呼叫/WPM设置键
#define 	OUT		PB3		//输出
#define 	BUZZER	PB4		//蜂鸣器

//定义速率界限与步进
#define WPM_MAX 30			//WPM最大值
#define WPM_MIN 10			//WPM最小值
#define WPM_DEFAULT 25		//默认WPM
#define WPM_STEP 1			//WPM调整步进

//定义自动呼叫语句的内容
prog_uchar cq[]={"CQ CQ CQ DE BG4UVR BG4UVR BG4UVR PSE K"};


//CW编码表
//字节的前5位表示点划的内容，0表示点，1表示划。后3位表示本码的位数
prog_uchar CW_TAB[]=
{
	0x95,	/// 	-..-. 	10010 101
	0xfd,	//0	 	-----	11111 101
	0x7d,	//1 	.----	01111 101
	0x3d,	//2 	..---	00111 101
	0x1d,	//3 	...--	00011 101
	0x0d,	//4 	....-	00001 101
	0x05,	//5 	.....	00000 101
	0x85,	//6 	-....	10000 101
	0xc5,	//7 	--...	11000 101
	0xe5,	//8 	---..	11100 101
	0xf5,	//9 	----.	11110 101
	0x42,	//A 	.-		01000 010
	0x84,	//B 	-...	10000 100
	0xa4,	//C 	-.-.	10100 100
	0x83,	//D 	-..		10000 011
	0x01,	//E 	.		00000 001
	0x24,	//F 	..-.	00100 100
	0xc3,	//G 	--.		11000 011
	0x04,	//H		....	00000 100
	0x02,	//I 	..		00000 010
	0x74,	//J 	.---	01110 100
	0xa3,	//K 	-.-		10100 011
	0x44,	//L 	.-..	01000 100
	0xc2,	//M		--		11000 010
	0x82,	//N 	-.		10000 010
	0xe3,	//O 	---		11100 011
	0x64,	//P 	.--.	01100 100
	0xd4,	//Q 	--.-	11010 100
	0x43,	//R 	.-.		01000 011
	0x03,	//S 	...		00000 011
	0x81,	//T 	-		10000 001
	0x23,	//U 	..-		00100 011
	0x14,	//V 	...-	00010 100
	0x63,	//W 	.--		01100 011
	0x94,	//X 	-..-	10010 100
	0xb4,	//Y 	-.--	10110 100
	0xc4,	//Z 	--..	11000 100
};

unsigned char wpm_save __attribute__((section(".eeprom")));		//wpm设置EEPROM存储变量

volatile unsigned char wpm_count;		//WPM计数器
unsigned int wpm_set;					//当前WPM设置值

volatile unsigned int power_count;
volatile unsigned char key_count;
volatile unsigned char cq_count;

//函数声明
static void avr_init(void);
void set_wpm(unsigned char updown);
void wpm_tone(void);
void snd_dida(unsigned char dida,unsigned char out);
void snd_space(unsigned char time);
void send_string(prog_uchar * msg);
void send_char(unsigned char x,unsigned char out);


//主程序
int main(void)
{
	unsigned char temp;
    avr_init();		//初始化
	wpm_tone();		//CW报当前WPM值
	while(1)
	{
		power_count=0;
		while(power_count<200)
		{
			temp=PINB&0x07;
			switch(temp)
			{
				case 6:				//DI
					snd_dida(0,1);			//发DI
					power_count=0;
					break;
				case 5:				//DA
					snd_dida(1,1);			//发DA
					power_count=0;
					break;
				case 3:				//KEY
					key_count=0;
					while(key_count<50);		//延时0.5秒
					if(PINB&_BV(KEY))				//如果键被松开，自动呼叫
					{
						GIMSK=_BV(PCIE);			//开引脚变化中断
						cq_count=0;
						while((cq_count++)<40)
						{
							send_string(cq);
							power_count=0;
							while((power_count<1500)&&(cq_count<40));		//延时15秒重发
						}
					}
					else							//如果键一直被按下，设置WPM
					{
						while(!(PINB&_BV(KEY)))
						{
							switch(PINB&0x03)
							{
								case 2:			//DI键
									set_wpm(1);		
									break;
								case 1:			//DA键
									set_wpm(0);
									break;
							}
						}
					}
					power_count=0;
					break;
			}
		}
		//进入掉电
		TIMSK0=0;					//关闭定时器0中断
		GIMSK=_BV(PCIE);			//使能引脚变化中断
		MCUCR|=_BV(SE);
		asm("sleep");				//进入掉电状态

		GIMSK=0;					//禁用端口变化中断
		TIMSK0=_BV(TOIE0);			//开启定时器中断
		power_count=0;
		key_count=0;
	}
    return(0);
}

//发指定字符串CW函数
void send_string(prog_uchar * msg)
{
	while(pgm_read_byte(msg)!=0)
	{
		send_char(pgm_read_byte(msg),1);
		msg++;
	}
}

//发送指定字符函数
void send_char(unsigned char x,unsigned char out)
{
	unsigned char temp,temp1,i;
	//计算偏移
	if(x==0x20)
	{
		snd_space(2);
	}
	else
	{
		if((x>=0x2f)&&(x<=0x39))
		{
			x-=0x2f;
		}
		else if((x>=0x41)&&(x<=0x5a))
		{
			x-=0x36; //0x41=66   66-12=55   55=0x36    
		}
		//读取码表
		temp=pgm_read_byte(&CW_TAB[x]);
		//发码
		temp1=temp&0x07;
		for(i=0;i<temp1;i++)
		{
			snd_dida(temp&0x80,out);
			temp<<=1;
		}
		snd_space(2);
	}
}

//设置WPM函数
void set_wpm(unsigned char updown)
{
	if(updown)
	{
		wpm_set+=WPM_STEP;
	}
	else
	{
		wpm_set-=WPM_STEP;
	}
	if(wpm_set>WPM_MAX)	wpm_set=WPM_MAX;
	if(wpm_set<WPM_MIN)	wpm_set=WPM_MIN;
	eeprom_busy_wait();			//保存WPM设置
	eeprom_write_byte(&wpm_save,wpm_set);
	wpm_tone();
}

//CW报WPM值
void wpm_tone(void)
{
	if((wpm_set/10)!=0)
	{
		send_char((wpm_set/10)+0x30,0);
	}
	send_char((wpm_set%10)+0x30,0);
}

//发DI、DA函数
/*
dida=0,发DI	dida=1,发DA
out=0不输出，out=1输出
WPM值和定时器溢出时间的关系
发点时长需要定时器溢出的次数 = 1200 / 定时器溢出的mS值
*/
void snd_dida(unsigned char dida,unsigned char out)
{
	unsigned int temp;
	wpm_count=0;
	PORTB|=_BV(BUZZER);				//开蜂鸣器
	if(out)			                                                
	{
		PORTB|=_BV(OUT);			//输出
	}
	if(dida)
	{
		temp=360;
	}
	else
	{
		temp=120;
	}
	while(wpm_count<(temp/wpm_set));
	PORTB&=~_BV(BUZZER);			//关闭蜂鸣器
	PORTB&=~_BV(OUT);				//关闭输出
	snd_space(1);
}


//发字符间的空格
void snd_space(unsigned char times)
{
	wpm_count=0;
	while(wpm_count<((120*times)/wpm_set));
}


//10ms定时器中断
SIGNAL(SIG_OVERFLOW0)
{
	TCNT0=0x44;				//重装预装值
	wpm_count++;			//WPM计数器+1
	power_count++;
	key_count++;
}

//外部电平变化中断
SIGNAL(SIG_PIN_CHANGE0)
{
	MCUCR&=~_BV(SE);		//清除SE
	cq_count=40;
}

//端口初始化
static void avr_init(void)
{
	DDRB = _BV(OUT) | _BV(BUZZER);
	PORTB = _BV(DI) | _BV(DA) | _BV(KEY) ;
	
	//恢复WPM的设置
	eeprom_busy_wait();
	wpm_set=eeprom_read_byte(&wpm_save);

	//如果未设置过，默认设置为20
	if((wpm_set>WPM_MAX)||(wpm_set<WPM_MIN))
	{
		wpm_set=WPM_DEFAULT;
		eeprom_busy_wait();
		eeprom_write_byte(&wpm_save,wpm_set);
	}
	
	//中断产生时间
	//中断时间=计数值*分频比/晶振频率
	//计数值=(晶振频率/分频比)*中断时间
	TCCR0A=0x00;					//定时器0工作在模式0
	TCCR0B=_BV(CS01)|_BV(CS00);		//分频比64
	TCNT0=0x44;						//定时10ms
	//TCNT0=0x93;						//600KHZ			
	TIMSK0=_BV(TOIE0);				//使能定时器0中断

	MCUCR=_BV(SM1);								//设置电源管理为掉电方式
	PCMSK=_BV(PCINT0)|_BV(PCINT1)|_BV(PCINT2);	//使能PB0、PB1、PB2的引脚变化中断
	//GIMSK=_BV(PCIE);							//使能引脚变化中断。与INT0的区别是INT0只有1个脚
	
	sei();					//开总中断允许
}
