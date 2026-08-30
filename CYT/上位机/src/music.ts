/*===========================================================================*/
/*  仿真器及旧版下位机兼容乐曲列表                                            */
/*===========================================================================*/
import type { Music_track_t } from "./types";

export const Music_tracks: Music_track_t[] = [
  { id: 1, name: "奇迹再现" },
  { id: 2, name: "小星星" },
  { id: 3, name: "欢乐颂" },
  { id: 4, name: "天使的翅膀（片段）" },
];

export const Default_music_track = Music_tracks[0];
