/*
 * Copyright (c) 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <string.h>

#include "sh2.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"

LOG_MODULE_REGISTER(BNO08X_APP, CONFIG_SENSOR_LOG_LEVEL);

K_MSGQ_DEFINE(app_queue, sizeof(sh2_SensorEvent_t *), 10, 4);

static sh2_ProductIds_t product_ids;

static void event_callback(void * cookie, sh2_AsyncEvent_t *pEvent)
{

}

static void print_event(sh2_SensorEvent_t *event)
{
    int rc;
    sh2_SensorValue_t value;
    float scaleRadToDeg = 180.0 / 3.14159265358;
    float r, i, j, k, acc_deg, x, y, z;
    float t;
    static int skip = 0;

    rc = sh2_decodeSensorEvent(&value, event);
    if (rc != SH2_OK) {
    	LOG_ERR("Error decoding sensor event: %d\n", rc);
        return;
    }

    t = value.timestamp / 1000000.0;  // time in seconds.

    switch (value.sensorId) {
        case SH2_RAW_ACCELEROMETER:
            LOG_INF("Raw acc: %d %d %d\n",
                   value.un.rawAccelerometer.x,
                   value.un.rawAccelerometer.y, value.un.rawAccelerometer.z);
            break;

        case SH2_ACCELEROMETER:
        	LOG_INF("Acc: %f %f %f\n",
                   value.un.accelerometer.x,
                   value.un.accelerometer.y,
                   value.un.accelerometer.z);
            break;
        case SH2_ROTATION_VECTOR:
            r = value.un.rotationVector.real;
            i = value.un.rotationVector.i;
            j = value.un.rotationVector.j;
            k = value.un.rotationVector.k;
            acc_deg = scaleRadToDeg *
                value.un.rotationVector.accuracy;
            LOG_INF("%8.4f Rotation Vector: "
                   "r:%0.6f i:%0.6f j:%0.6f k:%0.6f (acc: %0.6f deg)\n",
                   t,
                   r, i, j, k, acc_deg);
            break;
        case SH2_GAME_ROTATION_VECTOR:
            r = value.un.gameRotationVector.real;
            i = value.un.gameRotationVector.i;
            j = value.un.gameRotationVector.j;
            k = value.un.gameRotationVector.k;
            LOG_INF("%8.4f GRV: "
                   "r:%0.6f i:%0.6f j:%0.6f k:%0.6f\n",
                   t,
                   r, i, j, k);
            break;
        case SH2_GYROSCOPE_CALIBRATED:
            x = value.un.gyroscope.x;
            y = value.un.gyroscope.y;
            z = value.un.gyroscope.z;
            LOG_INF("%8.4f GYRO: "
                   "x:%0.6f y:%0.6f z:%0.6f\n",
                   t,
                   x, y, z);
            break;
        case SH2_GYROSCOPE_UNCALIBRATED:
            x = value.un.gyroscopeUncal.x;
            y = value.un.gyroscopeUncal.y;
            z = value.un.gyroscopeUncal.z;
            LOG_INF("%8.4f GYRO_UNCAL: "
                   "x:%0.6f y:%0.6f z:%0.6f\n",
                   t,
                   x, y, z);
            break;
        case SH2_GYRO_INTEGRATED_RV:
            // These come at 1kHz, too fast to print all of them.
            // So only print every 10th one
            skip++;
            if (skip == 10) {
                skip = 0;
                r = value.un.gyroIntegratedRV.real;
                i = value.un.gyroIntegratedRV.i;
                j = value.un.gyroIntegratedRV.j;
                k = value.un.gyroIntegratedRV.k;
                x = value.un.gyroIntegratedRV.angVelX;
                y = value.un.gyroIntegratedRV.angVelY;
                z = value.un.gyroIntegratedRV.angVelZ;
                LOG_INF("%8.4f Gyro Integrated RV: "
                       "r:%0.6f i:%0.6f j:%0.6f k:%0.6f x:%0.6f y:%0.6f z:%0.6f\n",
                       t,
                       r, i, j, k,
                       x, y, z);
            }
            break;
        default:
        	LOG_INF("Unknown sensor: %d\n", value.sensorId);
            break;
    }
}

static void sensor_callback(void * cookie, sh2_SensorEvent_t *pEvent)
{
	print_event(pEvent);
}

#if 1
static void sensor_configure(void)
{
    int status = SH2_OK;
    uint32_t config[7];
    const float scaleDegToRad = 3.14159265358 / 180.0;

#define FIX_Q(n, x) ((int32_t)(x * (float)(1 << n)))
#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))
#define GIRV_REF_6AG  (0x0207)  					   // 6 axis Game Rotation Vector
#define GIRV_REF_9AGM (0x0204)  					   // 9 axis Absolute Rotation Vector
#define GIRV_SYNC_INTERVAL (10000)                     // sync interval: 10000 uS (100Hz)
#define GIRV_MAX_ERR FIX_Q(29, (30.0 * scaleDegToRad)) // max error: 30 degrees
#define GIRV_ALPHA FIX_Q(20, 0.303072543909142)        // pred param alpha
#define GIRV_BETA  FIX_Q(20, 0.113295896384921)        // pred param beta
#define GIRV_GAMMA FIX_Q(20, 0.002776219713054)        // pred param gamma
#define GIRV_PRED_AMT FIX_Q(10, 0.0)               	   // prediction amt: 0

    // Note: The call to sh2_setFrs below updates a non-volatile FRS record
    // so it will remain in effect even after the sensor hub reboots.  It's not strictly
    // necessary to repeat this step every time the system starts up as we are doing
    // in this example code.

    // Configure prediction parameters for Gyro-Integrated Rotation Vector.
    // See section 4.3.24 of the SH-2 Reference Manual for a full explanation.
    // ...
    config[0] = GIRV_REF_6AG;           // Reference Data Type
    config[1] = (uint32_t)GIRV_SYNC_INTERVAL; // Synchronization Interval
    config[2] = (uint32_t)GIRV_MAX_ERR;  // Maximum error
    config[3] = (uint32_t)GIRV_PRED_AMT; // Prediction Amount
    config[4] = (uint32_t)GIRV_ALPHA;    // Alpha
    config[5] = (uint32_t)GIRV_BETA;     // Beta
    config[6] = (uint32_t)GIRV_GAMMA;    // Gamma

//    status = sh2_setFrs(GYRO_INTEGRATED_RV_CONFIG, config, ARRAY_LEN(config));
//    if (status != SH2_OK) {
//        LOG_ERR("Error: %d, from sh2_setFrs() in configure().\n", status);
//    }

    // The sh2_setCalConfig does not update non-volatile storage.  This
    // only remains in effect until the sensor hub reboots.

    // Enable dynamic calibration for A, G and M sensors
    status = sh2_setCalConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
    if (status != SH2_OK) {
    	LOG_ERR("Error: %d, from sh2_setCalConfig() in configure().\n", status);
    }
}
#endif

static void start_reports(int sensor)
{
    LOG_INF("Starting Sensor Reports.\n");

    static sh2_SensorConfig_t config = {
    	.changeSensitivityEnabled = false,
    	.wakeupEnabled = false,
    	.changeSensitivityRelative = false,
    	.alwaysOnEnabled = false,
    	.changeSensitivity = 0,
    	.reportInterval_us = 5000000,  // microseconds (100Hz)
    	.batchInterval_us = 0,
    };

    int status = sh2_setSensorConfig(sensor, &config);
    if (status != 0) {
        LOG_ERR("Error while enabling sensor %d\n", sensor);
    }
}

static void get_product_ids(void)
{
    int status, i;

    status = sh2_getProdIds(&product_ids);

    if (status < 0) {
    	LOG_ERR("Error from sh2_getProdIds.\n");
        return;
    }

    for (i = 0; i < product_ids.numEntries; i ++)
    {
        LOG_INF("Part %d : Version %d.%d.%d Build %d\n",
        		product_ids.entry[i].swPartNumber,
				product_ids.entry[i].swVersionMajor,
				product_ids.entry[i].swVersionMinor,
				product_ids.entry[i].swVersionPatch,
				product_ids.entry[i].swBuildNumber);
    }
}

void main(void)
{
    /* Init SH2 layer */
    sh2_initialize(event_callback, NULL);

    /* Register event listener */
    sh2_setSensorCallback(sensor_callback, NULL);

    /* TODO: Optimize this. */
    k_sleep(K_MSEC(5000));

    get_product_ids();

    /* Enable reports for rotation vector. */
    sensor_configure();
    start_reports(SH2_ACCELEROMETER);

    LOG_INF("Recving events..");
    while (true) {
    	sh2_SensorEvent_t event;

		if (k_msgq_get(&app_queue, (void *) &event, K_FOREVER) != 0)
			continue;

		print_event(&event);
    }
}
