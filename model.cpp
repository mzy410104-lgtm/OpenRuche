#include "model.h"
#include "microphone.h"

// ⚠️ 全局只有这里包含模型库！
#include <bee_sound_classification_v2_inferencing.h>

void model_init() {
    // 使用模型头文件中的宏定义，告诉麦克风需要准备多大的 Buffer
    Serial.print("模型需要的采样点数量: ");
    Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    
    // 初始化麦克风
    microphone_init(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
}

void model_run_inference() {
    // 配置 Edge Impulse 的 signal_t 结构体
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    // 存放推理结果的结构体
    ei_impulse_result_t result = { 0 };

    // 运行分类器 (false 表示不开启 debug 模式)
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK) {
        Serial.print("❌ 推理失败 (错误码: ");
        Serial.print(res);
        Serial.println(")");
        return;
    }

    // 串口输出判断结果
    Serial.println("--- 推理结果 ---");
    // 遍历所有分类标签并输出置信度
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.print(result.classification[i].label);
        Serial.print(": ");
        // 保留5位小数输出
        Serial.println(result.classification[i].value, 5); 
    }
    Serial.println("----------------");
}