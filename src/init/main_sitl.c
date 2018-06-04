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
 * main_sitl.c - Containing the main function.for the SITL simulation based on main.c
 */

/* Personal configs */
#include "FreeRTOSConfig.h"

/* FreeRtos includes */
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Project includes */
#include "socketlink.h"
#include "config.h"
#include "system.h"
#include "usec_time.h"

#define CRTP_PORT 19950
#define CRTP_SERVER_ADDRESS "INADDR_ANY"

int main(int argc, char **argv) 
{

  if (argc == 3){
    crtp_port = atoi(argv[1]);
    address_host = argv[2];
    printf("CF address : %s , port : %d \n", address_host , crtp_port );
  } else {
    // Initiaze socket parameters
    crtp_port =  CRTP_PORT;
    address_host = CRTP_SERVER_ADDRESS;
    printf("No port and ADDRESS selected ! INADDR_ANY-19950 selected as default\n");
  }
  
  //Launch the system task that will initialize and start everything
  systemLaunch();

  //Start the FreeRTOS scheduler
  vTaskStartScheduler();

  //Should never reach this point!
  while(1);

  return 0;
}

/*
* FreeRTOS assert name
*/
void vAssertCalled( unsigned long ulLine, const char * const pcFileName )
{
    printf("ASSERT: %s : %d\n", pcFileName, (int)ulLine);
    while(1);
}

unsigned long ulGetRunTimeCounterValue(void)
{
    return 0;
}

void vConfigureTimerForRunTimeStats(void)
{
    return;
}

/* For memory management */
void vApplicationMallocFailedHook(void)
{
    while(1);
}