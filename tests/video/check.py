"""视频导出（帧数/时长/音轨同步）"""
import json
import subprocess

from utils import export


def run(exe, out):
    """帧数、时长、音轨，以及音画是否同步（用 ffprobe 读回来核对）"""
    fps, seconds = 30, 2
    videopath = out / "clip.mp4"
    # 复用 sound 用例的场景：有 Sound pass 才验得了音轨和音画同步
    export(exe, ["tests/sound/config.json", "--video", str(videopath),
                 "--duration", str(seconds), "--fps", str(fps),
                 "--width", "128", "--height", "128"],
           out, with_output_dir=False)

    if not videopath.exists() or videopath.stat().st_size == 0:
        print(f"    没生成视频文件: {videopath}")
        return False

    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries",
         "stream=codec_type,duration,nb_frames:format=duration", "-of", "json", str(videopath)],
        capture_output=True, text=True)
    if probe.returncode != 0:
        print(f"    ffprobe 失败: {probe.stderr.strip()}")
        return False

    info = json.loads(probe.stdout)
    streams = info.get("streams", [])
    kinds = [s.get("codec_type") for s in streams]
    if "video" not in kinds:
        print(f"    没有视频流: {kinds}")
        return False

    video = next(s for s in streams if s.get("codec_type") == "video")
    frames = int(video.get("nb_frames", 0))
    if frames != seconds * fps:
        print(f"    帧数 {frames}，期望 {seconds * fps}")
        return False

    # 时长优先取流自己的，取不到再退回容器的
    vdur = float(video.get("duration") or info.get("format", {}).get("duration", 0))
    if abs(vdur - seconds) > 0.2:
        print(f"    视频时长 {vdur:.2f}s，期望 {seconds}s")
        return False

    if "audio" not in kinds:
        print(f"    shader 有 Sound pass 但视频里没有音轨: {kinds}")
        return False

    # 音画同步：音频也该是 2 秒，而不是按"每帧 0.5 秒"累积出来的 60 秒
    audio = next(s for s in streams if s.get("codec_type") == "audio")
    adur = float(audio.get("duration") or info.get("format", {}).get("duration", 0))
    if abs(adur - seconds) > 0.2:
        print(f"    音频时长 {adur:.2f}s，期望 {seconds}s（音画不同步）")
        return False
    return True
