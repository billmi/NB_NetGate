#include "realy.h"
#include "common.h"
#include "usart.h"


u8 ControlRealy(u8 ch,u8 cmd)
{
	u8 ret = 0;
	u8 i = 0;
	u8 re_try_cnt = 0;
	
	u8 send_buf[15] = "AT+RELAY=0,0\r\n\0";
	
	if(ch < 10)
	{
		send_buf[9] = ch + 0x30;
	}
	else
	{
		send_buf[9] = ch + 0x41 - 10;
	}
	
	send_buf[11] = cmd + 0x30;
	
	RE_SEND1:
	UsartSendString(USART1,send_buf, 14);
	
	i = 10;
	
	while(-- i)
	{
		delay_ms(300);
		
		if(Usart1RecvEnd == 0xAA)
		{
			Usart1RecvEnd = 0;
			
			if(MyStrstr(Usart1RxBuf, (u8 *)"OK", Usart1FrameLen, 2) != 0xFFFF)
			{
				memset(Usart1RxBuf,0,Usart1FrameLen);
				i = 1;
				ret = 1;
			}
		}
		
		if(ret == 0)
		{
			re_try_cnt ++;
			
			if(re_try_cnt < 10)
			{
				goto RE_SEND1;
			}
			else
			{
				return 0;
			}
		}
	}
	
	return ret;
}














































