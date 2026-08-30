#include "Foc_voice.h"
#include "Fast_sin/Fast_sin.h"
#include "Function/Function.h"
#include "Motor_Control/Motor_Control.h"
#include "My_TCPWM/My_TCPWM.h"
#include "SVPWM/SVPWM.h"

/*===========================================================================*/
/*  《奇迹再现》主歌，1=A                                                     */
/*===========================================================================*/
static const Foc_voiceNote_t Miracle_verse[] =
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

static const Foc_voiceSection_t Miracle_song[] =
{
    {Miracle_verse, (uint16)(sizeof(Miracle_verse) / sizeof(Miracle_verse[0]))}
};

/*===========================================================================*/
/*  《小星星》旋律                                                            */
/*===========================================================================*/
static const Foc_voiceNote_t Twinkle_verse[] =
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

static const Foc_voiceSection_t Twinkle_song[] =
{
    {Twinkle_verse, (uint16)(sizeof(Twinkle_verse) / sizeof(Twinkle_verse[0]))}
};

/*===========================================================================*/
/*  《欢乐颂》旋律                                                            */
/*===========================================================================*/
static const Foc_voiceNote_t Ode_verse[] =
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

static const Foc_voiceSection_t Ode_song[] =
{
    {Ode_verse, (uint16)(sizeof(Ode_verse) / sizeof(Ode_verse[0]))}
};

/*===========================================================================*/
/*  《天使的翅膀》指定副歌片段，移调为1=F                                    */
/*===========================================================================*/
static const Foc_voiceNote_t Angel_verse[] =
{
    /* 第一乐句 */
    {FOC_VOICE_PITCH_G4, 1u}, {FOC_VOICE_PITCH_A4, 1u},
    {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_D4, 2u}, {FOC_VOICE_PITCH_D4, 1u},
    {FOC_VOICE_PITCH_A4, 1u}, {FOC_VOICE_PITCH_G4, 2u},
    {FOC_VOICE_PITCH_F4, 1u}, {FOC_VOICE_PITCH_E4, 1u},
    {FOC_VOICE_PITCH_D4, 4u}, {FOC_VOICE_PITCH_REST, 2u},

    /* 第二乐句 */
    {FOC_VOICE_PITCH_F4, 1u}, {FOC_VOICE_PITCH_G4, 1u},
    {FOC_VOICE_PITCH_A4, 2u}, {FOC_VOICE_PITCH_C5, 2u},
    {FOC_VOICE_PITCH_A4, 1u}, {FOC_VOICE_PITCH_G4, 1u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_G4, 1u},
    {FOC_VOICE_PITCH_A4, 1u}, {FOC_VOICE_PITCH_F4, 4u},
    {FOC_VOICE_PITCH_REST, 2u},

    /* 结束乐句 */
    {FOC_VOICE_PITCH_A4, 1u}, {FOC_VOICE_PITCH_G4, 1u},
    {FOC_VOICE_PITCH_F4, 2u}, {FOC_VOICE_PITCH_D4, 2u},
    {FOC_VOICE_PITCH_F4, 1u}, {FOC_VOICE_PITCH_G4, 1u},
    {FOC_VOICE_PITCH_A4, 2u}, {FOC_VOICE_PITCH_G4, 1u},
    {FOC_VOICE_PITCH_E4, 1u}, {FOC_VOICE_PITCH_F4, 2u},
    {FOC_VOICE_PITCH_E4, 2u}, {FOC_VOICE_PITCH_D4, 8u}
};

static const Foc_voiceSection_t Angel_song[] =
{
    {Angel_verse, (uint16)(sizeof(Angel_verse) / sizeof(Angel_verse[0]))}
};

static const Foc_voiceSong_t Voice_songs[] =
{
    {
        "奇迹再现",
        Miracle_song,
        FOC_VOICE_MIRACLE_BPM,
        (uint8)(sizeof(Miracle_song) / sizeof(Miracle_song[0]))
    },
    {
        "小星星",
        Twinkle_song,
        FOC_VOICE_TWINKLE_BPM,
        (uint8)(sizeof(Twinkle_song) / sizeof(Twinkle_song[0]))
    },
    {
        "欢乐颂",
        Ode_song,
        FOC_VOICE_ODE_BPM,
        (uint8)(sizeof(Ode_song) / sizeof(Ode_song[0]))
    },
    {
        "天使的翅膀（片段）",
        Angel_song,
        FOC_VOICE_ANGEL_BPM,
        (uint8)(sizeof(Angel_song) / sizeof(Angel_song[0]))
    }
};

static volatile Foc_voice_t Voice =
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
static void Foc_voice_OutputNeutral(void)
{
    My_TCPWM_SetDuty(
        (uint16)(TCPWM_DUTY_MAX / 2u),
        (uint16)(TCPWM_DUTY_MAX / 2u),
        (uint16)(TCPWM_DUTY_MAX / 2u));
    SVPWM_DutyCache_Update(
        (uint16)(SVPWM_DUTY_MAX / 2u),
        (uint16)(SVPWM_DUTY_MAX / 2u),
        (uint16)(SVPWM_DUTY_MAX / 2u));
}

/***********************************************
 * @brief : 完成播放状态并恢复停止模式
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static void Foc_voice_Finish(void)
{
    Voice.Playing = 0u;
    Voice.Gate_count = 0u;
    Voice.Tone_phase = 0u;
    Voice.Tone_step = 0u;

    Motor.Open_loop.Uq = 0.0f;
    Motor.Open_loop.Step = 0;
    Motor.Control_mode = MOTOR_CONTROL_OPEN_LOOP;
    Foc_voice_OutputNeutral();
}

/***********************************************
 * @brief : 装载当前段落中的音符参数
 * @param : 无
 * @return: 1表示装载成功，0表示乐曲播放完成
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
static uint8 Foc_voice_LoadNote(void)
{
    const Foc_voiceSong_t *Song;
    const Foc_voiceSection_t *Section;
    const Foc_voiceNote_t *Note;
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
        Foc_voice_Finish();
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
 * @brief : 根据音符进度计算起音和释音包络
 * @param : Note_elapsed 音符已执行控制周期数
 * @param : Gate_count 音符有效发声控制周期数
 * @return: 音量包络，范围0~1
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static float Foc_voice_GetEnvelope(
    uint32 Note_elapsed,
    uint32 Gate_count)
{
    uint32 Release_start;
    float Envelope;

    if ((Gate_count == 0u) ||
        (Note_elapsed >= Gate_count))
    {
        return 0.0f;
    }

    Envelope = 1.0f;
    if (Note_elapsed < FOC_VOICE_RAMP_COUNT)
    {
        Envelope =
            (float)Note_elapsed / (float)FOC_VOICE_RAMP_COUNT;
    }

    Release_start = (Gate_count > FOC_VOICE_RAMP_COUNT) ?
                    (Gate_count - FOC_VOICE_RAMP_COUNT) : 0u;
    if (Note_elapsed > Release_start)
    {
        Envelope = Float_Limit(
            (float)(Gate_count - Note_elapsed) /
            (float)FOC_VOICE_RAMP_COUNT,
            0.0f,
            Envelope);
    }

    return Envelope;
}

/***********************************************
 * @brief : 将音频正弦信号转换为指定相序的差分占空比
 * @param : Envelope 当前音量包络，范围0~1
 * @param : Tone_phase 当前音频正弦相位
 * @param : Phase 主发声相
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
static void Foc_voice_Output(
    float Envelope,
    uint16 Tone_phase,
    Foc_voicePhase_t Phase)
{
    uint16 DutyA;
    uint16 DutyB;
    uint16 DutyC;
    int32 Neutral_duty;
    int32 Tone_duty;

    Neutral_duty = (int32)(TCPWM_DUTY_MAX / 2u);
    Tone_duty = (int32)(
        Envelope * fast_sinf(Tone_phase) *
        (float)FOC_VOICE_DUTY_AMPLITUDE);

    DutyA = (uint16)Neutral_duty;
    DutyB = (uint16)Neutral_duty;
    DutyC = (uint16)Neutral_duty;

    switch (Phase)
    {
        case FOC_VOICE_PHASE_B:
            DutyB = (uint16)Int_Limit(
                Neutral_duty + Tone_duty,
                0,
                (int32)TCPWM_DUTY_MAX);
            DutyC = (uint16)Int_Limit(
                Neutral_duty - Tone_duty,
                0,
                (int32)TCPWM_DUTY_MAX);
            break;

        case FOC_VOICE_PHASE_C:
            DutyC = (uint16)Int_Limit(
                Neutral_duty + Tone_duty,
                0,
                (int32)TCPWM_DUTY_MAX);
            DutyA = (uint16)Int_Limit(
                Neutral_duty - Tone_duty,
                0,
                (int32)TCPWM_DUTY_MAX);
            break;

        case FOC_VOICE_PHASE_A:
        default:
            DutyA = (uint16)Int_Limit(
                Neutral_duty + Tone_duty,
                0,
                (int32)TCPWM_DUTY_MAX);
            DutyB = (uint16)Int_Limit(
                Neutral_duty - Tone_duty,
                0,
                (int32)TCPWM_DUTY_MAX);
            break;
    }

    My_TCPWM_SetDuty(DutyA, DutyB, DutyC);
    SVPWM_DutyCache_Update(DutyA, DutyB, DutyC);
}

/* 阻塞播放一枚正弦包络音符。 */
void Foc_voice_PlayTone(
    Foc_voicePhase_t Phase,
    Foc_voicePitch_t Pitch,
    uint16 Tone_ms,
    uint16 Gap_ms)
{
    uint32 Irq_state;
    uint32 Tone_count;
    uint32 Gate_count;
    uint32 Tone_index;
    uint16 Tone_phase = 0u;
    uint16 Tone_step;
    float Envelope;

    Tone_count =
        (uint32)Tone_ms * FOC_VOICE_CONTROL_HZ / 1000u;
    Gate_count = Tone_count * FOC_VOICE_GATE_PERCENT / 100u;
    Tone_step = (uint16)(
        ((uint32)Pitch * ANGLE_PERIOD +
         (FOC_VOICE_CONTROL_HZ / 2u)) /
        FOC_VOICE_CONTROL_HZ);

    Irq_state = interrupt_global_disable();

    for (Tone_index = 0u; Tone_index < Tone_count; Tone_index++)
    {
        Envelope = Foc_voice_GetEnvelope(Tone_index, Gate_count);
        Foc_voice_Output(Envelope, Tone_phase, Phase);
        Tone_phase = Angle_Wrap(
            (int32)Tone_phase + (int32)Tone_step);
        system_delay_us(1000000u / FOC_VOICE_CONTROL_HZ);
    }

    Foc_voice_OutputNeutral();
    interrupt_global_enable(Irq_state);
    system_delay_ms(Gap_ms);
}

void Foc_voice_Start(void)
{
    (void)Foc_voice_StartSong(FOC_VOICE_SONG_ID);
}

uint8 Foc_voice_StartSong(uint8 Song_id)
{
    if ((Song_id == 0u) || (Song_id > Foc_voice_GetSongCount()))
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

uint8 Foc_voice_GetSongCount(void)
{
    return (uint8)(sizeof(Voice_songs) / sizeof(Voice_songs[0]));
}

const char *Foc_voice_GetSongName(uint8 Song_id)
{
    if ((Song_id == 0u) || (Song_id > Foc_voice_GetSongCount()))
    {
        return NULL;
    }

    return Voice_songs[Song_id - 1u].Name;
}

void Foc_voice_Stop(void)
{
    if ((Voice.Playing != 0u) ||
        (Motor.Control_mode == MOTOR_CONTROL_VOICE))
    {
        Foc_voice_Finish();
    }
}

uint8 Foc_voice_IsPlaying(void)
{
    return Voice.Playing;
}

void Foc_voice_Loop(void)
{
    float Envelope;

    if (Voice.Playing == 0u)
    {
        Foc_voice_Finish();
        return;
    }

    if (Voice.Note_total == 0u)
    {
        if (Foc_voice_LoadNote() == 0u)
        {
            return;
        }
    }
    else if (Voice.Note_elapsed >= Voice.Note_total)
    {
        Voice.Note_index++;
        if (Foc_voice_LoadNote() == 0u)
        {
            return;
        }
    }

    Envelope = Foc_voice_GetEnvelope(
        Voice.Note_elapsed,
        Voice.Gate_count);
    Foc_voice_Output(
        Envelope,
        Voice.Tone_phase,
        FOC_VOICE_PHASE_A);

    Voice.Tone_phase = Angle_Wrap(
        (int32)Voice.Tone_phase + (int32)Voice.Tone_step);
    Voice.Note_elapsed++;
    Voice.Song_elapsed++;
}
