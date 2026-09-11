"""Sound + common（函数定义不重复展开）"""
import wave

import numpy as np

from utils import export


def run(exe, out):
    """Sound 引用 common 里的函数：公共代码只能展开一次，展开两遍会撞成重复定义"""
    wav = out / "audio.wav"
    # shader 和 common.glsl 都在 sound 目录下，这里只是换成走 common 的那份配置
    export(exe, ["tests/sound/config-common.json", "--images", "0:60:1",
                 "--dump-audio", str(wav)], out)

    if not wav.exists():
        # shader 编译不过就不会有音频，连文件都不建
        print("    没有音频输出，Sound pass 大概没编译过")
        return False

    with wave.open(str(wav), "rb") as w:
        frames = w.getnframes()
        raw = w.readframes(frames)
    if frames != 44100:
        print(f"    音频 {frames} 帧，期望 44100（正好 1 秒）")
        return False

    samples = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32767.0

    # 两个频率都出自 common.glsl 的 tone()，宏 TAU 也是那边定义的
    ok = True
    for name, channel, freq, want in (("左", samples[0::2], 440, 0.30),
                                      ("右", samples[1::2], 700, 0.40)):
        mag = np.abs(np.fft.rfft(channel)) / (len(channel) / 2)
        if abs(mag[freq] - want) > want * 0.05:
            ok = False
            print(f"    {name}声道 {freq}Hz 幅度 {mag[freq]:.4f}，期望 {want}")
    return ok
