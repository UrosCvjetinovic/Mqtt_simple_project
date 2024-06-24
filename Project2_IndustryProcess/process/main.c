/*******************************************************************************
* Authors:
*    Uros Cvjetinovic & Uros Popovic
* @file
* \brief This program is used to simulate an industry process modeled by a 
*  system of first order with time constant of 10s and gain 1. The differential
*  describing the process is:
*  y_n = ((1.0/21.0) * (x_n + x_n-1)) + (19.0/21.0) * y_n-1
*  where y represents pressure, and x 
*******************************************************************************/
 
/******* include headers ******************************************************/
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

/******* local macros *********************************************************/
#define ADDRESS         "broker.hivemq.com:1883"
#define CLIENTID        "IndustryProcess"
#define TOPIC_Y         "UpdateCurrentPressure"
#define TOPIC_X	        "UpateHysteresisCorrection"
#define QOS             0
#define TIMEOUT         10000L
#define TIMERPERIOD     1000L
#define PAYLOADSIZE     ((uint32_t) 50u)

/******* local data objects ***************************************************/
MQTTClient client;

float hysteresis_correction[2u] = {0};
float pressure[2u] = {0};

/******* declaration of local functions ***************************************/
void updatePressureValue(float pressureValue);

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message);

void CALLBACK timerCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent,
                            DWORD dwTime);
                            
int main(void);

/******* definition of local functions ****************************************/
void updatePressureValue(float pressureValue) 
{
  int result;
  char payload[PAYLOADSIZE];
  MQTTClient_deliveryToken token;
  MQTTClient_message publishMessage = MQTTClient_message_initializer;
  
  publishMessage.payload = payload;
  publishMessage.payloadlen = strlen(payload);
  publishMessage.qos = QOS;
  publishMessage.retained = 0;
  
  snprintf(payload, sizeof(payload), "%.2f", pressureValue);

  result = MQTTClient_publishMessage(client, TOPIC_Y, &publishMessage, &token);
  if (result != MQTTCLIENT_SUCCESS) 
  {
    printf("Failed to publish message, return code %d\n", result);
  }
  else 
  {
    printf("Message published: %s\n", payload);
    result = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", token);
  }
}

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message) 
{
  if (strcmp(topicName, TOPIC_X) == 0) 
  {
    sscanf((char *)message->payload, "%f", &hysteresis_correction[1u]);
    printf("Received xn: %.2f\n", hysteresis_correction[1u]);
  }
  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
  return 1;
}

void CALLBACK timerCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent,
                            DWORD dwTime) 
{
  printf("x[n] = %f\n", hysteresis_correction[0u]);
  printf("x[n-1] = %f\n", hysteresis_correction[1u]);
  printf("y[n-1] = %f\n", pressure[0u]);
  
  pressure[1u] = ((1.0/21.0) * 
                  (hysteresis_correction[1u] + hysteresis_correction[0u])) +
                 (19.0/21.0) * pressure[0u];

  pressure[0u] = pressure[1u];
  hysteresis_correction[0u] = hysteresis_correction[1u];

  printf("y[n] = %f\n", pressure[1u]);
  updatePressureValue(pressure[1u]);
}

int main() 
{
  int result;
  MQTTClient_connectOptions connectionOptions = MQTTClient_connectOptions_initializer;
  MSG receivedMessage;
  UINT_PTR timerId;

  pressure[0u] = 100u;
  pressure[1u] = 0u;
  
  hysteresis_correction[0u] = 0u;
  hysteresis_correction[1u] = 0u;

  MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE,
                    NULL);
  MQTTClient_setCallbacks(client, NULL, NULL, messageArrivedHandler, NULL);
  
  connectionOptions.keepAliveInterval = 20;
  connectionOptions.cleansession = 1;
  
  result = MQTTClient_connect(client, &connectionOptions);
  if (result != MQTTCLIENT_SUCCESS) 
  {
    printf("Failed to connect, return code %d\n", result);
    return 1;
  }
  MQTTClient_subscribe(client, TOPIC_X, QOS);

  timerId = SetTimer(NULL, 0, TIMERPERIOD, (TIMERPROC)timerCallback);
  if (timerId == 0) 
  {
    printf("Failed to create timer.\n");
    return 1;
  }

  while (GetMessage(&receivedMessage, NULL, 0, 0)) 
  {
    TranslateMessage(&receivedMessage);
    DispatchMessage(&receivedMessage);
  }

  KillTimer(NULL, timerId);

  MQTTClient_disconnect(client, TIMEOUT);
  MQTTClient_destroy(&client);

  return 0;
}