#include "stm32l4xx.h"
#include <math.h>
#include "IMU.h"

extern I2C_HandleTypeDef hi2c2;

int16_t gyro_offset[3] = {0, 0, 0};
int16_t accel_offset[3] = {0, 0, 0};


float accel_g[3], gyro_dps[3], gyro_angle[3];


//FIRST TRY (CHECKING IF IMU IS AVAILABLE)

HAL_StatusTypeDef IMU_ReadRegister(uint16_t reg, uint8_t *data) {
	HAL_StatusTypeDef error;
    error = HAL_I2C_Mem_Read(&hi2c2, IMU_I2C_ADDR, reg, 1, data, 1, 1000);
    return error;
}

HAL_StatusTypeDef IMU_ReadRegisters(uint16_t reg, uint8_t *data, uint16_t length) {
	HAL_StatusTypeDef error;
    error = HAL_I2C_Mem_Read(&hi2c2, IMU_I2C_ADDR, reg, 1, data, length, 1000);
    return error;
}

HAL_StatusTypeDef IMU_Calibrate() {
    uint8_t buffer[14];
    int32_t accel_sum[3] = {0, 0, 0};
    int32_t gyro_sum[3] = {0, 0, 0};
    int num_samples = 1000;  // Number of samples for averaging

    for (int i = 0; i < num_samples; i++) {
        if (IMU_ReadRegisters(0x3B, buffer, 14) != HAL_OK) {
            return HAL_ERROR;
        }

        int16_t accelX = (buffer[0] << 8) | buffer[1];
        int16_t accelY = (buffer[2] << 8) | buffer[3];
        int16_t accelZ = (buffer[4] << 8) | buffer[5];

        int16_t gyroX = (buffer[8] << 8) | buffer[9];
        int16_t gyroY = (buffer[10] << 8) | buffer[11];
        int16_t gyroZ = (buffer[12] << 8) | buffer[13];

        // Sum values for averaging
        accel_sum[0] += accelX;
        accel_sum[1] += accelY;
        accel_sum[2] += (accelZ - 16384);  // Remove gravity effect

        gyro_sum[0] += gyroX;
        gyro_sum[1] += gyroY;
        gyro_sum[2] += gyroZ;
        HAL_Delay(2);
    }

    // Compute average offset
    for (int i = 0; i < 3; i++) {
        accel_offset[i] = accel_sum[i] / num_samples;
        gyro_offset[i] = gyro_sum[i] / num_samples;
    }

    return HAL_OK;
}





HAL_StatusTypeDef IMU_Init(void) {
    uint8_t who_am_i;


    if (HAL_I2C_IsDeviceReady(&hi2c2, IMU_I2C_ADDR, 2, 1000)!= HAL_OK){
    	return HAL_ERROR;
    }

    if (IMU_ReadRegister(WHO_AM_I_REG, &who_am_i)!= HAL_OK){
    	return HAL_ERROR;
    }
    else{
    	if (who_am_i!=0x71){
    		return HAL_ERROR;
    	}
    }


    if (IMU_Calibrate()!= HAL_OK){
    	return HAL_ERROR;
    }

    return HAL_OK;


}




void Convert_IMU_Data_All(int16_t *raw_accel, int16_t *raw_gyro, float *accel_g, float *gyro_dps, float *gyro_angle, float dt) {
    float accel_sensitivity = 16384.0f;  // ±2g
    float gyro_sensitivity  = 131.0f;    // ±250°/s

    // Static variable to hold cumulative integration between calls
    static float integrated_angle[3] = {0.0f, 0.0f, 0.0f};
    float treshold_dps = 1.0;

    // Convert accelerometer values to g
    // Convert gyroscope values to °/s and integrate to get angle
    for (int i = 0; i < 3; i++) {
        accel_g[i] = raw_accel[i] / accel_sensitivity;

        gyro_dps[i] = raw_gyro[i] / gyro_sensitivity;


        if (fabs(gyro_dps[i])>treshold_dps){
        	integrated_angle[i] += gyro_dps[i] * dt;
        }

        gyro_angle[i] = integrated_angle[i];

    }
}


HAL_StatusTypeDef IMU_ReadAccelGyro_Converted_All(float dt) {
    uint8_t buffer[14];

    if (IMU_ReadRegisters(0x3B, buffer, 14) != HAL_OK) {
        return HAL_ERROR;
    }

    int16_t raw_accel[3];
    int16_t raw_gyro[3];

    // Apply offsets
    raw_accel[0] = ((buffer[0] << 8) | buffer[1]) - accel_offset[0];
    raw_accel[1] = ((buffer[2] << 8) | buffer[3]) - accel_offset[1];
    raw_accel[2] = ((buffer[4] << 8) | buffer[5]) - accel_offset[2];

    raw_gyro[0] = ((buffer[8] << 8) | buffer[9]) - gyro_offset[0];
    raw_gyro[1] = ((buffer[10] << 8) | buffer[11]) - gyro_offset[1];
    raw_gyro[2] = ((buffer[12] << 8) | buffer[13]) - gyro_offset[2];

    // Convert to g and dps, and integrate gyroscope values into angles
    Convert_IMU_Data_All(raw_accel, raw_gyro, accel_g, gyro_dps, gyro_angle, dt);

    return HAL_OK;
}



//READING ACCELERATION

//int IMU_Check_Registers(uint16_t reg, uint8_t *data, uint16_t length) {
//    if (HAL_I2C_Mem_Read(&hi2c2, IMU_I2C_ADDR, reg, 1, data, length, HAL_MAX_DELAY) != HAL_OK) {
//        Send_UART_Message("I2C Read Error!\r\n");
//
//        return 1;  // Return error
//    }
//    return 0;  // Return success
//}

// BASIC OPERATION (READING RAW VALUES)

//void IMU_ReadPrint_AccelGyro_NoOffset(int16_t *gyro_offset, int16_t *accel_offset) {
//    uint8_t buffer[14];
//    char msg[50];
//
//    // Read accelerometer + gyroscope data
//    if (IMU_ReadRegisters(0x3B, buffer, 14) != 0) {
//        Send_UART_Message("Failed to read AccelGyro!\r\n");
//        return;
//    }
//
//    int16_t accelX = ((buffer[0] << 8) | buffer[1]);
//    int16_t accelY = ((buffer[2] << 8) | buffer[3]);
//    int16_t accelZ = ((buffer[4] << 8) | buffer[5]);
//
//    int16_t gyroX = ((buffer[8] << 8) | buffer[9]);
//    int16_t gyroY = ((buffer[10] << 8) | buffer[11]);
//    int16_t gyroZ = ((buffer[12] << 8) | buffer[13]);
//
//    // Send messages via UART
//    sprintf(msg, "Accel: %d %d %d\r\n", accelX, accelY, accelZ);
//    Send_UART_Message(msg);
//    sprintf(msg, "Gyro: %d %d %d\r\n", gyroX, gyroY, gyroZ);
//    Send_UART_Message(msg);
//}

//CHECKING REGISTERS OFFSET

//void IMU_ReadGyroOffsetsReg(void) {
//    uint8_t buffer[6];
//    char msg[60];
//
//    // Read gyroscope offset registers (0x13 to 0x18)
//    if (IMU_ReadRegisters(0x13, buffer, 6) != 0) {
//        Send_UART_Message("Failed to read Gyro offsets!\r\n");
//        return;
//    }
//
//    int16_t gyroOffsetX = (buffer[0] << 8) | buffer[1];
//    int16_t gyroOffsetY = (buffer[2] << 8) | buffer[3];
//    int16_t gyroOffsetZ = (buffer[4] << 8) | buffer[5];
//
//    // Print the offsets
//    sprintf(msg, "Gyro Offsets registers: X=%d, Y=%d, Z=%d\r\n", gyroOffsetX, gyroOffsetY, gyroOffsetZ);
//    Send_UART_Message(msg);
//}

//void IMU_ReadAccelOffsets(void) {
//    uint8_t buffer[6];
//    char msg[80];
//
//    // Read accelerometer offset registers: 0x77–0x78 (X), 0x7A–0x7B (Y), 0x7D–0x7E (Z)
//    if (IMU_ReadRegisters(0x77, buffer, 2) != 0 ||
//        IMU_ReadRegisters(0x7A, buffer + 2, 2) != 0 ||
//        IMU_ReadRegisters(0x7D, buffer + 4, 2) != 0) {
//        Send_UART_Message("Failed to read Accel offsets!\r\n");
//        return;
//    }
//
//    // Reconstruct 15-bit signed values
//    int16_t accelOffsetX = ((int16_t)buffer[0] << 7) | (buffer[1] >> 1);
//    int16_t accelOffsetY = ((int16_t)buffer[2] << 7) | (buffer[3] >> 1);
//    int16_t accelOffsetZ = ((int16_t)buffer[4] << 7) | (buffer[5] >> 1);
//
//    // Sign-extend to 16 bits if needed (15-bit value to signed int)
//    if (accelOffsetX & 0x4000) accelOffsetX |= 0x8000;
//    if (accelOffsetY & 0x4000) accelOffsetY |= 0x8000;
//    if (accelOffsetZ & 0x4000) accelOffsetZ |= 0x8000;
//
//    // Print the offsets
//    sprintf(msg, "Accel Offsets registers: X=%d, Y=%d, Z=%d\r\n",
//            accelOffsetX, accelOffsetY, accelOffsetZ);
//    Send_UART_Message(msg);
//}



// PRINT ALL VECTORS FROM IMU_ReadAccelGyro_Converted_All

//void IMU_Print_AccelGyro_Vectors(float *accel_g, float *gyro_dps, float *gyro_angle) {
//    char num_str[10];
//
//    // -------- Accelerometer Output --------
//    Send_UART_Message("Accel (g): X=");
//    itoa((int)(accel_g[0] * 100), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message(" Y=");
//    itoa((int)(accel_g[1] * 100), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message(" Z=");
//    itoa((int)(accel_g[2] * 100), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message("\r\n");
//
//    // -------- Gyroscope Output (°/s) --------
//    Send_UART_Message("Gyro (dps): X=");
//    itoa((int)(gyro_dps[0] * 100), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message(" Y=");
//    itoa((int)(gyro_dps[1] * 100), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message(" Z=");
//    itoa((int)(gyro_dps[2] * 100), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message("\r\n");
//
//    // -------- Integrated Gyro Angles --------
//    Send_UART_Message("Gyro Angles (degrees): X=");
//    itoa((int)(gyro_angle[0]), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message(" Y=");
//    itoa((int)(gyro_angle[1]), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message(" Z=");
//    itoa((int)(gyro_angle[2]), num_str, 10);
//    Send_UART_Message(num_str);
//    Send_UART_Message("\r\n");
//}

//FORCE SENSOR SENSITIVITY

//void IMU_ForceSensorSensitivity(void) {
//    uint8_t accel_config = 0x00; // ±2g
//    uint8_t gyro_config  = 0x00; // ±250°/s
//
//    HAL_I2C_Mem_Write(&hi2c2, IMU_I2C_ADDR, 0x1C, 1, &accel_config, 1, HAL_MAX_DELAY);
//    HAL_I2C_Mem_Write(&hi2c2, IMU_I2C_ADDR, 0x1B, 1, &gyro_config, 1, HAL_MAX_DELAY);
//
//    Send_UART_Message("Sensor sensitivities set: Accel=±2g, Gyro=±250°/s\r\n");
//}
//
////TO ASSURE THE SENSOR IS ENABLED
//
//void IMU_Enable_Sensors(void) {
//    uint8_t data = 0x00;  // Enable all sensors (Gyro + Accel)
//
//    // Write to Power Management 2 Register (0x6C)
//    HAL_I2C_Mem_Write(&hi2c2, IMU_I2C_ADDR, 0x6C, 1, &data, 1, HAL_MAX_DELAY);
//
//    Send_UART_Message("All Sensors Enabled!\r\n");
//}
