#include "Motor_Flash.h"
#include "Function/Function.h"
#include "Motor_Control/Motor_Control.h"

void Motor_Flash_Read(void)
{
    uint32 ZeroOffset;
    int32 DirectionValue;
    uint32 PolePairsValue;

    Motor.ready = 0u;
    if (flash_check(MOTOR_FLASH_SECTOR, MOTOR_FLASH_PAGE) == 0u)
    {
        return;
    }

    flash_read_page_to_buffer(
        MOTOR_FLASH_SECTOR,
        MOTOR_FLASH_PAGE,
        MOTOR_FLASH_LENGTH);

    if (flash_union_buffer[MOTOR_FLASH_LENGTH - 1u].uint32_type !=
        MOTOR_FLASH_MAGIC)
    {
        return;
    }

    ZeroOffset = flash_union_buffer[0u].uint32_type;
    DirectionValue = flash_union_buffer[1u].int32_type;
    PolePairsValue = flash_union_buffer[2u].uint32_type;
    if ((ZeroOffset >= ANGLE_PERIOD) ||
        ((DirectionValue != 1) && (DirectionValue != -1)) ||
        (PolePairsValue == 0u) ||
        (PolePairsValue > MOTOR_ZERO_CALIBRATION_MAX_POLE_PAIRS))
    {
        return;
    }

    Motor.zero_offset = (uint16)ZeroOffset;
    Motor.direction = (int8)DirectionValue;
    Motor.pole_pairs = (uint8)PolePairsValue;
    Motor.ready = 1u;
}

uint8 Motor_Flash_Write(void)
{
    uint16 ZeroOffset;
    int8 Direction;
    uint8 PolePairs;

    ZeroOffset = Motor.zero_offset;
    Direction = Motor.direction;
    PolePairs = Motor.pole_pairs;
    Motor.ready = 0u;

    if (((uint32)ZeroOffset >= ANGLE_PERIOD) ||
        ((Direction != 1) && (Direction != -1)) ||
        (PolePairs == 0u) ||
        (PolePairs > MOTOR_ZERO_CALIBRATION_MAX_POLE_PAIRS))
    {
        return 1u;
    }

    flash_union_buffer[0u].uint32_type =
        (uint32)ZeroOffset;
    flash_union_buffer[1u].int32_type =
        (int32)Direction;
    flash_union_buffer[2u].uint32_type =
        (uint32)PolePairs;
    flash_union_buffer[3u].uint32_type = 0u;
    flash_union_buffer[MOTOR_FLASH_LENGTH - 1u].uint32_type =
        MOTOR_FLASH_MAGIC;

    flash_erase_page(MOTOR_FLASH_SECTOR, MOTOR_FLASH_PAGE);
    if (flash_write_page_from_buffer(
            MOTOR_FLASH_SECTOR,
            MOTOR_FLASH_PAGE,
            MOTOR_FLASH_LENGTH) != 0u)
    {
        return 1u;
    }

    flash_read_page_to_buffer(
        MOTOR_FLASH_SECTOR,
        MOTOR_FLASH_PAGE,
        MOTOR_FLASH_LENGTH);

    if ((flash_union_buffer[0u].uint32_type != (uint32)ZeroOffset) ||
        (flash_union_buffer[1u].int32_type != (int32)Direction) ||
        (flash_union_buffer[2u].uint32_type != (uint32)PolePairs) ||
        (flash_union_buffer[MOTOR_FLASH_LENGTH - 1u].uint32_type !=
         MOTOR_FLASH_MAGIC))
    {
        return 1u;
    }

    Motor.ready = 1u;
    return 0u;
}

void Motor_Flash_Init(void)
{
    flash_init();
    Motor_Flash_Read();
}
