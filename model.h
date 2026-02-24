#ifndef MODEL_H
#define MODEL_H

#include <Arduino.h>

// 初始化模型（内部会自动初始化麦克风）
void model_init();

// 执行一次模型推理并串口输出结果
void model_run_inference();

#endif // MODEL_H