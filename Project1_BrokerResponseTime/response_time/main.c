/*******************************************************************************
* Authors:
*    Uros Popovic & Uros Cvjetinovic
* @file
* \brief This program is used to test response time of broker
*******************************************************************************/

/******* include headers ******************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include "MQTTClient.h"

/******* local macros *********************************************************/
#define ADDRESS                "tcp://broker.hivemq.com:1883"
#define CLIENTID               "ResponseCheck"
#define TOPIC                  "ResponseTest"
#define QOS                    2
#define TIMEOUT                10000L
#define NUMBEROFMSGTOSEND      ((uint32_t) 100u)
#define DEBUGLOG               0

/******* local data objects ***************************************************/
MQTTClient client;
volatile MQTTClient_deliveryToken deliveredtoken;
LARGE_INTEGER startTime;
LARGE_INTEGER endTime;
LARGE_INTEGER frequency;
double totalTime = 0;
uint32_t connectionLost = 0;
uint32_t messagePublished = 0;
uint32_t messageArrived = 0;

/******* declaration of local functions ***************************************/
void delivered(void *context, MQTTClient_deliveryToken deliveryToken);

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message);
             
void connectionLostHandler(void *context, char *cause);

int main(int argc, char* argv[]);

/******* definition of local functions ****************************************/
void tokenDeliveredHandler(void *context, MQTTClient_deliveryToken deliveryToken) 
{
  QueryPerformanceCounter(&endTime);

  double elapsedTime = (double)(endTime.QuadPart - startTime.QuadPart) *
                        1000.0 / frequency.QuadPart;
  totalTime += elapsedTime;
  messageArrived++;
#if (DEBUGLOG)
  printf("Message with token value %d delivery confirmed\n", deliveryToken);
  printf("-Response time: %.2f ms\n", elapsedTime);
#endif
  deliveredtoken = deliveryToken;
}

int messageArrivedHandler(void *context, char *topicName, int topicLen,
                          MQTTClient_message *message) 
{
#if (DEBUGLOG)
  printf("Recived message.\n");
#endif
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

int main(int argc, char* argv[]) 
{
  int result = 0;
  MQTTClient_connectOptions connectionOptions = MQTTClient_connectOptions_initializer;
  MQTTClient_message publishMessage = MQTTClient_message_initializer;
  MQTTClient_deliveryToken token;
  int numberOfConnectRetries = 100u;
  char payload[20];
  char userInput;
  
  MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE,
                    NULL);
  connectionOptions.keepAliveInterval = 20;
  connectionOptions.cleansession = 1;
  if (QOS == 2)
  {
    // Needed for QOS 2, add also in hive websocket client
    connectionOptions.username = "Uros";
    connectionOptions.password = "1";
  }
  
  publishMessage.qos = QOS;
  publishMessage.retained = 0;

  MQTTClient_setCallbacks(client, NULL, connectionLostHandler, 
                          messageArrivedHandler, tokenDeliveredHandler);
  
  result = MQTTClient_connect(client, &connectionOptions);
  if (result != MQTTCLIENT_SUCCESS) 
  {
    printf("Failed to connect, return code %d\n", result);
    exit(EXIT_FAILURE);
  }
  
  while(1) 
  {
    QueryPerformanceFrequency(&frequency);

    for (int i = 0; i <= NUMBEROFMSGTOSEND; i++) 
    {
      snprintf(payload, sizeof(payload), "%d", messagePublished++);
      publishMessage.payload = payload;
      publishMessage.payloadlen = (int) strlen(payload);

#if (DEBUGLOG)
      printf("Publication of: of %s\n", payload);
#endif
      QueryPerformanceCounter(&startTime);
      MQTTClient_publishMessage(client, TOPIC, &publishMessage, &token);
      result = MQTTClient_waitForCompletion(client, token, TIMEOUT);

      if(result = MQTTCLIENT_SUCCESS)
      {
#if (DEBUGLOG)
        printf("Message with delivery token %d delivered\n", token);
#endif
      }
      else if (QOS == 0)
      {
        // Since QOS0 doesn't provide message arrived, we can only check if delivered
        QueryPerformanceCounter(&endTime);
      
        double elapsedTime = (double)(endTime.QuadPart - startTime.QuadPart) *
                              1000.0 / frequency.QuadPart;
        totalTime += elapsedTime;
        messageArrived++;
#if (DEBUGLOG)
        printf("Message arrived based on completion for QOS0.\n");
        printf("-Response time: %.2f ms\n", elapsedTime);
#endif
      }
      
      if ((messagePublished >= 256u) && (i != NUMBEROFMSGTOSEND))
      {
        messagePublished = 0u;
      }
      
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
          printf("Failed to connect, return code %d\n", result);
          exit(EXIT_FAILURE);
        }
        else
        {
          connectionLost = 0u;
        }
      }
    }
  
    if (messageArrived > 0) 
    {
      printf("For QOS %d :\n", QOS);
      printf("Average response time: %.2f ms\n", totalTime / messageArrived);
      printf("Press Q key to quit, or any other to continue.\n");
      scanf("%c", &userInput);
      if(userInput == 81u || userInput == 113u) 
      {
        break;
      }
    }
  }

  MQTTClient_disconnect(client, 10000);
  MQTTClient_destroy(&client);

  return result;
}
