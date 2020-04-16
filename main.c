/**********************
*	AVR�Զ���
*   by BG4UVR
*	2007.06.08
**********************/

/*
������ʷ��
2007.06.09
1����8515оƬ������㷨�ṹ��
2007.09.23
1����ʼ������ֲ��TINY13�µĹ�����1.2MHZ��Ĭ��ʱ�ӡ���ʱ��0������10ms
2007.09.30
1��������֮��ļ��ʱ��7Ϊ5.
2007.10.04
1���ӳ����õĺ����ﳤ�ȣ����㳤���ŵ��������㹻�Ŀռ������ġ�
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>	//�ռ���䶨��ͷ�ļ�

//�����˿�
#define 	DI		PB0		//������
#define 	DA		PB1		//������
#define 	KEY		PB2		//�Զ�����/WPM���ü�
#define 	OUT		PB3		//���
#define 	BUZZER	PB4		//������

//�������ʽ����벽��
#define WPM_MAX 30			//WPM���ֵ
#define WPM_MIN 10			//WPM��Сֵ
#define WPM_DEFAULT 25		//Ĭ��WPM
#define WPM_STEP 1			//WPM��������

//�����Զ�������������
prog_uchar cq[]={"CQ CQ CQ DE BG4UVR BG4UVR BG4UVR PSE K"};


//CW�����
//�ֽڵ�ǰ5λ��ʾ�㻮�����ݣ�0��ʾ�㣬1��ʾ������3λ��ʾ�����λ��
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

unsigned char wpm_save __attribute__((section(".eeprom")));		//wpm����EEPROM�洢����

volatile unsigned char wpm_count;		//WPM������
unsigned int wpm_set;					//��ǰWPM����ֵ

volatile unsigned int power_count;
volatile unsigned char key_count;
volatile unsigned char cq_count;

//��������
static void avr_init(void);
void set_wpm(unsigned char updown);
void wpm_tone(void);
void snd_dida(unsigned char dida,unsigned char out);
void snd_space(unsigned char time);
void send_string(prog_uchar * msg);
void send_char(unsigned char x,unsigned char out);


//������
int main(void)
{
	unsigned char temp;
    avr_init();		//��ʼ��
	wpm_tone();		//CW����ǰWPMֵ
	while(1)
	{
		power_count=0;
		while(power_count<200)
		{
			temp=PINB&0x07;
			switch(temp)
			{
				case 6:				//DI
					snd_dida(0,1);			//��DI
					power_count=0;
					break;
				case 5:				//DA
					snd_dida(1,1);			//��DA
					power_count=0;
					break;
				case 3:				//KEY
					key_count=0;
					while(key_count<50);		//��ʱ0.5��
					if(PINB&_BV(KEY))				//��������ɿ����Զ�����
					{
						GIMSK=_BV(PCIE);			//�����ű仯�ж�
						cq_count=0;
						while((cq_count++)<40)
						{
							send_string(cq);
							power_count=0;
							while((power_count<1500)&&(cq_count<40));		//��ʱ15���ط�
						}
					}
					else							//�����һֱ�����£�����WPM
					{
						while(!(PINB&_BV(KEY)))
						{
							switch(PINB&0x03)
							{
								case 2:			//DI��
									set_wpm(1);		
									break;
								case 1:			//DA��
									set_wpm(0);
									break;
							}
						}
					}
					power_count=0;
					break;
			}
		}
		//�������
		TIMSK0=0;					//�رն�ʱ��0�ж�
		GIMSK=_BV(PCIE);			//ʹ�����ű仯�ж�
		MCUCR|=_BV(SE);
		asm("sleep");				//�������״̬

		GIMSK=0;					//���ö˿ڱ仯�ж�
		TIMSK0=_BV(TOIE0);			//������ʱ���ж�
		power_count=0;
		key_count=0;
	}
    return(0);
}

//��ָ���ַ���CW����
void send_string(prog_uchar * msg)
{
	while(pgm_read_byte(msg)!=0)
	{
		send_char(pgm_read_byte(msg),1);
		msg++;
	}
}

//����ָ���ַ�����
void send_char(unsigned char x,unsigned char out)
{
	unsigned char temp,temp1,i;
	//����ƫ��
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
		//��ȡ���
		temp=pgm_read_byte(&CW_TAB[x]);
		//����
		temp1=temp&0x07;
		for(i=0;i<temp1;i++)
		{
			snd_dida(temp&0x80,out);
			temp<<=1;
		}
		snd_space(2);
	}
}

//����WPM����
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
	eeprom_busy_wait();			//����WPM����
	eeprom_write_byte(&wpm_save,wpm_set);
	wpm_tone();
}

//CW��WPMֵ
void wpm_tone(void)
{
	if((wpm_set/10)!=0)
	{
		send_char((wpm_set/10)+0x30,0);
	}
	send_char((wpm_set%10)+0x30,0);
}

//��DI��DA����
/*
dida=0,��DI	dida=1,��DA
out=0�������out=1���
WPMֵ�Ͷ�ʱ�����ʱ��Ĺ�ϵ
����ʱ����Ҫ��ʱ������Ĵ��� = 1200 / ��ʱ�������mSֵ
*/
void snd_dida(unsigned char dida,unsigned char out)
{
	unsigned int temp;
	wpm_count=0;
	PORTB|=_BV(BUZZER);				//��������
	if(out)			                                                
	{
		PORTB|=_BV(OUT);			//���
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
	PORTB&=~_BV(BUZZER);			//�رշ�����
	PORTB&=~_BV(OUT);				//�ر����
	snd_space(1);
}


//���ַ���Ŀո�
void snd_space(unsigned char times)
{
	wpm_count=0;
	while(wpm_count<((120*times)/wpm_set));
}


//10ms��ʱ���ж�
SIGNAL(SIG_OVERFLOW0)
{
	TCNT0=0x44;				//��װԤװֵ
	wpm_count++;			//WPM������+1
	power_count++;
	key_count++;
}

//�ⲿ��ƽ�仯�ж�
SIGNAL(SIG_PIN_CHANGE0)
{
	MCUCR&=~_BV(SE);		//���SE
	cq_count=40;
}

//�˿ڳ�ʼ��
static void avr_init(void)
{
	DDRB = _BV(OUT) | _BV(BUZZER);
	PORTB = _BV(DI) | _BV(DA) | _BV(KEY) ;
	
	//�ָ�WPM������
	eeprom_busy_wait();
	wpm_set=eeprom_read_byte(&wpm_save);

	//���δ���ù���Ĭ������Ϊ20
	if((wpm_set>WPM_MAX)||(wpm_set<WPM_MIN))
	{
		wpm_set=WPM_DEFAULT;
		eeprom_busy_wait();
		eeprom_write_byte(&wpm_save,wpm_set);
	}
	
	//�жϲ���ʱ��
	//�ж�ʱ��=����ֵ*��Ƶ��/����Ƶ��
	//����ֵ=(����Ƶ��/��Ƶ��)*�ж�ʱ��
	TCCR0A=0x00;					//��ʱ��0������ģʽ0
	TCCR0B=_BV(CS01)|_BV(CS00);		//��Ƶ��64
	TCNT0=0x44;						//��ʱ10ms
	//TCNT0=0x93;						//600KHZ			
	TIMSK0=_BV(TOIE0);				//ʹ�ܶ�ʱ��0�ж�

	MCUCR=_BV(SM1);								//���õ�Դ����Ϊ���緽ʽ
	PCMSK=_BV(PCINT0)|_BV(PCINT1)|_BV(PCINT2);	//ʹ��PB0��PB1��PB2�����ű仯�ж�
	//GIMSK=_BV(PCIE);							//ʹ�����ű仯�жϡ���INT0��������INT0ֻ��1����
	
	sei();					//�����ж�����
}
