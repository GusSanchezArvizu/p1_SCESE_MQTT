/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2022 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "mqtt_freertos.h"

#include "board.h"
#include "stdint.h"
#include "fsl_silicon_id.h"

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/apps/mqtt.h"
#include "lwip/tcpip.h"

#include <time.h>
#include <stdio.h>

// FIXME cleanup

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief MQTT server host name or IP address. */
#ifndef EXAMPLE_MQTT_SERVER_HOST
#define EXAMPLE_MQTT_SERVER_HOST "industrial.api.ubidots.com"
#endif

/*! @brief MQTT server port number. */
#ifndef EXAMPLE_MQTT_SERVER_PORT
#define EXAMPLE_MQTT_SERVER_PORT 1883
#endif

/*! @brief Stack size of the temporary lwIP initialization thread. */
#define INIT_THREAD_STACKSIZE 1024

/*! @brief Priority of the temporary lwIP initialization thread. */
#define INIT_THREAD_PRIO DEFAULT_THREAD_PRIO

/*! @brief Stack size of the temporary initialization thread. */
#define APP_THREAD_STACKSIZE 1024

/*! @brief Priority of the temporary initialization thread. */
#define APP_THREAD_PRIO DEFAULT_THREAD_PRIO


#define BUFFER_SIZE 32

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static void connect_to_mqtt(void *ctx);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*! @brief MQTT client data. */
static mqtt_client_t *mqtt_client;

/*! @brief MQTT client ID string. */
static char client_id[(SILICONID_MAX_LENGTH * 2) + 5];

/*! @brief MQTT client information. */
static const struct mqtt_connect_client_info_t mqtt_client_info = {
    .client_id   = (const char *)&client_id[0],
    .client_user = "BBUS-D9r32SVvY22pmqw9vYLcE7CEHl3hus",
    .client_pass = "a",
    .keep_alive  = 100,
    .will_topic  = NULL,
    .will_msg    = NULL,
    .will_qos    = 0,
    .will_retain = 0,
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    .tls_config = NULL,
#endif
};

/*! @brief MQTT broker IP address. */
static ip_addr_t mqtt_addr;

/*! @brief Indicates connection to MQTT broker. */
static volatile bool connected = false;

static volatile uint8_t sec_count = 0;
static volatile uint8_t min_count = 0;

static volatile u8_t topic_value = 0;
static volatile char* topic_name ="";

static const char* PTC_topic = "/v1.6/devices/iteso_test_1/freq/lv";
static const char* PTA_topic = "/v1.6/devices/iteso_test_1/alarm/lv";
static const char* PTF_topic = "/v1.6/devices/iteso_test_1/flash/lv";
static const char* PTE_topic = "/v1.6/devices/iteso_test_1/mode/lv";
//static const char* PTM_topic = "/v1.6/devices/iteso_test_1/main/lv";

static char* temperature = 	"/v1.6/devices/iteso_test_1/temperature";
static char* uv = 			"/v1.6/devices/iteso_test_1/uv_ level";
static char* rH = 			"/v1.6/devices/iteso_test_1/r_humidity";
static char* noise = 		"/v1.6/devices/iteso_test_1/noise_db";
//static char* aqi = 			"/v1.6/devices/iteso_test_1/aqi";

static volatile u8_t PTC;
static volatile u8_t PTA;
static volatile u8_t PTF;
static volatile u8_t PTE;

static char message[128];

u16_t data[1][4];

typedef struct
{
	char topic[256];
	char msg[128];
}publish_pack_t;

typedef struct
{
	volatile u8_t value;
	char *param;
}incoming_sub_t;

publish_pack_t msg_p;
incoming_sub_t msg_r;
/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @bref Function made to simulate the tx of data form sensors
 */
int generate_random(int min, int max) {
    return rand() % (max - min + 1) + min;
}

void receive_sensor_data()
{
    data[0][0] = generate_random(8, 45); //Temperatura
    data[0][1] = generate_random(0, 100); //Humedad
    data[0][2] = generate_random(0, 150); //Ruido
    data[0][3] = generate_random(0, 7); //Uv
}


void doSomething(char* topic, u8_t value)
{
	if(!(strcmp(topic,PTC_topic)))
	{
		if(value>0 && value <=60)
		{
			PTC = value;
			PRINTF(" - ACTION - : PTC PARAMETER CHANGED\r\n");
		}
		else
		{
			PRINTF(" - ACTION - : INVALID PTC PARAMETER\r\n");
		}
	}
	else if (!(strcmp(topic,PTA_topic)))
	{
		if(value == 1)
		{
			//Enciende alarma
			PRINTF(" - ACTION - : AAAAAAAALAAAAAAARMAAAAAAAAAAA\r\n");

		}
		else
		{
			//Apaga alarma
			PRINTF(" - ACTION - : SE APAGA ALARMA\r\n");
		}
	}
	else if (!(strcmp(topic,PTF_topic)))
	{
		if(value == 1)
		{
			PRINTF(" - ACTION - : FLASH ON\r\n");
		}
		else
		{
			PRINTF(" - ACTION - : FLASH OFF\r\n");
		}
	}
	else if (!(strcmp(topic,PTE_topic)))
	{
		if(value == 1)
		{
			//Se duerme
			PRINTF(" - ACTION - : MODO AHORRO ENERGIA\r\n");
		}
		else
		{
			//Modo normal
			PRINTF(" - ACTION - : MODO NORMAL ENERGIA\r\n");
		}
	}
	else
	{
		PRINTF(" - WARNING - : ALGO EXTRANO OCURRIO\r\n");
	}
}

u8_t checkSubscritions(const u8_t* data, u16_t length)
{
    char *dot_pos = strchr(data, '.');
    if (dot_pos != NULL) {
        *dot_pos = '\0';
    }
    return atoi(data);
}
/*!
 * @brief Called when subscription request finishes.
 */
static void mqtt_topic_subscribed_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        PRINTF("Subscribed to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to subscribe to the topic \"%s\": %d.\r\n", topic, err);
    }
}

/*!
 * @brief Called when there is a message on a subscribed topic.
 */
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    LWIP_UNUSED_ARG(arg);
    PRINTF("Received %u bytes from the topic \"%s\": \"", tot_len, topic);
    msg_r.param = malloc(strlen(topic) + 1);
    strcpy(msg_r.param,topic);
}

/*!
 * @brief Called when recieved incoming published message fragment.
 */
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    LWIP_UNUSED_ARG(arg);

    msg_r.value = checkSubscritions(data,len);

    doSomething(msg_r.param,msg_r.value);

    if (flags & MQTT_DATA_FLAG_LAST)
    {
        PRINTF("\"\r\n");
    }
}

/*!
 * @brief Subscribe to MQTT topics.
 */
static void mqtt_subscribe_topics(mqtt_client_t *client)
{
    static const char *topics[] = { "/v1.6/devices/iteso_test_1/freq/lv",
    								"/v1.6/devices/iteso_test_1/alarm/lv",
    								"/v1.6/devices/iteso_test_1/flash/lv",
    								"/v1.6/devices/iteso_test_1/mode/lv"};
    int qos[]                   = {0,0,0,0};
    err_t err;
    int i;
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb,
                            LWIP_CONST_CAST(void *, &mqtt_client_info));

    for (i = 0; i < ARRAY_SIZE(topics); i++)
    {
        err = mqtt_subscribe(client, topics[i], qos[i], mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, topics[i]));

        if (err == ERR_OK)
        {
            //PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topics[i], qos[i]);
        }
        else
        {
            PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topics[i], qos[i], err);
        }
    }
}

/*!
 * @brief Called when connection state changes.
 */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

    connected = (status == MQTT_CONNECT_ACCEPTED);

    switch (status)
    {
        case MQTT_CONNECT_ACCEPTED:
            PRINTF("MQTT client \"%s\" connected.\r\n", client_info->client_id);
            mqtt_subscribe_topics(client);
            break;

        case MQTT_CONNECT_DISCONNECTED:
            PRINTF("MQTT client \"%s\" not connected.\r\n", client_info->client_id);
            /* Try to reconnect 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_TIMEOUT:
            PRINTF("MQTT client \"%s\" connection timeout.\r\n", client_info->client_id);
            /* Try again 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
        case MQTT_CONNECT_REFUSED_SERVER:
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
            PRINTF("MQTT client \"%s\" connection refused: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;

        default:
            PRINTF("MQTT client \"%s\" connection status: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;
    }
}

/*!
 * @brief Starts connecting to MQTT broker. To be called on tcpip_thread.
 */
static void connect_to_mqtt(void *ctx)
{
    LWIP_UNUSED_ARG(ctx);

    PRINTF("Connecting to MQTT broker at %s...\r\n", ipaddr_ntoa(&mqtt_addr));

    mqtt_client_connect(mqtt_client, &mqtt_addr, EXAMPLE_MQTT_SERVER_PORT, mqtt_connection_cb,
                        LWIP_CONST_CAST(void *, &mqtt_client_info), &mqtt_client_info);
}

/*!
 * @brief Called when publish request finishes.
 */
static void mqtt_message_published_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        //PRINTF("Published to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to publish to the topic \"%s\": %d.\r\n", topic, err);
    }
}

/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_message(void *ctx)
{

    int i;
    err_t err;
    publish_pack_t *pack = (publish_pack_t *)ctx;
    const char *topic = pack->topic;  // Ahora sí puedes acceder a topic
    const char *message = pack->msg;

    	err = mqtt_publish(mqtt_client, topic, message, strlen(message), 0, 0, mqtt_message_published_cb, (void *)topic);
        if (err == ERR_OK)
        {
            PRINTF("Publishing to the topic \"%s\" the message: \"%s\"...\r\n", topic, message);
        }
        else
        {
            PRINTF("Failed to publish to the topic \"%s\" with err %d\r\n", topic, err);
        }

}
/*!
 * @brief Task for sensing (UART)
 */
void vTaskSec(void *pvParameters) {
    err_t err;
    char message[10];
	static const char *topic[]   = {"/v1.6/devices/iteso_test_1/temperature",
			"/v1.6/devices/iteso_test_1/r_humidity",
			"/v1.6/devices/iteso_test_1/noise_db",
			"/v1.6/devices/iteso_test_1/uv_level"
			};

    while (1)
    {
    	vTaskDelay(pdMS_TO_TICKS(1000)); // Espera 1000 ms (1 segundo)
    	sec_count++;
    	uint8_t isPtcTime = sec_count%PTC;

    	if(isPtcTime == 0)
    	{
			PRINTF(" - vTaskSec - : DATA\r\n");
    		receive_sensor_data();

    		if (connected)
			{
				for (int i = 0; i < ARRAY_SIZE(topic); i++)
				{
					snprintf(msg_p.msg, sizeof(msg_p.msg), "%u", data[0][i]);
					strcpy(msg_p.topic, topic[i]);

					vTaskDelay(100);
					err = tcpip_callback(publish_message, (void*)&msg_p);
					if (err != ERR_OK)
					{
						PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
					}
				}
			}
            //printf("Tarea ejecutándose cada segundo...\n");
    	}
    	if(sec_count == 60)
    	{
    		sec_count = 0;
    	}
    }
}

/*!
 * @brief Application thread.
 */
static void app_thread(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    err_t err;
    int i;

    PRINTF("\r\nIPv4 Address     : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
    PRINTF("IPv4 Subnet mask : %s\r\n", ipaddr_ntoa(&netif->netmask));
    PRINTF("IPv4 Gateway     : %s\r\n\r\n", ipaddr_ntoa(&netif->gw));

    /*
     * Check if we have an IP address or host name string configured.
     * Could just call netconn_gethostbyname() on both IP address or host name,
     * but we want to print some info if goint to resolve it.
     */
    if (ipaddr_aton(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr) && IP_IS_V4(&mqtt_addr))
    {
        /* Already an IP address */
        err = ERR_OK;
    }
    else
    {
        /* Resolve MQTT broker's host name to an IP address */
        PRINTF("Resolving \"%s\"...\r\n", EXAMPLE_MQTT_SERVER_HOST);
        err = netconn_gethostbyname(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr);
    }

    if (err == ERR_OK)
    {
        /* Start connecting to MQTT broker from tcpip_thread */
        err = tcpip_callback(connect_to_mqtt, NULL);
        if (err != ERR_OK)
        {
            PRINTF("Failed to invoke broker connection on the tcpip_thread: %d.\r\n", err);
        }
    }
    else
    {
        PRINTF("Failed to obtain IP address: %d.\r\n", err);
    }

//    /* Publish some messages */
//    for (i = 0; i < 5;)
//    {
//        if (connected)
//        {
//            err = tcpip_callback(publish_message, NULL);
//            if (err != ERR_OK)
//            {
//                PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
//            }
//            i++;
//        }
//
//        sys_msleep(1000U);
//    }

    vTaskDelete(NULL);
}

static void generate_client_id(void)
{
    uint8_t silicon_id[SILICONID_MAX_LENGTH];
    const char *hex = "0123456789abcdef";
    status_t status;
    uint32_t id_len = sizeof(silicon_id);
    int idx         = 0;
    int i;
    bool id_is_zero = true;

    /* Get unique ID of SoC */
    status = SILICONID_GetID(&silicon_id[0], &id_len);
    assert(status == kStatus_Success);
    assert(id_len > 0U);
    (void)status;

    /* Covert unique ID to client ID string in form: nxp_hex-unique-id */

    /* Check if client_id can accomodate prefix, id and terminator */
    assert(sizeof(client_id) >= (5U + (2U * id_len)));

    /* Fill in prefix */
    client_id[idx++] = 'n';
    client_id[idx++] = 'x';
    client_id[idx++] = 'p';
    client_id[idx++] = '_';

    /* Append unique ID */
    for (i = (int)id_len - 1; i >= 0; i--)
    {
        uint8_t value    = silicon_id[i];
        client_id[idx++] = hex[value >> 4];
        client_id[idx++] = hex[value & 0xFU];

        if (value != 0)
        {
            id_is_zero = false;
        }
    }

    /* Terminate string */
    client_id[idx] = '\0';

    if (id_is_zero)
    {
        PRINTF(
            "WARNING: MQTT client id is zero. (%s)"
#ifdef OCOTP
            " This might be caused by blank OTP memory."
#endif
            "\r\n",
            client_id);
    }
}

/*!
 * @brief Create and run example thread
 *
 * @param netif  netif which example should use
 */
void mqtt_freertos_run_thread(struct netif *netif)
{
    LOCK_TCPIP_CORE();
    mqtt_client = mqtt_client_new();
    UNLOCK_TCPIP_CORE();
    if (mqtt_client == NULL)
    {
        PRINTF("mqtt_client_new() failed.\r\n");
        while (1)
        {
        }
    }
    srand(time(NULL));

    generate_client_id();

    if (sys_thread_new("app_task", app_thread, netif, APP_THREAD_STACKSIZE, APP_THREAD_PRIO) == NULL)
    {
        LWIP_ASSERT("mqtt_freertos_start_thread(): Task creation failed.", 0);
    }

    if(sys_thread_new("vTaskSec", vTaskSec, netif, APP_THREAD_STACKSIZE, APP_THREAD_PRIO) == NULL)
    {
    	LWIP_ASSERT("vTaskSec(): Task creation failed.", 0);
    }

    if(sys_thread_new("vTaskSub", vTaskSub, netif, APP_THREAD_STACKSIZE, APP_THREAD_PRIO) == NULL)
    {
    	LWIP_ASSERT("vTaskSub(): Task creation failed.", 0);
    }
}
