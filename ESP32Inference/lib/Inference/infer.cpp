#include "infer.h"

const u_int32_t kArenaSize = 20 * 1024;

Inference::Inference()
{
    error_reporter = new tflite::MicroErrorReporter();

    model = tflite::GetModel(model_quant_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        TF_LITE_REPORT_ERROR(error_reporter, "Model provided is schema version %d not equal to supported version %d.", model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    // Pulls in the operators implementations we need
    resolver = new tflite::MicroMutableOpResolver<tOPS>();
    resolver->AddCast();
    resolver->AddGather(); 
    resolver->AddConcatenation(); 
    resolver->AddDequantize();
    resolver->AddQuantize();
    resolver->AddUnidirectionalSequenceLSTM();
    resolver->AddStridedSlice(); 
    resolver->AddFullyConnected(); 
    resolver->AddLogistic();
    resolver->AddRelu();
    
    tensor_arena = (uint8_t *)malloc(kArenaSize);
   
    if (!tensor_arena)
    {
        TF_LITE_REPORT_ERROR(error_reporter, "Could not allocate arena");
        return;
    }

    // Builds an interpreter to run the model with.
    interpreter = new tflite::MicroInterpreter(model, *resolver, tensor_arena, kArenaSize);

    // Allocates memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
        return;
    }

    size_t used_bytes = interpreter->arena_used_bytes();
    TF_LITE_REPORT_ERROR(error_reporter, "Model arena: used bytes %d\n", used_bytes);

    // Pointers to the model's input and output tensors.
    input[0] = interpreter->input(0);
    input[1] = interpreter->input(1);
    input[2] = interpreter->input(2);
    output[0] = interpreter->output(0);
    output[1] = interpreter->output(1);
}


TfLiteTensor** Inference::GetInputBuffers()
{
    if(input[0]->type == kTfLiteFloat32)
        Serial.println("Input 1 OK");

    if(input[1]->type == kTfLiteFloat32)
        Serial.println("Input 2 OK");

    if(input[2]->type == kTfLiteInt32)
        Serial.println("Input 3 OK");
    for (int i = 0; i < sizeof(input) / sizeof(void*); i++)
    {
        Serial.printf("Input %d total dimensions: %d\n", i,  input[i]->dims->size);
        for (int k = 0; k < input[i]->dims->size; k++) {
            printf("dim[%d] = %d\n", i, input[i]->dims->data[k]);
        }
    }
    return input;
}

void Inference::SetSequences(SensorDataBatch raw_data_sequence)
{
    for (int i = 0; i < BATCH_SIZE; i++)
        for(int k = 0; k < SEQUENCE_LENGTH; k++)
        {
            sensor_data[i][k][0] = (float)raw_data_sequence[i][k]->co2_ppm;
            sensor_data[i][k][1] = (float)raw_data_sequence[i][k]->tvoc_ppm;
            sensor_data[i][k][2] = raw_data_sequence[i][k]->bmp280_temperature;
            sensor_data[i][k][3] = raw_data_sequence[i][k]->bmp280_pressure;
            sensor_data[i][k][4] = raw_data_sequence[i][k]->mlx_object_temperature;
            sensor_data[i][k][5] = raw_data_sequence[i][k]->mlx_ambient_temperature;
            sensor_data[i][k][6] = raw_data_sequence[i][k]->humidity_dht;
            sensor_data[i][k][7] = raw_data_sequence[i][k]->temperature_dht;
            sensor_data[i][k][8] = raw_data_sequence[i][k]->pir_uptime;
        }
    Serial.printf("Sensor sequences are set!\n");
}

void Inference::SetShiftedOutputs(float **counts, int32_t **ventilation)
{
    for (int i = 0; i < BATCH_SIZE; i++)
        for(int k = 0; k < SEQUENCE_LENGTH; k++)
        {
            human_counts[i][k][0] = counts[i][k];
            ventilation_tags[i][k] = ventilation[i][k];
        }
}

// shifts pointers in data sequences
void Inference::ShiftSequences(SensorDataBatch sensor_data_sequence)
{
     for (int i = 0; i < BATCH_SIZE; i++)
     {
        Data *first_element = sensor_data_sequence[i][0];
        for(int k = 0; k < SEQUENCE_LENGTH-1; k++)
        {
            human_counts[i][k][0] = human_counts[i][k+1][0];
            ventilation_tags[i][k] = ventilation_tags[i][k+1];
            sensor_data_sequence[i][k] = sensor_data_sequence[i][k+1];
        }
        sensor_data_sequence[i][SEQUENCE_LENGTH-1] = first_element; //first element is now last and can be overwritten
        ventilation_tags[i][SEQUENCE_LENGTH-1] = prediction[i].ventilation_tag;// here as well
        human_counts[i][SEQUENCE_LENGTH-1][0] = prediction[i].human_count; //need to be rounded/typecasted
     }
}

void Inference::SetInputBuffers()
{
    for (int i = 0; i < BATCH_SIZE; i++)
        for(int k = 0; k < SEQUENCE_LENGTH; k++)
        {
            for(int j = 0; j < SENSORS; j++)
                input[0]->data.f[i * (SEQUENCE_LENGTH * SENSORS)+ k * SENSORS + j] = sensor_data[i][k][j];
            input[1]->data.f[i * SEQUENCE_LENGTH + k] = human_counts[i][k][0];
            input[2]->data.i32[i * SEQUENCE_LENGTH + k] = ventilation_tags[i][k];
        }
}

void Inference::ScaleData()
{
    for (int i = 0; i < BATCH_SIZE; i++)
        for(int k = 0; k < SEQUENCE_LENGTH; k++)
            for(int j = 0; j < SENSORS; j++)
                sensor_data[i][k][j] = (sensor_data[i][k][j] - min_deltas[j])/(max_deltas[j] - min_deltas[j]);
}

void Inference::ComputeSensorDeltas()
{
    for (int i = 0; i < BATCH_SIZE; i++)
    {
        float first_row[SENSORS];
        memcpy(first_row,  sensor_data[i][0], sizeof(sensor_data[i][0]));
        for(int k = 0; k < SEQUENCE_LENGTH; k++)
            for(int j = 0; j < SENSORS-1; j++)
                sensor_data[i][k][j] -= first_row[j];
    }
}

bool Inference::Predict()
{
    
    if(interpreter->Invoke() != kTfLiteOk)
    {
       TF_LITE_REPORT_ERROR(error_reporter, "Interpreter invokation error\n");
       return false;
    }
    for (int i = 0; i < BATCH_SIZE; i++)
    {
        prediction[i].human_count = roundf(output[1]->data.f[i]);
        prediction[i].ventilation_tag = (int32_t)roundf(output[0]->data.f[i]);
        Serial.printf("Raw human count: %.2f\n", output[1]->data.f[i]);
        Serial.printf("Raw ventilation: %.2f\n", output[0]->data.f[i]);
    }
    return true;
}


void Inference::printTagBuffer(uint32_t batch_ind)
{
    Serial.print("[");
    for(int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        if (i != SEQUENCE_LENGTH - 1)
            Serial.printf("%d, ", ventilation_tags[batch_ind][i]);
        else
            Serial.printf("%d", ventilation_tags[batch_ind][i]);
    }
    Serial.print("]");
}

void Inference::printSensorBuffer(uint32_t batch_ind)
{
    Serial.print("[");
    for(int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        Serial.print("[");
        for(int j = 0; j < SENSORS; j++)
        {
            if (j != SENSORS - 1)
                Serial.printf("%.2f, ", sensor_data[batch_ind][i][j]);
            else
                Serial.printf("%.2f", sensor_data[batch_ind][i][j]);
        }
        if (i != SEQUENCE_LENGTH - 1)
            Serial.print("]\n");
        else
            Serial.print("]");
    }
    Serial.print("],\n");
}

void Inference::printCountBuffer(uint32_t batch_ind)
{
    Serial.print("[");
    for(int i = 0; i < SEQUENCE_LENGTH; i++)
    {
        Serial.printf("[%.2f]", human_counts[batch_ind][i][0]);
        if (i != SEQUENCE_LENGTH - 1)
            Serial.print(", ");
    }
    Serial.print("],\n");
}

void Inference::PrintBuffers()
{
    for (int i = 0; i < BATCH_SIZE; i++)
    {   
        Serial.print("(");
        printSensorBuffer(i);
        printCountBuffer(i);
        printTagBuffer(i);
        Serial.print(")\n");
    }
}

void Inference::SetDefaultLabels(float human_count, int32_t ventilation_tag)
{
    for (int i = 0; i < BATCH_SIZE; i++)
        for(int k = 0; k < SEQUENCE_LENGTH; k++)
        {
            human_counts[i][k][0] = human_count;
            ventilation_tags[i][k] = ventilation_tag;
        }
}

Prediction Inference::GetRecentPrediction()
{
    return prediction[BATCH_SIZE - 1];
}


