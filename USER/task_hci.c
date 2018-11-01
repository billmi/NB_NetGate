#include "task_hci.h"
#include "delay.h"
#include "usart.h"
#include "inventr.h"
#include "at_protocol.h"
#include "realy.h"


TaskHandle_t xHandleTaskHCI = NULL;

void vTaskHCI(void *pvParameters)
{
	u16 send_len4 = 0;

	AT_CommandInit();

	UsartSendString(USART1,"READY\r\n", 7);

	while(1)
	{
		if(Usart4RecvEnd == 0xAA)
		{
			Usart4RecvEnd = 0;

			send_len4 = AT_CommandDataAnalysis(Usart4RxBuf,Usart4FrameLen,Usart4TxBuf,HoldReg);

			memset(Usart4RxBuf,0,Usart4FrameLen);
		}

		if(send_len4 != 0)
		{
			UsartSendString(UART4,Usart4TxBuf, send_len4);

			memset(Usart4TxBuf,0,send_len4);

			send_len4 = 0;
		}

		delay_ms(100);
	}
}






































