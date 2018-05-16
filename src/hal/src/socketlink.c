/*
 *    ||          ____  _ __                           
 * +------+      / __ )(_) /_______________ _____  ___ 
 * | 0xBC |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * +------+    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *  ||  ||    /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie simulation firmware
 *
 * Copyright (c) 2018  Eric Goubault, Sylvie Putot, Franck Djeumou
 *             Cosynux , LIX , France
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * socketlink.c: Socket implementation of the CRTP link
 */
#define DEBUG_MODULE "SOCKET_LINK"
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "socketlink.h"
#include "crtp.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "queuemonitor.h"
#include "semphr.h"
#include "debug.h"
#include "cfassert.h"

#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

#include <errno.h>

// Global variables for socket management
static struct sockaddr_in myaddr;
static struct sockaddr_in remaddr;
static socklen_t addrlen;
static int fd;
static struct pollfd fds[1];


static bool isInit = false;
static xQueueHandle crtpPacketDelivery;

static int socketlinkSendPacket(CRTPPacket *p);
static int socketlinkSetEnable(bool enable);
static int socketlinkReceiveCRTPPacket(CRTPPacket *p);


static struct crtpLinkOperations socketlinkOp =
{
  .setEnable         = socketlinkSetEnable,
  .sendPacket        = socketlinkSendPacket,
  .receivePacket     = socketlinkReceiveCRTPPacket,
};

static CRTPPacket p;

static void socketlinkTask(void *param)
{
  int recvlen;
  while(1)
  {
    while (poll(&fds[0], (sizeof(fds[0]) / sizeof(fds[0])), -1) < 0){
      // Failed when no data because of EINTR caused by FreeRTOS linux port)
      // Workaround is to put a delay for the moment
      vTaskDelay(M2T(1));
    }

    // Check if any data was received
    if (fds[0].revents & POLLIN){
      // Fetch a socket data
      recvlen = recvfrom(fd, p.raw, sizeof(p.raw), 0, (struct sockaddr *)&remaddr, &addrlen);
      if (recvlen > 0){
        p.size = recvlen - 1; // We remove the header size
        xQueueSend(crtpPacketDelivery, &p, 0);
      } else {
        DEBUG_PRINT("error : %s \n" , strerror(errno));
      }
    } 
  }

}

static int socketlinkReceiveCRTPPacket(CRTPPacket *p)
{
  if (xQueueReceive(crtpPacketDelivery, p, M2T(100)) == pdTRUE)
  {
    return 0;
  }

  return -1;
}

static int socketlinkSendPacket(CRTPPacket *p)
{
  int dataSize;

  ASSERT(p->size < 32);

  if (p->size > CRTP_MAX_DATA_SIZE)
    return 0;

  dataSize = p->size + 1;
  dataSize = sendto(fd, p->raw, dataSize, 0, (struct sockaddr *)&remaddr, addrlen);
  // Shutdown if not able to send ???
  return (dataSize > 0);
}

static int socketlinkSetEnable(bool enable)
{
  return 0;
}

/*
 * Public functions
 */
void socketlinkInit()
{
  if(isInit)
    return;

  // Create UDP socket
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    isInit = false;
    DEBUG_PRINT("cannot create socket\n");
    ASSERT_FAILED();
    return;
  }
  DEBUG_PRINT("Create socket succeed \n");
  
  // Bind the UDP socket
  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  if (strcmp(address_host, "INADDR_ANY") == 0){
    myaddr.sin_addr.s_addr =  htonl(INADDR_ANY);
  } else if (inet_addr(address_host) == INADDR_NONE){
    isInit = false;
    return;
  } else{
    myaddr.sin_addr.s_addr = inet_addr(address_host); // Set the address if valid
  }
  myaddr.sin_port = htons(crtp_port); // Set the port

  //Initialize addrlen
  addrlen =  sizeof(remaddr);

  if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0){
    isInit =false;
    DEBUG_PRINT("Binding failed \n");
    ASSERT_FAILED();
    return;
  }
  DEBUG_PRINT("Binding succeed \n");

  // initialize the poll structure
  fds[0].fd = fd;
  fds[0].events = POLLIN;

  // Create RX queue and start socketlink task
  crtpPacketDelivery = xQueueCreate(5, sizeof(CRTPPacket));
  DEBUG_QUEUE_MONITOR_REGISTER(crtpPacketDelivery);

  xTaskCreate(socketlinkTask, USBLINK_TASK_NAME,
              USBLINK_TASK_STACKSIZE, NULL, USBLINK_TASK_PRI, NULL);

  isInit = true;
}

bool socketlinkTest()
{
  return isInit;
}

struct crtpLinkOperations * socketlinkGetLink()
{
  return &socketlinkOp;
}

