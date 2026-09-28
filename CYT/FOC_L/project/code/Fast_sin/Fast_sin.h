#ifndef FAST_SIN_H
#define FAST_SIN_H

#include "zf_common_headfile.h"

#define FAST_SIN_QUARTER_COUNT    (512u)            /* 四分之一周期采样点数 */
#define FAST_SIN_SCALE            (10000)           /* 查表定标系数 */
#define FAST_SIN_STEP_SHIFT       (4u)              /* 每个表间隔对应16个角度单位 */
#define FAST_SIN_STEP_MASK        (0x0Fu)           /* 表间隔内的小数部分掩码 */
#define FAST_SIN_STEP_SIZE        (16)              /* 表间隔包含的角度单位数 */

/*==================================================== 基础函数 ====================================================*/
float   fast_sinf           (uint16 ElectricalAngle);
float   fast_cosf           (uint16 ElectricalAngle);
/*==================================================== 基础函数 ====================================================*/

#endif

