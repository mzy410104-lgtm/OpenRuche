#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <Arduino.h>

// 初始化麦克风，传入需要缓存的采样点总数
void microphone_init(int sample_count);

// 阻塞式录音，直到填满指定数量的数据
bool microphone_record_data();

// 供外部获取数据的回调函数（将 int16_t 转为 float）
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);

#endif // MICROPHONE_H