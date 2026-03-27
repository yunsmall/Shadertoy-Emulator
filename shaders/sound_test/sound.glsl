// Sound test shader - 440Hz sine wave
// iSampleRate = 44100

vec2 mainSound(int samp, float time) {
    // 440Hz A4 note sine wave
    float freq = 440.0;
    float wave = sin(time * freq * 6.28318) * 0.5*pow(0.8,time);
//    float wave = sin(time * freq * 6.28318) * 0.5;

    // Left and right channels
    return vec2(wave, wave);
}
