/*******************************************************************************
* Authors:
*    Uros Popovic & Uros Cvjetinovic
* @file
* \brief This program is used to test response time
*******************************************************************************/

/******* include headers ******************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "MQTTClient.h"

/******* local macros *********************************************************/
#define ADDRESS       "tcp://broker.hivemq.com:1883"
#define CLIENTID      "ResponseCheck"
#define TOPIC         "ResponseTest"
#define QOS           2
#define TIMEOUT       10000L

/******* local data objects ***************************************************/
volatile MQTTClient_deliveryToken deliveredtoken;
LARGE_INTEGER startTime;
LARGE_INTEGER endTime;
LARGE_INTEGER frequency;
double totalTime = 0;
int messageCount = 0;

/******* declaration of local functions ***************************************/
void delivered(void *context, MQTTClient_deliveryToken deliveryToken);

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message);
             
void connectionLostHandler(void *context, char *cause);

int main(int argc, char* argv[]);

/******* definition of local functions ****************************************/
void tokenDeliveredHandler(void *context, MQTTClient_deliveryToken deliveryToken) 
{
  printf("Message with token value %d delivery confirmed\n", deliveryToken);  // TODO: remove
  deliveredtoken = deliveryToken;
}

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message) 
{
  QueryPerformanceCounter(&endTime);

  double elapsedTime = (double)(endTime.QuadPart - startTime.QuadPart) *
                        1000.0 / frequency.QuadPart;
  totalTime += elapsedTime;
  messageCount++;
  printf("Recived:\n");  // TODO: remove
  printf("-Topic: %s\n", topicName);  // TODO: remove
  printf("-Message: %.*s\n", message->payloadlen, (char*)message->payload);  // TODO: remove
  printf("-Response time: %.2f ms\n", elapsedTime);  // TODO: remove

  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
  return 1;
}

void connectionLostHandler(void *context, char *cause) 
{
  printf("\nConnection lost\n");
  printf("-Cause: %s\n", cause);
}

int main(int argc, char* argv[]) 
{
  int result = 0;
  
  while(1) 
  {
    MQTTClient client;
    MQTTClient_connectOptions connectionOptions = MQTTClient_connectOptions_initializer;
    MQTTClient_message publishMessage = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE,
                      NULL);
    connectionOptions.keepAliveInterval = 20;
    connectionOptions.cleansession = 1;

    MQTTClient_setCallbacks(client, NULL, connectionLostHandler, 
                            messageArrivedHandler, tokenDeliveredHandler);
    
    result = MQTTClient_connect(client, &connectionOptions);
    if (result != MQTTCLIENT_SUCCESS) 
    {
      printf("Failed to connect, return code %d\n", result);
      exit(EXIT_FAILURE);
    }
    
    MQTTClient_subscribe(client, TOPIC, QOS);
    QueryPerformanceFrequency(&frequency);

    for (int i = 0; i <= 100; i++) 
    {
      char payload[20];
      snprintf(payload, sizeof(payload), "%d", i);
      publishMessage.payload = payload;
      publishMessage.payloadlen = strlen(payload);
      publishMessage.qos = QOS;
      publishMessage.retained = 0;

      QueryPerformanceCounter(&startTime);
      MQTTClient_publishMessage(client, TOPIC, &publishMessage, &token);
      printf("Waiting for publication of %s\n", payload);  // TODO: remove
      result = MQTTClient_waitForCompletion(client, token, TIMEOUT);
      printf("Message with delivery token %d delivered\n", token);  // TODO: remove
      Sleep(10);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    if (messageCount > 0) 
    {
      printf("Average response time: %.2f ms\n", totalTime / messageCount);
      Sleep(100);
    }
  }
  
  return result;
}
