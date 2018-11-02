#include "net_protocol.h"
#include "rtc.h"
#include "usart.h"
#include "24cxx.h"
#include "common.h"

//读取/处理网络数据
u16 time_out = 0;
u16 NetDataFrameHandle(pTcp *tcp,u8 *outbuf,u8 *hold_reg,CONNECT_STATE_E connect_state)
{
	s16 ret = 0;
	u16 len = 0;
	u8 buf[1500];
//	static u16 time_out = 0;

	memset(buf,0,1500);

	len = (*tcp)->read(tcp,buf);
	if(len != 0)
	{
		time_out = 0;
		
		if(connect_state == ON_SERVER)
		{
			ret = (s16)NetDataAnalysis(buf,len,outbuf,hold_reg);
		}

		if(ret == 0)
		{
			memcpy(Usart1TxBuf,buf,len);
			Usart1SendLen = len;
			USART_ITConfig(USART1, USART_IT_TXE, ENABLE);		//用串口发送出去
		}

		memset(buf,0,len);
	}
	else
	{
		time_out ++;
		
		if(time_out >= 600)		//一分钟内未收到任何数据，强行关闭连接
		{
			time_out = 0;
			
			ret = -1;
		}
	}
	
	return ret;
}

//网络数据帧协议解析
u16 NetDataAnalysis(u8 *buf,u16 len,u8 *outbuf,u8 *hold_reg)
{
	u16 ret = 0;
	u16 pos1 = 0;
	u16 pos2 = 0xFFFF;
	u16 data_len = 0;

	u8 cmd_code = 0;
	u8 read_check_sum = 0;
	u8 cal_check_sum = 0;

	u8 buf_tail[6] = {0xFE,0xFD,0xFC,0xFB,0xFA,0xF9};

	if(DeviceUUID != NULL)
	{
		pos2 = MyStrstr(buf,DeviceUUID,len,UU_ID_LEN - 2);
	}

	cmd_code = *(buf + 8);

	pos1 = MyStrstr(buf,buf_tail,len,6);

	if(pos1 != 0xFFFF && (pos2 != 0xFFFF || cmd_code == 0xF0))
	{
		if(*(buf + 0) == 0x68 && \
			*(buf + 7) == 0x68 && \
			*(buf + pos1 - 1) == 0x16)							//判断包头和包尾
		{
										//获取功能码
			data_len = DeviceUUID == NULL ? (*(buf + 9)):(*(buf + 9) - UU_ID_LEN - 2);				//获取有效数据的长度
			read_check_sum = *(buf + pos1 - 2);					//获取校验和
			cal_check_sum = CalCheckSum(buf, pos1 - 2);			//计算校验和

			if(read_check_sum == cal_check_sum)
			{
				switch(cmd_code)
				{
					case 0xE0:									//发送固定信息，上行，在别处处理

					break;

					case 0xE1:									//发送心跳，上行，在别处处理

					break;

					case 0xE2:									//开关灯/调光，下行

					break;

					case 0xE3:									//远程升级OTA，下行
						ret = SetUpdateFirmWareInfo(cmd_code,buf + 10 + 36,data_len,outbuf);
					break;

					case 0xE4:									//重启/复位，下行
						ret = ControlDeviceReset(cmd_code,buf + 10 + 36,data_len,outbuf);
					break;

					case 0xE5:									//设置定时发送间隔,下行
						ret = SetDeviceUpLoadINCL(cmd_code,buf + 10 + 36,data_len,outbuf);
					break;

					case 0xE6:									//控制柜断电/通电，下行

					break;

					case 0xE7:									//设置亮灭灯定时策略，下行

					break;

					case 0xE8:									//读取/发送设备配置信息，下行

					break;

					case 0xE9:									//设置设备的工作模式，手动或者自动模式

					break;

					case 0xF0:									//设置设备UUID，固定64个字节
						ret = SetDeviceUUID(cmd_code,buf + 10,data_len,outbuf);
					break;

					case 0xF1:									//从服务器获取时间戳
						ret = GetTimeDateFromServer(cmd_code,buf + 10 + 36,data_len,outbuf);
					break;

					case 0x80:									//应答，下行,上行在别处处理
						UnPackAckPacket(cmd_code,buf + 10 + 36,data_len);
					break;

					default:									//此处要给云端应答一个功能码错误信息

					break;
				}
			}
		}
		else	//此处可以给云端应答一个校验错误信息
		{

		}
	}
	else		//此处可以给云端应答一个校验错误信息
	{

	}

	return ret;
}

//解析ACK包
u8 UnPackAckPacket(u8 cmd_code,u8 *buf,u8 len)
{
	u8 ret = 0;

	if(len == 2)
	{
		if(*(buf + 1) == 0)
		{
			ret = 1;
		}
	}

	return ret;
}

//ACK打包
u16 PackAckPacket(u8 cmd_code,u8 *data,u8 *outbuf)
{
	u16 len = 0;

	len = PackNetData(0x80,data,2,outbuf);

	return len;
}

//下发更新固件命令
u16 SetUpdateFirmWareInfo(u8 cmd_code,u8 *buf,u8 len,u8 *outbuf)
{
	u8 out_len = 0;
	u8 data_buf[2] = {0,0};
	data_buf[0] = cmd_code;

	if(len == 5)
	{
		NewFirmWareVer    = (((u16)(*(buf + 0))) << 8) + (u16)(*(buf + 1));
		NewFirmWareBagNum = (((u16)(*(buf + 2))) << 8) + (u16)(*(buf + 3));
		LastBagByteNum    = *(buf + 4);

		if(NewFirmWareBagNum == 0 || NewFirmWareBagNum > MAX_FW_BAG_NUM \
			|| NewFirmWareVer == 0 || NewFirmWareVer > MAX_FW_VER \
			|| LastBagByteNum == 0 || LastBagByteNum > MAX_FW_LAST_BAG_NUM)  //128 + 2 + 4 = 134
		{
			data_buf[1] = 1;
		}
		else
		{
			HaveNewFirmWare = 0xAA;
			if(NewFirmWareAdd == 0xAA)
			{
				NewFirmWareAdd = 0x55;
			}
			else if(NewFirmWareAdd == 0x55)
			{
				NewFirmWareAdd = 0xAA;
			}
			else
			{
				NewFirmWareAdd = 0xAA;
			}

			WriteOTAInfo(HoldReg,0);		//将数据写入EEPROM

			NeedToReset = 1;				//重新启动
		}
	}
	else
	{
		data_buf[1] = 2;
	}

	out_len = PackAckPacket(cmd_code,data_buf,outbuf);

	return out_len;
}

//远程重启
u16 ControlDeviceReset(u8 cmd_code,u8 *buf,u8 len,u8 *outbuf)
{
	u8 out_len = 0;
	u8 data_buf[2] = {0,0};
	data_buf[0] = cmd_code;

	if(len == 0)
	{
		NeedToReset = 1;
	}
	else
	{
		data_buf[1] = 2;
	}

	out_len = PackAckPacket(cmd_code,data_buf,outbuf);

	return out_len;
}

//设置设备数据上传时间间隔
u16 SetDeviceUpLoadINCL(u8 cmd_code,u8 *buf,u8 len,u8 *outbuf)
{
	u8 out_len = 0;
	u8 data_buf[2] = {0,0};
	u16 incl;

	data_buf[0] = cmd_code;

	if(len == 2)												//数据长度必须是64
	{
		incl = ((u16)(*(buf + 0)) << 16) + (*(buf + 1));

		if(incl <= MAX_UPLOAD_INVL)
		{
			UpLoadINCL = incl;

			memcpy(&HoldReg[UPLOAD_INVL_ADD],buf,2);
			WriteDataFromHoldBufToEeprom(&HoldReg[UPLOAD_INVL_ADD],UPLOAD_INVL_ADD, UPLOAD_INVL_LEN - 2);
		}
		else
		{
			data_buf[1] = 1;
		}
	}
	else
	{
		data_buf[1] = 2;
	}

	out_len = PackAckPacket(cmd_code,data_buf,outbuf);

	return out_len;
}

//设置设备的UUID
u16 SetDeviceUUID(u8 cmd_code,u8 *buf,u8 len,u8 *outbuf)
{
	u8 out_len = 0;
	u8 data_buf[2] = {0,0};
	u8 uuid_buf[38];

	data_buf[0] = cmd_code;

	if(len == 36)												//数据长度必须是64
	{
		memset(uuid_buf,0,38);

		memcpy(&HoldReg[UU_ID_ADD],buf,36);

		GetDeviceUUID();

		WriteDataFromHoldBufToEeprom(&HoldReg[UU_ID_ADD],UU_ID_ADD, UU_ID_LEN - 2);
	}
	else
	{
		data_buf[1] = 2;
	}

	out_len = PackAckPacket(cmd_code,data_buf,outbuf);

	return out_len;
}

//从服务器获取时间戳
u16 GetTimeDateFromServer(u8 cmd_code,u8 *buf,u8 len,u8 *outbuf)
{
	u8 out_len = 0;
	u8 data_buf[2] = {0,0};
	u8 year = 0;
	u8 mon = 0;
	u8 day = 0;
	u8 hour = 0;
	u8 min = 0;
	u8 sec = 0;

	data_buf[0] = cmd_code;

	if(len == 6)												//数据长度必须是64
	{
		year = *(buf + 0);
		mon  = *(buf + 1);
		day  = *(buf + 2);
		hour = *(buf + 3);
		min  = *(buf + 4);
		sec  = *(buf + 5);

		if(year >= 18 && mon <= 12 && day <= 31 && hour <= 23 && min <= 59 && sec <= 59)
		{
			RTC_Set(year + 2000,mon,day,hour,min,sec);

			GetTimeOK = 1;
		}
		else
		{
			data_buf[1] = 1;
		}
	}
	else
	{
		data_buf[1] = 2;
	}

	out_len = PackAckPacket(cmd_code,data_buf,outbuf);

	return out_len;
}
































