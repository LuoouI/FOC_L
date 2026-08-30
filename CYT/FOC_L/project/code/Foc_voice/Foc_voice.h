#ifndef FOC_VOICE_H
#define FOC_VOICE_H

#include "zf_common_headfile.h"

#define FOC_VOICE_CONTROL_HZ          (20000u)   // 声音控制调用频率，必须与电机控制频率一致
#define FOC_VOICE_MIRACLE_BPM         (128u)     // 《奇迹再现》播放速度
#define FOC_VOICE_TWINKLE_BPM         (120u)     // 《小星星》播放速度
#define FOC_VOICE_ODE_BPM             (120u)     // 《欢乐颂》播放速度
#define FOC_VOICE_ANGEL_BPM           (74u)      // 《天使的翅膀》片段播放速度
#define FOC_VOICE_DUTY_AMPLITUDE      (500u)     // 单相音频占空比峰值，500对应5%
#define FOC_VOICE_GATE_PERCENT        (92u)      // 单个音符的有效发声比例
#define FOC_VOICE_RAMP_COUNT          (100u)     // 起音和释音斜坡控制周期数
#define FOC_VOICE_SONG_ID             (1u)       // 《奇迹再现》曲目编号

/*===========================================================================*/
/*  音符频率                                                                  */
/*===========================================================================*/
typedef enum
{
    FOC_VOICE_PITCH_REST = 0,                  // 休止符
    FOC_VOICE_PITCH_B3   = 247,                // B3音符，频率约247 Hz
    FOC_VOICE_PITCH_C4   = 262,                // C4音符，频率约262 Hz
    FOC_VOICE_PITCH_CS4  = 277,                // C#4音符，频率约277 Hz
    FOC_VOICE_PITCH_D4   = 294,                // D4音符，频率约294 Hz
    FOC_VOICE_PITCH_DS4  = 311,                // D#4音符，频率约311 Hz
    FOC_VOICE_PITCH_E4   = 330,                // E4音符，频率约330 Hz
    FOC_VOICE_PITCH_F4   = 349,                // F4音符，频率约349 Hz
    FOC_VOICE_PITCH_FS4  = 370,                // F#4音符，频率约370 Hz
    FOC_VOICE_PITCH_G4   = 392,                // G4音符，频率约392 Hz
    FOC_VOICE_PITCH_GS4  = 415,                // G#4音符，频率约415 Hz
    FOC_VOICE_PITCH_A4   = 440,                // A4音符，频率约440 Hz
    FOC_VOICE_PITCH_B4   = 494,                // B4音符，频率约494 Hz
    FOC_VOICE_PITCH_C5   = 523,                // C5音符，频率约523 Hz
    FOC_VOICE_PITCH_CS5  = 554,                // C#5音符，频率约554 Hz
    FOC_VOICE_PITCH_D5   = 587,                // D5音符，频率约587 Hz
    FOC_VOICE_PITCH_DS5  = 622,                // D#5音符，频率约622 Hz
    FOC_VOICE_PITCH_E5   = 659,                // E5音符，频率约659 Hz
    FOC_VOICE_PITCH_F5   = 698,                // F5音符，频率约698 Hz
    FOC_VOICE_PITCH_FS5  = 740,                // F#5音符，频率约740 Hz
    FOC_VOICE_PITCH_G5   = 784,                // G5音符，频率约784 Hz
    FOC_VOICE_PITCH_GS5  = 831,                // G#5音符，频率约831 Hz
    FOC_VOICE_PITCH_A5   = 880,                // A5音符，频率约880 Hz
    FOC_VOICE_PITCH_B5   = 988,                // B5音符，频率约988 Hz
    FOC_VOICE_PITCH_C6   = 1047,               // C6音符，频率约1047 Hz
    FOC_VOICE_PITCH_CS6  = 1109                // C#6音符，频率约1109 Hz
} Foc_voicePitch_t;

/*===========================================================================*/
/*  音符主发声相                                                              */
/*===========================================================================*/
typedef enum
{
    FOC_VOICE_PHASE_A = 0,                     // A相正向、B相反向输出
    FOC_VOICE_PHASE_B,                         // B相正向、C相反向输出
    FOC_VOICE_PHASE_C                          // C相正向、A相反向输出
} Foc_voicePhase_t;

/*===========================================================================*/
/*  单个音符描述                                                              */
/*===========================================================================*/
typedef struct
{
    Foc_voicePitch_t Pitch;                    // 音符频率，休止符为0
    uint8 Duration_8th;                         // 音符时值，以八分音符为单位
} Foc_voiceNote_t;

/*===========================================================================*/
/*  乐曲段落描述                                                              */
/*===========================================================================*/
typedef struct
{
    const Foc_voiceNote_t *Note_data;           // 段落音符数据
    uint16 Note_count;                           // 段落音符数量
} Foc_voiceSection_t;

/*===========================================================================*/
/*  乐曲描述                                                                  */
/*===========================================================================*/
typedef struct
{
    const char *Name;                           // UTF-8编码乐曲名称
    const Foc_voiceSection_t *Section_data;     // 乐曲段落数据
    uint16 Bpm;                                  // 乐曲播放速度
    uint8 Section_count;                         // 乐曲段落数量
} Foc_voiceSong_t;

/*===========================================================================*/
/*  电机音乐播放状态                                                          */
/*===========================================================================*/
typedef struct
{
    uint32 Note_elapsed;                         // 当前音符已执行控制周期数
    uint32 Note_total;                           // 当前音符总控制周期数
    uint32 Gate_count;                           // 当前音符有效发声控制周期数
    uint32 Song_elapsed;                         // 当前曲目累计执行控制周期数
    uint16 Tone_phase;                           // 音频正弦波相位
    uint16 Tone_step;                            // 单控制周期音频相位增量
    uint16 Note_index;                           // 当前段落音符索引
    uint16 Song_duration8th;                     // 当前音符结束位置对应的累计八分音符数
    uint8 Section_index;                         // 当前乐曲段落索引
    uint8 Song_id;                               // 当前乐曲编号
    uint8 Playing;                               // 正在播放标志
} Foc_voice_t;

/***********************************************
 * @brief : 阻塞播放一枚带正弦包络的音符，调用前应确保电机停止
 * @param : Phase 主发声相
 * @param : Pitch 音符频率
 * @param : Tone_ms 音符持续时间，单位为ms
 * @param : Gap_ms 音符结束后的静音时间，单位为ms
 * @return: 无
 * @date  : 2026-08-29
 * @author: L
 ************************************************/
void Foc_voice_PlayTone(
    Foc_voicePhase_t Phase,
    Foc_voicePitch_t Pitch,
    uint16 Tone_ms,
    uint16 Gap_ms);

/***********************************************
 * @brief : 从头开始播放《奇迹再现》旋律
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_voice_Start(void);

/***********************************************
 * @brief : 从头播放指定的下位机内置乐曲
 * @param : Song_id 乐曲编号，范围1~内置乐曲数量
 * @return: 1表示开始播放，0表示乐曲编号无效
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
uint8 Foc_voice_StartSong(uint8 Song_id);

/***********************************************
 * @brief : 获取下位机内置乐曲数量
 * @param : 无
 * @return: 内置乐曲数量
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
uint8 Foc_voice_GetSongCount(void);

/***********************************************
 * @brief : 获取指定内置乐曲的UTF-8名称
 * @param : Song_id 乐曲编号，范围1~内置乐曲数量
 * @return: 乐曲名称地址，编号无效时返回空指针
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
const char *Foc_voice_GetSongName(uint8 Song_id);

/***********************************************
 * @brief : 停止播放并关闭电机电压输出
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_voice_Stop(void);

/***********************************************
 * @brief : 查询电机音乐是否正在播放
 * @param : 无
 * @return: 1表示正在播放，0表示未播放
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
uint8 Foc_voice_IsPlaying(void);

/***********************************************
 * @brief : 执行一次电机音乐控制周期，需按20 kHz周期调用
 * @param : 无
 * @return: 无
 * @date  : 2026-08-28
 * @author: L
 ************************************************/
void Foc_voice_Loop(void);

#endif
