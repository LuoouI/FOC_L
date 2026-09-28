#ifndef MOTOR_FLASH_H
#define MOTOR_FLASH_H

#include "zf_common_headfile.h"

#define MOTOR_FLASH_SECTOR     (0)          /* 定义 FLASH 操作的扇区 */
#define MOTOR_FLASH_PAGE       (11)         /* 定义 FLASH 操作的页 */
#define MOTOR_FLASH_LENGTH     (5)          /* 保存3个电机参数及有效标记 */
#define MOTOR_FLASH_MAGIC      (0x4068u)    /* 电机参数有效校验标记 */

/*==================================================== 基础函数 ====================================================*/
void        Motor_Flash_Init            (void);
uint8       Motor_Flash_Load            (void);
uint8       Motor_Flash_Save            (void);
/*==================================================== 基础函数 ====================================================*/

#endif
