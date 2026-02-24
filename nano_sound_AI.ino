#include "model.h"
#include "microphone.h"

void setup() {
    // 初始化串口
    Serial.begin(115200);
    // 等待串口连接 (如果希望上电拔掉USB也能独立跑，可以注释掉这行)
    while (!Serial); 
    
    Serial.println("🐝 智能蜂箱声音 AI 分类系统启动中...");
    
    // 初始化模型和麦克风
    model_init();
}

void loop() {
    Serial.println("\n🎙️ 正在采集音频...");
    
    // 阻塞式采集数据，填满一个时间窗口
    microphone_record_data();
    
    Serial.println("🧠 采集完毕，开始推理...");
    
    // 喂给模型并输出结果
    model_run_inference();
    
    // 加一个延时，避免疯狂循环，方便观察串口打印
    delay(1000); 
}