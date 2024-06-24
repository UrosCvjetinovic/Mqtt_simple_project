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
#define TOPIC_Y         "CurrentPressure"
#define TOPIC_X	        "HysteresisCorrection"
#define QOS             0
#define TIMEOUT         10000L
#define TIMERPERIOD     1000L
#define PAYLOADSIZE     ((uint32_t) 50u)
#define DEBUGLOG        0

/******* local data objects ***************************************************/
MQTTClient client;

uint32_t connectionLost = 0;
float hysteresis_correction[2u] = {0};
float pressure[2u] = {0};

/******* declaration of local functions ***************************************/
void updatePressureValue(float pressureValue);

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message);

void CALLBACK timerCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent,
                            DWORD dwTime);
                    
void connectionLostHandler(void *context, char *cause);
        
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
#if (DEBUGLOG)
    printf("Message published: %s\n", payload);
#endif
    result = MQTTClient_waitForCompletion(client, token, TIMEOUT);
#if (DEBUGLOG)
    printf("Message with delivery token %d delivered\n", token);
#endif
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
#if (DEBUGLOG)
  printf("x[n] = %f\n", hysteresis_correction[0u]);
  printf("x[n-1] = %f\n", hysteresis_correction[1u]);
  printf("y[n-1] = %f\n", pressure[0u]);
#endif
  pressure[1u] = ((1.0/21.0) * 
                  (hysteresis_correction[1u] + hysteresis_correction[0u])) +
                 (19.0/21.0) * pressure[0u];

  pressure[0u] = pressure[1u];
  hysteresis_correction[0u] = hysteresis_correction[1u];

  printf("y[n] = %f\n", pressure[1u]);
  updatePressureValue(pressure[1u]);
}

void connectionLostHandler(void *context, char *cause) 
{
  connectionLost = 1u;
#if (DEBUGLOG)
  printf("\nConnection lost\n");
  printf("-Cause: %s\n", cause);
#endif
}

int main() 
{
  int result;
  int numberOfConnectRetries = 100u;
  MQTTClient_connectOptions connectionOptions = MQTTClient_connectOptions_initializer;
  MSG receivedMessage;
  UINT_PTR timerId;

  pressure[0u] = 100u;
  pressure[1u] = 0u;
  
  hysteresis_correction[0u] = 0u;
  hysteresis_correction[1u] = 0u;

  MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE,
                    NULL);
  MQTTClient_setCallbacks(client, NULL, connectionLostHandler,
                          messageArrivedHandler, NULL);
  
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
    
    if (connectionLost == 1u)
    {
      result = MQTTCLIENT_FAILURE;
      while ((result != MQTTCLIENT_SUCCESS) && (numberOfConnectRetries > 0))
      {
        result = MQTTClient_connect(client, &connectionOptions);
        numberOfConnectRetries--;
      }
      
      if (result != MQTTCLIENT_SUCCESS) 
      {
        printf("Failed to reconnect, return code %d\n", result);
        exit(EXIT_FAILURE);
      }
      else
      {
        connectionLost = 0u;
      }
    }
  }

  KillTimer(NULL, timerId);

  MQTTClient_disconnect(client, TIMEOUT);
  MQTTClient_destroy(&client);

  return 0;
}