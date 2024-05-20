#include "stm32f10x.h"                  // Device header
#include <time.h>
#include "OLED.h"
#include "Delay.h"

uint16_t _RTC_Time[] = {2024, 3, 15, 14, 03, 36};	//定义全局的时间数组，数组内容分别为年、月、日、时、分、秒

void _RTC_SetTime(void);				//函数声明


void _RTC_Init(void)
{

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);		//开启PWR的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);		//开启BKP的时钟


	PWR_BackupAccessCmd(ENABLE);							//使用PWR开启对备份寄存器的访问

	if ((RCC->BDCR & RCC_BDCR_RTCEN) != RCC_BDCR_RTCEN && GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3) )	//判断RTC是否是第一次配置
															//if成立则执行第一次的RTC配置
	{
		RCC_LSEConfig(RCC_LSE_ON);							//开启LSE时钟
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != SET)
		{
		GPIO_ResetBits(GPIOC, GPIO_Pin_13);
		OLED_ShowString(3, 1, "waiting for LSE");
		OLED_ShowString(1, 1, "Remembering the memory");
		OLED_ShowString(2, 1, " memory");
		}
		GPIO_SetBits(GPIOC, GPIO_Pin_13);
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);				//选择RTCCLK来源为LSE
		RCC_RTCCLKCmd(ENABLE);								//RTCCLK使能

		RTC_WaitForSynchro();								//等待同步
		RTC_WaitForLastTask();								//等待上一次操作完成
		_RTC_SetTime();									//设置时间，调用此函数，全局数组里时间值刷新到RTC硬件电路
	}
	else if ((RCC->BDCR & RCC_BDCR_RTCEN) != RCC_BDCR_RTCEN && !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3))
	{
		OLED_ShowString(1, 1, "Battery is dead");
		OLED_ShowString(2, 1, "We lost memory.");
		OLED_ShowString(4, 1, "Go find WYL");
		while (1)
		{
			GPIO_ResetBits(GPIOC, GPIO_Pin_13);
			Delay_ms(1000);
			GPIO_SetBits(GPIOC, GPIO_Pin_13);
			Delay_ms(1000);
		}

	}
	else													//RTC不是第一次配置
	{
		RTC_WaitForSynchro();								//等待同步
		RTC_WaitForLastTask();								//等待上一次操作完成
	}
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
    RTC_WaitForLastTask();

	RTC_SetPrescaler(32768);						//设置RTC预分频器，预分频后的计数频率为1Hz，加快弥补32750
	RTC_WaitForLastTask();								//等待上一次操作完成

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

}

//如果LSE无法起振导致程序卡死在初始化函数中
//可将初始化函数替换为下述代码，使用LSI当作RTCCLK
//LSI无法由备用电源供电，故主电源掉电时，RTC走时会暂停
/*
void _RTC_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);

	PWR_BackupAccessCmd(ENABLE);

	if (BKP_ReadBackupRegister(BKP_DR1) != 0x0805)
	{
		RCC_LSICmd(ENABLE);
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET);

		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		RCC_RTCCLKCmd(ENABLE);

		RTC_WaitForSynchro();
		RTC_WaitForLastTask();

		RTC_SetPrescaler(40000 - 1);
		RTC_WaitForLastTask();

		_RTC_SetTime();

		BKP_WriteBackupRegister(BKP_DR1, 0x0805);
	}
	else
	{
		RCC_LSICmd(ENABLE);				//即使不是第一次配置，也需要再次开启LSI时钟
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET);

		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		RCC_RTCCLKCmd(ENABLE);

		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
	}
}*/

void _RTC_SetTime(void)
{
	time_t time_cnt;		//定义秒计数器数据类型
	struct tm time_date;	//定义日期时间数据类型

	time_date.tm_year = _RTC_Time[0] - 1900;		//将数组的时间赋值给日期时间结构体
	time_date.tm_mon = _RTC_Time[1] - 1;
	time_date.tm_mday = _RTC_Time[2];
	time_date.tm_hour = _RTC_Time[3];
	time_date.tm_min = _RTC_Time[4];
	time_date.tm_sec = _RTC_Time[5];

	time_cnt = mktime(&time_date) - 8 * 60 * 60;	//调用mktime函数，将日期时间转换为秒计数器格式
													//- 8 * 60 * 60为东八区的时区调整

	RTC_SetCounter(time_cnt);						//将秒计数器写入到RTC的CNT中
	RTC_WaitForLastTask();							//等待上一次操作完成
}


void _RTC_ReadTime(void)
{
	time_t time_cnt;		//定义秒计数器数据类型
	struct tm time_date;	//定义日期时间数据类型

	time_cnt = RTC_GetCounter() + 8 * 60 * 60;		//读取RTC的CNT，获取当前的秒计数器
													//+ 8 * 60 * 60为东八区的时区调整

	time_date = *localtime(&time_cnt);				//使用localtime函数，将秒计数器转换为日期时间格式

	_RTC_Time[0] = time_date.tm_year + 1900;		//将日期时间结构体赋值给数组的时间
	_RTC_Time[1] = time_date.tm_mon + 1;
	_RTC_Time[2] = time_date.tm_mday;
	_RTC_Time[3] = time_date.tm_hour;
	_RTC_Time[4] = time_date.tm_min;
	_RTC_Time[5] = time_date.tm_sec;
}

void SetAlarm(uint32_t seconds)
{
    RTC_WaitForLastTask();
    RTC_SetAlarm(RTC_GetCounter() + seconds);
    RTC_WaitForLastTask();
}
