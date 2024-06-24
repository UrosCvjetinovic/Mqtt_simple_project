/*******************************************************************************
* Authors:
*    Uros Popovic & Uros Cvjetinovic
* @file
* \brief This program is used to monitor and publish the hysteresis correction
*   value
*******************************************************************************/

/******* include headers ******************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

/******* local macros *********************************************************/
#define ADDRESS         "tcp://broker.hivemq.com:1883"
#define CLIENTID        "Regulator"
#define TOPIC_Y         "CurrentPressure"
#define TOPIC_YZ        "SetPressure"
#define TOPIC_X	        "HysteresisCorrection"
#define QOS             0
#define TIMEOUT         10000L
#define PAYLOADSIZE     ((uint32_t) 50u)
#define DELTA_P		      ((uint32_t) 1u)
#define DEBUGLOG        0

/******* local data objects ***************************************************/
MQTTClient client;

uint32_t connectionLost = 0;
float pressureValueToSet = 0;
float currentPressureValue = 0;

/******* declaration of local functions ***************************************/
void updateHysteresisControlValue(float hysteresisControlToSet);

int messageArrived(void *context, char *topicName, int topicLen,
                   MQTTClient_message *message);

void handleCurrentPressureTopic(void);

void handleSetPressureTopic(float pressureValue);

void connectionLostHandler(void *context, char *cause);

int main(void);

/******* definition of local functions ****************************************/
void updateHysteresisControlValue(float hysteresisControlToSet) 
{
  char payload_x[PAYLOADSIZE];
  MQTTClient_message publishMessage = MQTTClient_message_initializer;
#if (DEBUGLOG)
  printf("Hysteresis control update %f\n", hysteresisControlToSet);
#endif

  snprintf(payload_x, sizeof(payload_x), "%.2f", hysteresisControlToSet);
  publishMessage.payload = payload_x;
  publishMessage.payloadlen = strlen(payload_x);
  publishMessage.qos = QOS;
  publishMessage.retained = 0;
  
  MQTTClient_publishMessage(client, TOPIC_X, &publishMessage, NULL);
}

void handleCurrentPressureTopic(void) 
{
#if (DEBUGLOG)
  printf("Current pressure: %f\n", currentPressureValue);
  printf("Pressure to set: %f\n", pressureValueToSet);
  printf("Pressure difference: %f\n", currentPressureValue - pressureValueToSet);
#endif
  if ((pressureValueToSet > currentPressureValue) && 
      ((pressureValueToSet - currentPressureValue) > DELTA_P))
  {
    updateHysteresisControlValue(100u);
  } 
  else if ((pressureValueToSet < currentPressureValue) &&
           ((currentPressureValue - pressureValueToSet) > DELTA_P))
  {
    updateHysteresisControlValue(0u);
  }
}

void handleSetPressureTopic(float pressureValue) 
{
#if (DEBUGLOG)
  printf("SetPressure: %f\n", currentPressureValue);
#endif
  pressureValueToSet = pressureValue;
}

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message) 
{
  char* payload = (char*) malloc(message->payloadlen + 1);
  float messageValue = atof(payload);

  printf("Message arrived on topic: %s\n", topicName);

  memcpy(payload, message->payload, message->payloadlen);
  payload[message->payloadlen] = '\0';

#if (DEBUGLOG)
  printf("Received message: %s\n", payload);
  printf("Converted float: %f\n", messageValue);
#endif

  if (strcmp(topicName, TOPIC_Y) == 0)
  {
    handleCurrentPressureTopic();
  } 
  else if (strcmp(topicName, TOPIC_YZ) == 0) 
  {
    handleSetPressureTopic(messageValue);
  }

  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);

  return 1;
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
  int result = 0;
  int numberOfConnectRetries = 100u;
  MQTTClient_connectOptions connectionOptions = MQTTClient_connectOptions_initializer;

  MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE,
                    NULL);
                    
  connectionOptions.keepAliveInterval = 20;
  connectionOptions.cleansession = 1;

  MQTTClient_setCallbacks(client, NULL, connectionLostHandler,
                          messageArrivedHandler, NULL);

  result = MQTTClient_connect(client, &connectionOptions);
  if (result != MQTTCLIENT_SUCCESS) 
  {
    printf("Failed to connect, return code %d\n", result);
    return -1;
  }

  printf("Subscribing to topics:\n1. %s\n2. %s\nfor client %s using QoS%d\n\n",
         TOPIC_Y, TOPIC_YZ, CLIENTID, QOS);

  result = MQTTClient_subscribe(client, TOPIC_Y, QOS);
  if (result != MQTTCLIENT_SUCCESS) 
  {
    printf("Failed to subscribe to topic %s, return code %d\n",
           TOPIC_Y, result);
    return -1;
  }

  result = MQTTClient_subscribe(client, TOPIC_YZ, QOS);
  if (result != MQTTCLIENT_SUCCESS) 
  {
    printf("Failed to subscribe to topic %s, return code %d\n", 
           TOPIC_YZ, result);
    return -1;
  }

  while (1)
  {
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

  MQTTClient_disconnect(client, 10000);
  MQTTClient_destroy(&client);
  return 0;
}
