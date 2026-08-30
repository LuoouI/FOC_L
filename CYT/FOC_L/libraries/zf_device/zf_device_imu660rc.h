/*********************************************************************************************************************
* RT1064DVL6A Opensourec Library 即（RT1064DVL6A 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2022 SEEKFREE 逐飞科技
*
* 本文件是 RT1064DVL6A 开源库的一部分
*
* RT1064DVL6A 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
*
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
*
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
*
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
*
* 文件名称          zf_device_imu660rc
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          IAR 8.32.4 or MDK 5.38
* 适用平台          RT1064DVL6A
* 店铺链接          https://seekfree.taobao.com/
*
* 修改记录
* 日期              作者                备注
* 2025-12-12        SeekFree            first version
********************************************************************************************************************/
/*********************************************************************************************************************
* 接线定义：
*                   ------------------------------------
*                   模块管脚            单片机管脚
*                   // 硬件 SPI 引脚
*                   SCL/SPC           查看 zf_device_imu660rc.h 中 IMU660RC_SPC_PIN 宏定义
*                   SDA/DSI           查看 zf_device_imu660rc.h 中 IMU660RC_SDI_PIN 宏定义
*                   SA0/SDO           查看 zf_device_imu660rc.h 中 IMU660RC_SDO_PIN 宏定义
*                   CS                查看 zf_device_imu660rc.h 中 IMU660RC_CS_PIN 宏定义
*                   INT2              查看 zf_device_imu660rc.h 中 IMU660RC_INT2_PIN 宏定义
*                   VCC               3.3V电源
*                   GND               电源地
*                   其余引脚悬空
*
*                   // 软件 IIC 引脚
*                   SCL/SPC           查看 zf_device_imu660rc.h 中 IMU660RC_SCL_PIN 宏定义
*                   SDA/DSI           查看 zf_device_imu660rc.h 中 IMU660RC_SDA_PIN 宏定义
*                   VCC               3.3V电源
*                   GND               电源地
*                   其余引脚悬空
*                   ------------------------------------
********************************************************************************************************************/


#ifndef _zf_device_imu660rc_h_
#define _zf_device_imu660rc_h_

#include "zf_common_typedef.h"


// IMU660RC_USE_SOFT_IIC定义为0表示使用硬件SPI驱动 定义为1表示使用软件IIC驱动
// 当更改IMU660RC_USE_SOFT_IIC定义后，需要先编译并下载程序，单片机与模块需要断电重启才能正常通讯
#define IMU660RC_USE_SOFT_IIC           ( 0 )                                   // 默认使用硬件 SPI 方式驱动

#if IMU660RC_USE_SOFT_IIC                                                       // 这两段 颜色正常的才是正确的 颜色灰的就是没有用的
//====================================================软件 IIC 驱动====================================================
#define IMU660RC_SOFT_IIC_DELAY         ( 59 )                                 // 软件 IIC 的时钟延时周期 数值越小 IIC 通信速率越快
#define IMU660RC_SCL_PIN                ( P14_2 )                                 // 软件 IIC SCL 引脚 连接 IMU660RC 的 SCL 引脚
#define IMU660RC_SDA_PIN                ( P14_1 )                                 // 软件 IIC SDA 引脚 连接 IMU660RC 的 SDA 引脚
//====================================================软件 IIC 驱动====================================================
#else

//====================================================硬件 SPI 驱动====================================================
#define IMU660RC_SPI_SPEED              (10 * 1000 * 1000)                      // 硬件 SPI 速率
#define IMU660RC_SPI                    (SPI_4)                                 // 硬件 SPI 号
#define IMU660RC_SPC_PIN                (SPI4_CLK_P14_2 )                         // 硬件 SPI SCK 引脚
#define IMU660RC_SDI_PIN                (SPI4_MOSI_P14_1)                         // 硬件 SPI MOSI 引脚
#define IMU660RC_SDO_PIN                (SPI4_MISO_P14_0)                         // 硬件 SPI MISO 引脚
//====================================================硬件 SPI 驱动====================================================
#endif
#define IMU660RC_CS_PIN                 ( P18_0 )                                 // CS 片选引脚
#define IMU660RC_CS(x)                  ( (x) ? (gpio_high(IMU660RC_CS_PIN)) : (gpio_low(IMU660RC_CS_PIN)) )
#define IMU660RC_INT2_PIN               ( P18_5 )                                 // 中断信号引脚，在读取四元数时需要使用


#define IMU660RC_QUARTERNION_GET_GYRO   ( 1 )                                   // 1：在输出四元数的模式时，读取四元数时自动读取角速度 0：不自动读取
#define IMU660RC_QUARTERNION_GET_ACC    ( 1 )                                   // 1：在输出四元数的模式时，读取四元数时自动读取加速度 0：不自动读取
#define IMU660RC_ACC_SAMPLE_DEFAULT     ( IMU660RC_ACC_SAMPLE_SGN_8G )          // 在这设置默认的 加速度计 初始化量程
#define IMU660RC_GYRO_SAMPLE_DEFAULT    ( IMU660RC_GYRO_SAMPLE_SGN_2000DPS )    // 在这设置默认的 陀螺仪   初始化量程

typedef enum
{
    IMU660RC_MAIN_MEM_BANK  = 0x00,
    IMU660RC_HUB_MEM_BANK   = 0x40,
    IMU660RC_EMBED_MEM_BANK = 0x80,
}imu660rc_mem_bank_enum;

typedef enum
{
    IMU660RC_ACC_SAMPLE_SGN_2G ,                                                // 加速度计量程 ±2G  (ACC = Accelerometer 加速度计) (SGN = signum 带符号数 表示正负范围) (G = g 重力加速度 g≈9.80 m/s^2)
    IMU660RC_ACC_SAMPLE_SGN_4G ,                                                // 加速度计量程 ±4G
    IMU660RC_ACC_SAMPLE_SGN_8G ,                                                // 加速度计量程 ±8G
    IMU660RC_ACC_SAMPLE_SGN_16G,                                                // 加速度计量程 ±16G
}imu660rc_acc_sample_config;

typedef enum
{
    IMU660RC_GYRO_SAMPLE_SGN_125DPS ,                                           // 陀螺仪量程 ±125DPS  (GYRO = Gyroscope 陀螺仪) (SGN = signum 带符号数 表示正负范围) (DPS = Degree Per Second 角速度单位 °/S)
    IMU660RC_GYRO_SAMPLE_SGN_250DPS ,                                           // 陀螺仪量程 ±250DPS
    IMU660RC_GYRO_SAMPLE_SGN_500DPS ,                                           // 陀螺仪量程 ±500DPS
    IMU660RC_GYRO_SAMPLE_SGN_1000DPS,                                           // 陀螺仪量程 ±1000DPS
    IMU660RC_GYRO_SAMPLE_SGN_2000DPS,                                           // 陀螺仪量程 ±2000DPS
    IMU660RC_GYRO_SAMPLE_SGN_4000DPS,                                           // 陀螺仪量程 ±4000DPS
}imu660rc_gyro_sample_config;

typedef enum
{
    IMU660RC_QUARTERNION_15HZ,                                                  // 15 Hz
    IMU660RC_QUARTERNION_30HZ,                                                  // 30 Hz
    IMU660RC_QUARTERNION_60HZ,                                                  // 60 Hz
    IMU660RC_QUARTERNION_120HZ,                                                 // 120Hz
    IMU660RC_QUARTERNION_240HZ,                                                 // 240Hz
    IMU660RC_QUARTERNION_480HZ,                                                 // 480Hz
    IMU660RC_QUARTERNION_DISABLE,                                               // 禁用四元数输出
}imu660rc_quarternion_rate_config;


//================================================定义 IMU660RC 内部地址================================================
#define IMU660RC_DEV_ADDR           ( 0x6B )                                    // SA0接地：0x6A SA0上拉：0x6B 模块默认上拉
#define IMU660RC_SPI_W              ( 0x00 )
#define IMU660RC_SPI_R              ( 0x80 )
#define IMU660RC_TIMEOUT_COUNT      ( 0x00FF )                                  // IMU660RC 超时计数


//================================================定义 IMU660RC 寄存器地址================================================
#define IMU660RC_FUNC_CFG_ACCESS    ( 0x01 )
#define IMU660RC_INT2_CTRL          ( 0x0E )
#define IMU660RC_CHIP_ID            ( 0x0F )
#define IMU660RC_CTRL1              ( 0x10 )
#define IMU660RC_CTRL2              ( 0x11 )
#define IMU660RC_CTRL3              ( 0x12 )
#define IMU660RC_CTRL4              ( 0x13 )
#define IMU660RC_CTRL5              ( 0x14 )
#define IMU660RC_CTRL6              ( 0x15 )
#define IMU660RC_CTRL7              ( 0x16 )
#define IMU660RC_CTRL8              ( 0x17 )
#define IMU660RC_CTRL9              ( 0x18 )
#define IMU660RC_CTRL10             ( 0x19 )
#define IMU660RC_CTRL_STATUS        ( 0x1A )
#define IMU660RC_STATUS_REG         ( 0x1E )
#define IMU660RC_OUT_TEMP_L         ( 0x20 )
#define IMU660RC_OUT_TEMP_H         ( 0x21 )
#define IMU660RC_OUTX_L_G           ( 0x22 )
#define IMU660RC_OUTX_H_G           ( 0x23 )
#define IMU660RC_OUTY_L_G           ( 0x24 )
#define IMU660RC_OUTY_H_G           ( 0x25 )
#define IMU660RC_OUTZ_L_G           ( 0x26 )
#define IMU660RC_OUTZ_H_G           ( 0x27 )
#define IMU660RC_OUTX_L_A           ( 0x28 )
#define IMU660RC_OUTX_H_A           ( 0x29 )
#define IMU660RC_OUTY_L_A           ( 0x2A )
#define IMU660RC_OUTY_H_A           ( 0x2B )
#define IMU660RC_OUTZ_L_A           ( 0x2C )
#define IMU660RC_OUTZ_H_A           ( 0x2D )

#define IMU660RC_PAGE_SEL           ( 0x02 )
#define IMU660RC_EMB_FUNC_EN_A      ( 0x04 )
#define IMU660RC_PAGE_RW            ( 0x17 )      
#define IMU660RC_SFLP_ODR           ( 0x5E )
#define IMU660RC_EMB_FUNC_CFG       ( 0x63 )



extern float imu660rc_transition_factor[2];
extern int16 imu660rc_gyro_x,   imu660rc_gyro_y,    imu660rc_gyro_z;    // 三轴陀螺仪数据
extern int16 imu660rc_acc_x ,   imu660rc_acc_y ,    imu660rc_acc_z;     // 三轴加速度计数据
extern float imu660rc_roll  ,   imu660rc_pitch ,    imu660rc_yaw;       // 欧拉角
extern float imu660rc_quarternion[4];                                   // 四元数


void    imu660rc_get_acc            (void);
void    imu660rc_get_gyro           (void);
void    imu660rc_get_quarternion    (void);

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     将 IMU660RC 加速度计数据转换为实际物理数据
// 参数说明     acc_value       任意轴的加速度计数据
// 返回参数     void
// 使用示例     float data = imu660rc_acc_transition(imu660rc_acc_x);           // 单位为 g(m/s^2)
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
#define imu660rc_acc_transition(acc_value)      ((float)(acc_value) / imu660rc_transition_factor[0])

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     将 IMU660RC 陀螺仪数据转换为实际物理数据
// 参数说明     gyro_value      任意轴的陀螺仪数据
// 返回参数     void
// 使用示例     float data = imu660rc_gyro_transition(imu660rc_gyro_x);         // 单位为 °/s
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
#define imu660rc_gyro_transition(gyro_value)    ((float)(gyro_value) / imu660rc_transition_factor[1])
void    imu660rc_callback           (void);
uint8   imu660rc_init               (imu660rc_quarternion_rate_config quarternion_rate);



#endif
