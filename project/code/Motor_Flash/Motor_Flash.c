#include "Motor_Flash.h"
#include "Motor_Control/Motor_Control.h"

void Motor_Flash_Init(void)
{
    flash_init();
    (void)Motor_Flash_Load();
}

uint8 Motor_Flash_Load(void)
{
    flash_read_page_to_buffer(
        MOTOR_FLASH_SECTOR,
        MOTOR_FLASH_PAGE,
        MOTOR_FLASH_LENGTH);

    if (flash_union_buffer[MOTOR_FLASH_LENGTH - 1].uint32_type != MOTOR_FLASH_MAGIC)
    {
        Motor.Zero_ready = 0u;
        return 0u;
    }

    Motor.Encoder.Zero_offset =
        (uint16)flash_union_buffer[0].uint32_type;
    Motor.Encoder.Direction =
        (int8)(int32)flash_union_buffer[1].uint32_type;
    Motor.Pole_pairs =
        (uint8)flash_union_buffer[2].uint32_type;
    Motor.Zero_ready = 1u;
    Angle_Update();

    return 1u;
}

uint8 Motor_Flash_Save(void)
{
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
