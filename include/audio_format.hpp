#pragma once

// Sound pass 固定按这个采样率出样本（iSampleRate 就是它），导出的 WAV 头也照它写。
// 播放、生成、导出三方必须一致，所以放一处
inline constexpr int SOUND_SAMPLE_RATE = 44100;
