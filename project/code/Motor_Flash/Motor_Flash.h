#ifndef MOTOR_FLASH_H
#define MOTOR_FLASH_H

#include "zf_common_headfile.h"

#define MOTOR_FLASH_SECTOR     (0)                  // 定义 FLASH 操作的扇区
#define MOTOR_FLASH_PAGE       (11)                 // 定义 FLASH 操作的页
#define MOTOR_FLASH_LENGTH     (5)                  // 保存到flash的长度（3个参数 + 1校验位 = 4，取5留余量）
#define MOTOR_FLASH_MAGIC      (0x4068u)            // 电机参数有效校验标记

#endif
