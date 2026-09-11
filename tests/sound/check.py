"""Sound 频谱（频率/幅度/声道隔离）"""
import wave

import numpy as np

from utils import export


def run(exe, out):
    """左右声道各几个已知频率，用 FFT 核对峰值位置、幅度和声道隔离"""
    wav = out / "audio.wav"
    # 音频长度是 帧数/fps，60 帧正好 1 秒。整数频率只有落在整数号 bin 上才不会漏能量，
    # 所以窗口必须正好整秒
    export(exe, ["tests/sound/config.json", "--images", "0:60:1",
                 "--dump-audio", str(wav)], out)

    with wave.open(str(wav), "rb") as w:
        if (w.getnchannels(), w.getframerate(), w.getsampwidth()) != (2, 44100, 2):
            print(f"    格式异常: {w.getnchannels()}ch {w.getframerate()}Hz "
                  f"{w.getsampwidth() * 8}bit")
            return False
        frames = w.getnframes()
        raw = w.readframes(frames)

    if frames != 44100:
        print(f"    音频 {frames} 帧，期望 44100（正好 1 秒）")
        return False

    samples = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32767.0
    left = samples[0::2]
    right = samples[1::2]

    # 每个声道：自己该有的峰，以及只有对面才有的频率——后者有值就是串道
    channels = [
        ("左", left, [(440, 0.30), (1000, 0.20), (3000, 0.10)], (700, 2000)),
        ("右", right, [(700, 0.40), (2000, 0.25)], (440, 1000, 3000)),
    ]

    ok = True
    for name, channel, peaks, others in channels:
        # 单边谱除以 N/2 之后，峰值就等于该正弦的幅度
        mag = np.abs(np.fft.rfft(channel)) / (len(channel) / 2)

        for freq, want in peaks:
            got = mag[freq]
            if abs(got - want) > want * 0.05:
                ok = False
                print(f"    {name}声道 {freq}Hz 幅度 {got:.4f}，期望 {want}")

        for freq in others:
            if mag[freq] > 0.01:
                ok = False
                print(f"    {name}声道串入了 {freq}Hz，幅度 {mag[freq]:.4f}")
    return ok
