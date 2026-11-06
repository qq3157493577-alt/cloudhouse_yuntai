#include "stm32f10x.h"                  // Device header
#include "MyI2C.h"
#include "MPU6050.h"
#include "MPU6050_Reg.h"
#include "eMPL/inv_mpu.h"
#include "eMPL/inv_mpu_dmp_motion_driver.h"
#include <math.h>

#define MPU6050_ADDRESS			0xD0

static uint8_t DMP_I2CError;
static short DMP_Accel[3], DMP_Gyro[3];

void MPU6050_WriteReg(uint8_t RegAddress,uint8_t Data)
{
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(Data);
	MyI2C_ReceiveAck();
	MyI2C_Stop();
}

uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();
	
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);
	MyI2C_ReceiveAck();
	Data = MyI2C_ReceiveByte();
	MyI2C_SendAck(1);
	MyI2C_Stop();
	
	return Data;
}

// 连续写入：Addr 使用 7 位地址 0x68，成功返回 0，失败返回 1。
uint8_t MPU6050_Write_Len(uint8_t Addr, uint8_t RegAddress,
                          uint8_t Len, const uint8_t *Buf)
{
	uint8_t i;
	if (Addr > 0x7F || Len == 0 || Buf == 0)
	{
		return 1;
	}

	MyI2C_Start();
	MyI2C_SendByte((uint8_t)(Addr << 1));
	if (MyI2C_ReceiveAck() != 0) goto Error;

	MyI2C_SendByte(RegAddress);
	if (MyI2C_ReceiveAck() != 0) goto Error;

	for (i = 0; i < Len; i++)
	{
		MyI2C_SendByte(Buf[i]);
		if (MyI2C_ReceiveAck() != 0) goto Error;
	}
	MyI2C_Stop();
	return 0;

Error:
	DMP_I2CError = 1;
	// 未收到应答也要结束本次通信，释放总线。
	MyI2C_Stop();
	return 1;
}

// 连续读取：除最后一字节外发送 ACK，最后一字节发送 NACK。
uint8_t MPU6050_Read_Len(uint8_t Addr, uint8_t RegAddress,
                         uint8_t Len, uint8_t *Buf)
{
	uint8_t i;
	if (Addr > 0x7F || Len == 0 || Buf == 0)
	{
		return 1;
	}

	MyI2C_Start();
	MyI2C_SendByte((uint8_t)(Addr << 1));
	if (MyI2C_ReceiveAck() != 0) goto Error;

	MyI2C_SendByte(RegAddress);
	if (MyI2C_ReceiveAck() != 0) goto Error;

	MyI2C_Start();
	MyI2C_SendByte((uint8_t)((Addr << 1) | 0x01));
	if (MyI2C_ReceiveAck() != 0) goto Error;

	for (i = 0; i < Len; i++)
	{
		Buf[i] = MyI2C_ReceiveByte();
        if (i == Len - 1)
        {
            MyI2C_SendAck(1);
        }
        else
        {
            MyI2C_SendAck(0);
        }
	}
	MyI2C_Stop();
	return 0;

Error:
	DMP_I2CError = 1;
	MyI2C_Stop();
	return 1;
}

void MPU6050_Init(void)
{
	MyI2C_Init();
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1,0x01);
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2,0x00);
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV,0x09);
	MPU6050_WriteReg(MPU6050_CONFIG,0x06);
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG,0x18);
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG,0x18);
}

void MPU6050_GetData(int16_t *AccX,int16_t *AccY,int16_t *AccZ,
					int16_t *GyroX,int16_t *GyroY,int16_t *GyroZ)
{
	uint16_t DataH,DataL;
	
	DataH = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L);
	*AccX = (DataH <<8) | DataL;
	
	DataH = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L);
	*AccY = (DataH <<8) | DataL;
	
	DataH = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L);
	*AccZ = (DataH <<8) | DataL;
	
	DataH = MPU6050_ReadReg(MPU6050_GYRO_XOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_GYRO_XOUT_L);
	*GyroX = (DataH <<8) | DataL;
	
	DataH = MPU6050_ReadReg(MPU6050_GYRO_YOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_GYRO_YOUT_L);
	*GyroY = (DataH <<8) | DataL;
	
	DataH = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
	DataL = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
	*GyroZ = (DataH <<8) | DataL;
}

// 0 表示成功，非 0 表示失败步骤，供 OLED 显示 E01～E13。
uint8_t MPU6050_DMPInit(void)
{
    uint8_t ID;
    static const signed char Orientation[9] = {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };

    MyI2C_Init();
    DMP_I2CError = 0;
    if (MyI2C_RecoverBus()) return 1;
    if (MPU6050_Read_Len(0x68, MPU6050_WHO_AM_I, 1, &ID)) return 1;
    if ((ID & 0x7E) != 0x68) return 2;
    if (mpu_init() || DMP_I2CError) return 3;
    if (mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL) || DMP_I2CError) return 4;
    if (mpu_set_gyro_fsr(2000) || DMP_I2CError) return 5;
    if (mpu_set_accel_fsr(2) || DMP_I2CError) return 6;
    if (mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL) || DMP_I2CError) return 7;
    if (mpu_set_sample_rate(50) || DMP_I2CError) return 8;
    if (dmp_load_motion_driver_firmware() || DMP_I2CError) return 9;
    if (dmp_set_orientation(inv_orientation_matrix_to_scalar(Orientation)) || DMP_I2CError)
        return 10;
    if (dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT |
                           DMP_FEATURE_SEND_RAW_ACCEL |
                           DMP_FEATURE_SEND_CAL_GYRO |
                           DMP_FEATURE_GYRO_CAL) || DMP_I2CError)
        return 11;
    if (dmp_set_fifo_rate(50) || DMP_I2CError) return 12;
    if (mpu_set_dmp_state(1) || DMP_I2CError) return 13;
    return 0;
}

/* 0：读到角度；1：暂时没有新数据；2：通信或数据异常。
 * 最多取 4 包，保留最新一包，防止在主循环里无限追赶 FIFO。
 */
uint8_t MPU6050_ReadDMP(float *Pitch, float *Roll, float *Yaw)
{
    short Gyro[3], Accel[3], Sensors;
    long Quat[4];
    unsigned long Timestamp = 0;
    unsigned char More;
    uint8_t i, HasData = 0;
    float q0, q1, q2, q3, Norm, SinPitch;

    if (Pitch == 0 || Roll == 0 || Yaw == 0) return 2;
    DMP_I2CError = 0;
    for (i = 0; i < 4; i++)
    {
        if (dmp_read_fifo(Gyro, Accel, Quat, &Timestamp, &Sensors, &More))
        {
            if (DMP_I2CError) return 2;
            break;  // FIFO 空或者库已重置溢出的 FIFO，稍后再读。
        }
        if (!(Sensors & INV_WXYZ_QUAT)) return 2;
        HasData = 1;
        if (More == 0) break;
    }
    if (!HasData) return 1;

    q0 = Quat[0] / 1073741824.0f;
    q1 = Quat[1] / 1073741824.0f;
    q2 = Quat[2] / 1073741824.0f;
    q3 = Quat[3] / 1073741824.0f;
    Norm = q0*q0 + q1*q1 + q2*q2 + q3*q3;
    if (!(Norm > 0.5f && Norm < 1.5f)) return 2;
    Norm = sqrtf(Norm);
    q0 /= Norm;
    q1 /= Norm;
    q2 /= Norm;
    q3 /= Norm;

    SinPitch = 2.0f * (q0*q2 - q1*q3);
    if (SinPitch > 1.0f) SinPitch = 1.0f;
    if (SinPitch < -1.0f) SinPitch = -1.0f;
    *Pitch = asinf(SinPitch) * 57.29578f;
    *Roll = atan2f(2.0f*(q0*q1 + q2*q3),
                   1.0f - 2.0f*(q1*q1 + q2*q2)) * 57.29578f;
    *Yaw = atan2f(2.0f*(q0*q3 + q1*q2),
                  1.0f - 2.0f*(q2*q2 + q3*q3)) * 57.29578f;
    for (i = 0; i < 3; i++)
    {
        DMP_Accel[i] = Accel[i];
        DMP_Gyro[i] = Gyro[i];
    }
    return 0;
}

// 获取上一次成功读取的六轴数据，不再额外进行 I2C 通信。
void MPU6050_GetDMPData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
                       int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
    *AccX = DMP_Accel[0]; *AccY = DMP_Accel[1]; *AccZ = DMP_Accel[2];
    *GyroX = DMP_Gyro[0]; *GyroY = DMP_Gyro[1]; *GyroZ = DMP_Gyro[2];
}
