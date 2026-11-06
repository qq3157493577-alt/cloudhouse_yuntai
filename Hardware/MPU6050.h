#ifndef __MPU6050_H
#define __MPU6050_H

#include <stdint.h>

void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data);
uint8_t MPU6050_ReadReg(uint8_t RegAddress);


uint8_t MPU6050_Write_Len(uint8_t Addr, uint8_t RegAddress,
                          uint8_t Len, const uint8_t *Buf);
uint8_t MPU6050_Read_Len(uint8_t Addr, uint8_t RegAddress,
                         uint8_t Len, uint8_t *Buf);

void MPU6050_Init(void);
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
                     int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ);

// 初始化返回 0 成功，1～13 表示失败步骤。
uint8_t MPU6050_DMPInit(void);
// 0：新数据；1：暂无新数据；2：异常。角度单位为度。
uint8_t MPU6050_ReadDMP(float *Pitch, float *Roll, float *Yaw);
void MPU6050_GetDMPData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
                       int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ);

#endif
