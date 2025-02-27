#ifndef __RTC_H
#define __RTC_H

extern uint16_t _RTC_Time[];

void _RTC_Init(void);
void _RTC_SetTime(void);
void _RTC_ReadTime(void);
void SetAlarm(uint32_t seconds);


#endif
