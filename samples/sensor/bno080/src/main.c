/*
 * Copyright (c) 2021 Entropic Engg
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/printk.h>

#include <string.h>

#include "sh2.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"

static sh2_ProductIds_t product_ids;

static void event_callback(void * cookie, sh2_AsyncEvent_t *pEvent)
{
	/* TODO */
}

static void sensor_callback(void * cookie, sh2_SensorEvent_t *pEvent)
{
	/* TODO */
}

static void get_product_ids(void)
{
    int status, i;

    status = sh2_getProdIds(&product_ids);

    if (status < 0) {
        printk("Error from sh2_getProdIds.\n");
        return;
    }

    for (i = 0; i < product_ids.numEntries; i ++)
    {
        printk("Part %d : Version %d.%d.%d Build %d\n",
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

    get_product_ids();

}
