#include "microphone.h"
#include <PDM.h>

// 麦克风 PDM 接收临时缓冲区
static short pdm_buffer[512];

// 存放完整一次推理所需音频数据的 Buffer (动态分配)
static int16_t *inference_buffer = nullptr;
static volatile uint32_t inference_buffer_index = 0;
static volatile bool is_recording_ready = false;
static int required_samples = 0;

// PDM 数据接收中断回调函数
static void onPDMdata() {
    // 检查有多少字节可读
    int bytesAvailable = PDM.available();
    
    // 读入临时 buffer
    int samplesRead = PDM.read(pdm_buffer, bytesAvailable);

    // 如果还没有存满一帧推理所需的数据，则放入 inference_buffer
    for(int i = 0; i < samplesRead; i++) {
        if (inference_buffer_index < required_samples) {
            inference_buffer[inference_buffer_index++] = pdm_buffer[i];
        }
    }

    // 检查是否已经填满
    if (inference_buffer_index >= required_samples) {
        is_recording_ready = true;
    }
}

void microphone_init(int sample_count) {
    required_samples = sample_count;
    
    // 根据模型所需的长度动态分配内存
    if (inference_buffer == nullptr) {
        inference_buffer = new int16_t[required_samples];
    }

    // 设置回调函数
    PDM.onReceive(onPDMdata);
    // 配置 PDM 缓冲区大小
    PDM.setBufferSize(512);

    // 启动 PDM：单声道 (1), 采样率 16000 Hz
    if (!PDM.begin(1, 16000)) {
        Serial.println("❌ PDM 麦克风初始化失败!");
        while (1) {
            delay(10); // 挂机
        }
    }
    Serial.println("✅ PDM 麦克风初始化成功");
}

bool microphone_record_data() {
    // 重置状态，准备接受新一轮数据
    inference_buffer_index = 0;
    is_recording_ready = false;

    // 阻塞等待数据填满 (中断会在后台工作)
    while (!is_recording_ready) {
        delay(1);
    }
    return true;
}

// 供外部获取数据的接口，手动完成 int16_t 到 float 的转换
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)inference_buffer[offset + i];
    }
    return 0;
}