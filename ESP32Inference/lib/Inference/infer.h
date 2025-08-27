#include <Arduino.h>
#include <stdint.h>
#include <math.h>
#include "communication.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"

extern unsigned char model_quant_tflite[];
extern unsigned int model_quant_tflite_len;

#define BATCH_SIZE 1
#define SEQUENCE_LENGTH 20
#define SENSORS 9
#define MAX_PIR_UPTIME 10.5
#define tOPS 10

typedef Data* SensorDataBatch[BATCH_SIZE][SEQUENCE_LENGTH];

class Inference {

private:
    tflite::MicroMutableOpResolver<tOPS> *resolver;
    tflite::ErrorReporter *error_reporter;
    const tflite::Model *model;
    tflite::MicroInterpreter *interpreter;
    TfLiteTensor *input[3];
    TfLiteTensor *output[2];
    uint8_t *tensor_arena;
    const float max_deltas[9] = {499., 76., 1.48, 20.91, 5.56, 1.62, 3., 0.8, MAX_PIR_UPTIME}; //
    const float min_deltas[9] = {-594., -90., -1.42, -22.01, -5.72, -1.46, -4., -0.8, 0.}; //

    float sensor_data[BATCH_SIZE][SEQUENCE_LENGTH][SENSORS];
    float human_counts[BATCH_SIZE][SEQUENCE_LENGTH][1];
    int32_t ventilation_tags[BATCH_SIZE][SEQUENCE_LENGTH];
    Prediction prediction[BATCH_SIZE];
    void printTagBuffer(uint32_t batch_ind);
    void printSensorBuffer(uint32_t batch_ind);
    void printCountBuffer(uint32_t batch_ind);
    
public:
    EventBits_t events;
    Inference();
    TfLiteTensor** GetInputBuffers();
    void SetSequences(SensorDataBatch raw_data);
    void SetShiftedOutputs(float **human_counts, int32_t** ventilation);
    void ShiftSequences(SensorDataBatch raw_data);
    void SetInputBuffers();
    void ScaleData();
    void ComputeSensorDeltas();
    bool Predict();
    void PrintBuffers();
    void SetDefaultLabels(float human_count, int32_t ventilation_tag);
    Prediction GetRecentPrediction();
};
