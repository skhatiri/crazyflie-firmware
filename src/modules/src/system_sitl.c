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
 * system_sitl.c - Top level module implementation based on system.c
                   We remove low level dependencies and start adequate modules
 */
#define DEBUG_MODULE "SYS"

#include <stdbool.h>

/* FreeRtos includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "debug.h"
#include "version.h"
#include "config.h"
#include "param.h"
#include "log.h"

#include "system.h"
#include "configblock.h"
#include "worker.h"
#include "comm.h"
#include "stabilizer.h"
#include "commander.h"
#include "console.h"
#include "socketlink.h"
#include "mem.h"
#include "queuemonitor.h"
#include "buzzer.h"
#include "sysload.h"

/* Private variable */
static bool selftestPassed;
static bool canFly;
static bool isInit;

// Constant battery voltage --> Needed for crazyflie ros logging
static const float batteryVoltage = 4.20;

/* System wide synchronisation */
xSemaphoreHandle canStartMutex;

/* Private functions */
static void systemTask(void *arg);

/* Public functions */
void systemLaunch(void)
{
  xTaskCreate(systemTask, SYSTEM_TASK_NAME,
              SYSTEM_TASK_STACKSIZE, NULL,
              SYSTEM_TASK_PRI, NULL);

}

// This must be the first module to be initialized!
void systemInit(void)
{
  if(isInit)
    return;

  canStartMutex = xSemaphoreCreateMutex();
  xSemaphoreTake(canStartMutex, portMAX_DELAY);

  socketlinkInit();
  sysLoadInit();

  /* Initialized hear and early so that DEBUG_PRINT (buffered) can be used early */
  crtpInit();
  consoleInit();

  DEBUG_PRINT("----------------------------\n");
  DEBUG_PRINT(P_NAME " is up and running!\n");
  DEBUG_PRINT("Build %s:%s (%s) %s\n", V_SLOCAL_REVISION,
              V_SREVISION, V_STAG, (V_MODIFIED)?"MODIFIED":"CLEAN");

  configblockInit();
  workerInit();
  buzzerInit();

  isInit = true;
}

bool systemTest()
{
  bool pass=isInit;
  pass &= workerTest();
  pass &= buzzerTest();
  return pass;
}

/* Private functions implementation */

void systemTask(void *arg)
{
  bool pass = true;

#ifdef DEBUG_QUEUE_MONITOR
  queueMonitorInit();
#endif

  //Init the high-levels modules
  systemInit();
  commInit();
  commanderInit();
  DEBUG_PRINT("Commander init finished \n");

  StateEstimatorType estimator = ESTIMATOR_NAME;
  stabilizerInit(estimator);
  DEBUG_PRINT("stabilizer init finished \n");

  memInit();
  DEBUG_PRINT("Memory init finished \n");

  //Test the modules
  pass &= systemTest();
  pass &= configblockTest();
  pass &= commTest();
  pass &= commanderTest();
  pass &= stabilizerTest();
  pass &= memTest();

  //Start the firmware
  if(pass)
  {
    selftestPassed = 1;
    systemStart();
    DEBUG_PRINT("ALL TESTS PASSED ! System started... \n");
  }
  else
  {
    selftestPassed = 0;
    if (systemTest())
    {
      while(1)
      {
        DEBUG_PRINT("Trying to start system ...\n");
        vTaskDelay(M2T(2000));
        // System can be forced to start by setting the param to 1 from the cfclient
        if (selftestPassed)
        {
	        DEBUG_PRINT("Start forced.\n");
          systemStart();
          break;
        }
      }
    }
    else
    {
      DEBUG_PRINT("System Test failed ...\n");
    }
  }

  workerLoop();

  //Should never reach this point!
  while(1)
    vTaskDelay(portMAX_DELAY);
}


/* Global system variables */
void systemStart()
{
  xSemaphoreGive(canStartMutex);
}

void systemWaitStart(void)
{
  //This permits to guarantee that the system task is initialized before other
  //tasks waits for the start event.
  while(!isInit)
    vTaskDelay(2);

  xSemaphoreTake(canStartMutex, portMAX_DELAY);
  xSemaphoreGive(canStartMutex);
}

void systemSetCanFly(bool val)
{
  canFly = val;
}

bool systemCanFly(void)
{
  return canFly;
}

/* No need  since no physical FLASH memory */
/*System parameters (mostly for test, should be removed from here) */
/*PARAM_GROUP_START(cpu)
PARAM_ADD(PARAM_UINT16 | PARAM_RONLY, flash, MCU_FLASH_SIZE_ADDRESS)
PARAM_ADD(PARAM_UINT32 | PARAM_RONLY, id0, MCU_ID_ADDRESS+0)
PARAM_ADD(PARAM_UINT32 | PARAM_RONLY, id1, MCU_ID_ADDRESS+4)
PARAM_ADD(PARAM_UINT32 | PARAM_RONLY, id2, MCU_ID_ADDRESS+8)
PARAM_GROUP_STOP(cpu)*/

PARAM_GROUP_START(system)
PARAM_ADD(PARAM_INT8, selftestPassed, &selftestPassed)
PARAM_GROUP_STOP(sytem)

/* Loggable variables */
LOG_GROUP_START(sys)
LOG_ADD(LOG_INT8, canfly, &canFly)
LOG_GROUP_STOP(sys)

/* TODO write a simulated version for battery */
LOG_GROUP_START(pm)
LOG_ADD(LOG_FLOAT, vbat, &batteryVoltage)
LOG_GROUP_STOP(pm)
