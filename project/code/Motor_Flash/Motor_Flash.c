#include "Motor_Flash.h"
#include "Motor_Control/Motor_Control.h"

/***********************************************
 * @brief : 初始化Flash并加载已保存的电机参数
 * @param : 无
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Motor_Flash_Init(void)
{
    flash_init();
    (void)Motor_Flash_Load();
}

/***********************************************
 * @brief : 从Flash加载并校验电机零点参数
 * @param : 无
 * @return: 1表示加载成功，0表示无有效参数
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
uint8 Motor_Flash_Load(void)
{
    uint32 Zero_offset;
    int32 Direction;
    uint32 Pole_pairs;

    flash_read_page_to_buffer(
        MOTOR_FLASH_SECTOR,
        MOTOR_FLASH_PAGE,
        MOTOR_FLASH_LENGTH);

    if (flash_union_buffer[MOTOR_FLASH_LENGTH - 1].uint32_type != MOTOR_FLASH_MAGIC)
    {
        Motor.Zero_ready = 0u;
        return 0u;
    }

    Zero_offset = flash_union_buffer[0].uint32_type;
    Direction = (int32)flash_union_buffer[1].uint32_type;
    Pole_pairs = flash_union_buffer[2].uint32_type;
    if ((Zero_offset > ANGLE_MAX) ||
        ((Direction != 1) && (Direction != -1)) ||
        (Pole_pairs == 0u) ||
        (Pole_pairs > 255u))
    {
        Motor.Zero_ready = 0u;
        return 0u;
    }

    Motor.Encoder.Zero_offset = (uint16)Zero_offset;
    Motor.Encoder.Direction = (int8)Direction;
    Motor.Pole_pairs = (uint8)Pole_pairs;
    Motor.Zero_ready = 1u;
    Angle_Update();

    return 1u;
}

/***********************************************
 * @brief : 保存当前电机零点参数
 * @param : 无
 * @return: 1表示保存成功，0表示保存失败
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
uint8 Motor_Flash_Save(void)
{
    if ((Motor.Encoder.Zero_offset > ANGLE_MAX) ||
        ((Motor.Encoder.Direction != 1) &&
         (Motor.Encoder.Direction != -1)) ||
        (Motor.Pole_pairs == 0u))
    {
        Motor.Zero_ready = 0u;
        return 0u;
    }

    flash_union_buffer[0].uint32_type =
        (uint32)Motor.Encoder.Zero_offset;
    flash_union_buffer[1].uint32_type =
        (uint32)(int32)Motor.Encoder.Direction;
    flash_union_buffer[2].uint32_type =
        (uint32)Motor.Pole_pairs;
    flash_union_buffer[MOTOR_FLASH_LENGTH - 1].uint32_type =
        MOTOR_FLASH_MAGIC;

    flash_erase_page(MOTOR_FLASH_SECTOR, MOTOR_FLASH_PAGE);

    return (flash_write_page_from_buffer(
                MOTOR_FLASH_SECTOR,
                MOTOR_FLASH_PAGE,
                MOTOR_FLASH_LENGTH) == 0u) ? 1u : 0u;
}
