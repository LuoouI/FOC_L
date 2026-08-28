#include "FOC_Voice.h"
#include "Fast_sin/Fast_sin.h"
#include "Function/Function.h"
#include "Motor_Control/Motor_Control.h"
#include "My_TCPWM/My_TCPWM.h"

/*===========================================================================*/
/*  《奇迹再现》主歌，1=A                                                     */
/*===========================================================================*/
static const FOC_VoiceNote_t Miracle_verse[] =
{
    /* 第1~4小节 */
    {FOC_VOICE_PITCH_REST, 2u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_FS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_REST, 2u},
    {FOC_VOICE_PITCH_B4, 2u},   {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_GS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 3u},
    {FOC_VOICE_PITCH_REST, 2u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_FS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_REST, 2u},
    {FOC_VOICE_PITCH_CS5, 2u},  {FOC_VOICE_PITCH_B4, 1u},
    {FOC_VOICE_PITCH_B4, 1u},   {FOC_VOICE_PITCH_B4, 1u},
    {FOC_VOICE_PITCH_CS5, 3u},

    /* 第5~8小节 */
    {FOC_VOICE_PITCH_REST, 2u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_FS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_REST, 2u},
    {FOC_VOICE_PITCH_CS5, 2u},  {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_B4, 3u},
    {FOC_VOICE_PITCH_GS4, 8u},
    {FOC_VOICE_PITCH_F5, 1u},   {FOC_VOICE_PITCH_C5, 1u},
    {FOC_VOICE_PITCH_CS5, 1u},  {FOC_VOICE_PITCH_G5, 1u},
    {FOC_VOICE_PITCH_GS4, 1u},  {FOC_VOICE_PITCH_CS5, 1u},
    {FOC_VOICE_PITCH_FS5, 1u},  {FOC_VOICE_PITCH_FS5, 1u},

    /* 第9~12小节 */
    {FOC_VOICE_PITCH_REST, 2u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_FS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_REST, 2u},
    {FOC_VOICE_PITCH_B4, 2u},   {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_GS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 3u},
    {FOC_VOICE_PITCH_REST, 2u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_FS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_REST, 2u},
    {FOC_VOICE_PITCH_CS5, 2u},  {FOC_VOICE_PITCH_B4, 1u},
    {FOC_VOICE_PITCH_B4, 1u},   {FOC_VOICE_PITCH_B4, 1u},
    {FOC_VOICE_PITCH_CS5, 3u},

    /* 第13~16小节 */
    {FOC_VOICE_PITCH_REST, 2u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_FS4, 1u},  {FOC_VOICE_PITCH_GS4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_REST, 2u},
    {FOC_VOICE_PITCH_CS5, 2u},  {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_A4, 1u},   {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_B4, 3u},
    {FOC_VOICE_PITCH_GS4, 2u},  {FOC_VOICE_PITCH_E5, 1u},
    {FOC_VOICE_PITCH_B4, 1u},   {FOC_VOICE_PITCH_FS5, 1u},
    {FOC_VOICE_PITCH_B4, 1u},   {FOC_VOICE_PITCH_CS6, 2u},
    {FOC_VOICE_PITCH_CS6, 6u},  {FOC_VOICE_PITCH_CS4, 1u},
    {FOC_VOICE_PITCH_DS4, 1u}
};

static const FOC_VoiceSection_t Miracle_song[] =
{
    {Miracle_verse, (uint16)(sizeof(Miracle_verse) / sizeof(Miracle_verse[0]))}
};

/*===========================================================================*/
/*  《小星星》旋律                                                            */
/*===========================================================================*/
static const FOC_VoiceNote_t Twinkle_verse[] =
{
    {FOC_VOICE_PITCH_C4, 2u}, {FOC_VOICE_PITCH_C4, 2u},
    {FOC_VOICE_PITCH_G4, 2u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_A4, 2u}, {FOC_VOICE_PITCH_A4, 2u},
    {FOC_VOICE_PITCH_G4, 4u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_D4, 2u}, {FOC_VOICE_PITCH_D4, 2u},
    {FOC_VOICE_PITCH_C4, 4u},
    {FOC_VOICE_PITCH_G4, 2u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_D4, 4u},
    {FOC_VOICE_PITCH_G4, 2u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_D4, 4u},
    {FOC_VOICE_PITCH_C4, 2u}, {FOC_VOICE_PITCH_C4, 2u},
    {FOC_VOICE_PITCH_G4, 2u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_A4, 2u}, {FOC_VOICE_PITCH_A4, 2u},
    {FOC_VOICE_PITCH_G4, 4u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_D4, 2u}, {FOC_VOICE_PITCH_D4, 2u},
    {FOC_VOICE_PITCH_C4, 4u}
};

static const FOC_VoiceSection_t Twinkle_song[] =
{
    {Twinkle_verse, (uint16)(sizeof(Twinkle_verse) / sizeof(Twinkle_verse[0]))}
};

/*===========================================================================*/
/*  《欢乐颂》旋律                                                            */
/*===========================================================================*/
static const FOC_VoiceNote_t Ode_verse[] =
{
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_G4, 2u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_D4, 2u},
    {FOC_VOICE_PITCH_C4, 2u}, {FOC_VOICE_PITCH_C4, 2u},
    {FOC_VOICE_PITCH_D4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_E4, 3u}, {FOC_VOICE_PITCH_D4, 1u},
    {FOC_VOICE_PITCH_D4, 4u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_G4, 2u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_D4, 2u},
    {FOC_VOICE_PITCH_C4, 2u}, {FOC_VOICE_PITCH_C4, 2u},
    {FOC_VOICE_PITCH_D4, 2u}, {FOC_VOICE_PITCH_E4, 2u},
    {FOC_VOICE_PITCH_D4, 3u}, {FOC_VOICE_PITCH_C4, 1u},
    {FOC_VOICE_PITCH_C4, 4u}
};

static const FOC_VoiceSection_t Ode_song[] =
{
    {Ode_verse, (uint16)(sizeof(Ode_verse) / sizeof(Ode_verse[0]))}
};

static const FOC_VoiceSong_t Voice_songs[] =
{
    {
        Miracle_song,
        FOC_VOICE_MIRACLE_BPM,
        (uint8)(sizeof(Miracle_song) / sizeof(Miracle_song[0]))
    },
    {
        Twinkle_song,
        FOC_VOICE_TWINKLE_BPM,
        (uint8)(sizeof(Twinkle_song) / sizeof(Twinkle_song[0]))
    },
    {
        Ode_song,
        FOC_VOICE_ODE_BPM,
        (uint8)(sizeof(Ode_song) / sizeof(Ode_song[0]))
    }
};

static volatile FOC_Voice_t Voice =
{
    .Note_elapsed = 0u,
    .Note_total = 0u,
    .Gate_count = 0u,
    .Song_elapsed = 0u,
    .Tone_phase = 0u,
    .Tone_step = 0u,
    .Note_index = 0u,
    .Song_duration8th = 0u,
    .Section_index = 0u,
    .Song_id = FOC_VOICE_SONG_ID,
    .Playing = 0u
};

/***********************************************
 * @brief : 输出三相中点电压并停止发声
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Voice_OutputNeutral(void)
{
    My_TCPWM_SetDuty(
        (uint16)(TCPWM_DUTY_MAX / 2u),
        (uint16)(TCPWM_DUTY_MAX / 2u),
        (uint16)(TCPWM_DUTY_MAX / 2u));
}

/***********************************************
 * @brief : 完成播放状态并恢复停止模式
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Voice_Finish(void)
{
    Voice.Playing = 0u;
    Voice.Gate_count = 0u;
    Voice.Tone_phase = 0u;
    Voice.Tone_step = 0u;

    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    FOC_Voice_OutputNeutral();
}

/***********************************************
 * @brief : 装载当前段落中的音符参数
 * @param : 无
 * @return: 1表示装载成功，0表示乐曲播放完成
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static uint8 FOC_Voice_LoadNote(void)
{
    const FOC_VoiceSong_t *Song;
    const FOC_VoiceSection_t *Section;
    const FOC_VoiceNote_t *Note;
    uint32 Duration_count;

    Song = &Voice_songs[Voice.Song_id - 1u];

    while (Voice.Section_index < Song->Section_count)
    {
        Section = &Song->Section_data[Voice.Section_index];
        if (Voice.Note_index < Section->Note_count)
        {
            break;
        }

        Voice.Section_index++;
        Voice.Note_index = 0u;
    }

    if (Voice.Section_index >= Song->Section_count)
    {
        FOC_Voice_Finish();
        return 0u;
    }

    Section = &Song->Section_data[Voice.Section_index];
    Note = &Section->Note_data[Voice.Note_index];

    Voice.Song_duration8th += Note->Duration_8th;
    Duration_count =
        (uint32)Voice.Song_duration8th * FOC_VOICE_CONTROL_HZ * 30u /
        Song->Bpm - Voice.Song_elapsed;

    Voice.Note_elapsed = 0u;
    Voice.Note_total = Duration_count;
    Voice.Tone_phase = 0u;
    Voice.Tone_step = (uint16)(
        ((uint32)Note->Pitch * ANGLE_PERIOD +
         (FOC_VOICE_CONTROL_HZ / 2u)) /
        FOC_VOICE_CONTROL_HZ);

    if (Note->Pitch == FOC_VOICE_PITCH_REST)
    {
        Voice.Gate_count = 0u;
    }
    else
    {
        Voice.Gate_count =
            Duration_count * FOC_VOICE_GATE_PERCENT / 100u;
    }

    return 1u;
}

/***********************************************
 * @brief : 计算当前音符的起音和释音包络
 * @param : 无
 * @return: 音量包络，范围0~1
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static float FOC_Voice_GetEnvelope(void)
{
    uint32 Release_start;
    float Envelope;

    if ((Voice.Gate_count == 0u) ||
        (Voice.Note_elapsed >= Voice.Gate_count))
    {
        return 0.0f;
    }

    Envelope = 1.0f;
    if (Voice.Note_elapsed < FOC_VOICE_RAMP_COUNT)
    {
        Envelope =
            (float)Voice.Note_elapsed / (float)FOC_VOICE_RAMP_COUNT;
    }

    Release_start = (Voice.Gate_count > FOC_VOICE_RAMP_COUNT) ?
                    (Voice.Gate_count - FOC_VOICE_RAMP_COUNT) : 0u;
    if (Voice.Note_elapsed > Release_start)
    {
        Envelope = Float_Limit(
            (float)(Voice.Gate_count - Voice.Note_elapsed) /
            (float)FOC_VOICE_RAMP_COUNT,
            0.0f,
            Envelope);
    }

    return Envelope;
}

/***********************************************
 * @brief : 将音频正弦信号转换为两相差分占空比
 * @param : Envelope 当前音量包络，范围0~1
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void FOC_Voice_Output(float Envelope)
{
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;
    int32 Tone_duty;

    Tone_duty = (int32)(
        Envelope * fast_sinf(Voice.Tone_phase) *
        (float)FOC_VOICE_DUTY_AMPLITUDE);

    /* A、B两相反向变化形成线间音频电压，C相保持中点占空比。 */
    DutyA = (uint16)Int_Limit(
        (int32)(TCPWM_DUTY_MAX / 2u) + Tone_duty,
        0,
        (int32)TCPWM_DUTY_MAX);
    DutyB = (uint16)Int_Limit(
        (int32)(TCPWM_DUTY_MAX / 2u) - Tone_duty,
        0,
        (int32)TCPWM_DUTY_MAX);
    DutyC = (uint16)(TCPWM_DUTY_MAX / 2u);

    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);
}

void FOC_Voice_Start(void)
{
    (void)FOC_Voice_StartSong(FOC_VOICE_SONG_ID);
}

uint8 FOC_Voice_StartSong(uint8 Song_id)
{
    if ((Song_id == 0u) || (Song_id > FOC_VOICE_SONG_COUNT))
    {
        return 0u;
    }

    Voice.Note_elapsed = 0u;
    Voice.Note_total = 0u;
    Voice.Gate_count = 0u;
    Voice.Song_elapsed = 0u;
    Voice.Tone_phase = 0u;
    Voice.Tone_step = 0u;
    Voice.Note_index = 0u;
    Voice.Song_duration8th = 0u;
    Voice.Section_index = 0u;
    Voice.Song_id = Song_id;
    Voice.Playing = 1u;

    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Open_loop.Hold_count = 0u;
    Motor.Open_loop.Started = 0u;
    Motor.Control_mode = MOTOR_CONTROL_VOICE;

    return 1u;
}

void FOC_Voice_Stop(void)
{
    if ((Voice.Playing != 0u) ||
        (Motor.Control_mode == MOTOR_CONTROL_VOICE))
    {
        FOC_Voice_Finish();
    }
}

uint8 FOC_Voice_IsPlaying(void)
{
    return Voice.Playing;
}

void FOC_Voice_Loop(void)
{
    float Envelope;

    if (Voice.Playing == 0u)
    {
        FOC_Voice_Finish();
        return;
    }

    if (Voice.Note_total == 0u)
    {
        if (FOC_Voice_LoadNote() == 0u)
        {
            return;
        }
    }
    else if (Voice.Note_elapsed >= Voice.Note_total)
    {
        Voice.Note_index++;
        if (FOC_Voice_LoadNote() == 0u)
        {
            return;
        }
    }

    Envelope = FOC_Voice_GetEnvelope();
    FOC_Voice_Output(Envelope);

    Voice.Tone_phase = Angle_Wrap(
        (int32)Voice.Tone_phase + (int32)Voice.Tone_step);
    Voice.Note_elapsed++;
    Voice.Song_elapsed++;
}
