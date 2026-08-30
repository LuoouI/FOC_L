#ifndef MOTOR_FLASH_H
#define MOTOR_FLASH_H

#include "zf_common_headfile.h"

#define MOTOR_FLASH_SECTOR     (0)                  // 定义 FLASH 操作的扇区
#define MOTOR_FLASH_PAGE       (11)                 // 定义 FLASH 操作的页
#define MOTOR_FLASH_LENGTH     (5)                  // 保存3个电机参数及有效标记
#define MOTOR_FLASH_MAGIC      (0x4068u)            // 电机参数有效校验标记

/***********************************************
 * @brief : 初始化Flash并加载已保存的电机参数
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Motor_Flash_Init(void);

/***********************************************
 * @brief : 从Flash加载并校验电机零点参数
 * @param : 无
 * @return: 1表示加载成功，0表示无有效参数
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
uint8 Motor_Flash_Load(void);

/***********************************************
 * @brief : 保存当前电机零点参数
 * @param : 无
 * @return: 1表示保存成功，0表示保存失败
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
uint8 Motor_Flash_Save(void);

#endif
