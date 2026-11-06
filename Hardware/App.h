#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef struct {
    float Pitch, Roll;
    int16_t GX, GY, GZ;
    uint8_t Fresh, Healthy, InitError;
} AppAttitude;

void App_Init(uint32_t Now);
void App_Run(uint32_t Now, const AppAttitude *Attitude);
uint8_t App_GetMode(void);

#endif
