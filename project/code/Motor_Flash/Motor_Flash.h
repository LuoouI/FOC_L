#ifndef MOTOR_FLASH_H
#define MOTOR_FLASH_H

#include "zf_common_headfile.h"

#define MOTOR_FLASH_SECTOR     (0)                  // 定义 FLASH 操作的扇区
#define MOTOR_FLASH_PAGE       (11)                 // 定义 FLASH 操作的页
#define MOTOR_FLASH_LENGTH     (5)                  // 保存到flash的长度（3个参数 + 1校验位 = 4，取5留余量）
#define MOTOR_FLASH_MAGIC      (0x4068u)            // 电机参数有效校验标记

/***********************************************
 * @brief : 初始化电机参数Flash并读取已保存参数
 * @param : /
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Motor_Flash_Init(void);

/***********************************************
 * @brief : 从Flash读取电机零点、方向和极对数
 * @param : /
 * @return: void
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
void Motor_Flash_Read(void);

/***********************************************
 * @brief : 将电机零点、方向和极对数写入Flash
 * @param : /
 * @return: 0写入并校验成功，1写入或校验失败
 * @date  : 2026-08-17
 * @author: LYF
 ************************************************/
uint8 Motor_Flash_Write(void);

#endif
