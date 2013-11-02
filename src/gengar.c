/*
 *      Author: Brian Carrigan
 *      Date:   11/2/2013
 *      Email:  brian.c.carrigan@gmail.com
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *          
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pic12f615.h>
#include <stdint.h>
#include <xc.h>

#define SEED1       0x21    // Byte 1 of the random number seed.
#define SEED2       0x2C    // Byte 2 of the random number seed.
#define SEED3       0x7F    // Byte 3 of the random number seed.
#define SEED4       0x7F    // Byte 4 of the random number seed.
#define THRESHOLD   250     // If the random is greater than this, blink.

//  4MHz Internal oscillator
#pragma config FOSC = 4

//  Turn off the watchdog
#pragma config WDTE = 0

/**
 * @brief The LFSR generator data structure. Holds it's current state and index.
 *
 * Index must be set to 0 before use.
 * Based on http://www.electricdruid.net/index.php?page=techniques.practicalLFSRs
 */
typedef struct
{
    uint8_t state[4];
    uint8_t index;
} lfsr_generator_t;

// Function prototypes
void inline     LFSR_Init(lfsr_generator_t *pGenerator);
uint8_t inline  LFSR_Generate(lfsr_generator_t *pGenerator);
void inline     Timer_Init(void);
void inline     Timer_Wait(uint8_t pMilliseconds);

int main()
{
    // Declare the random number generator
    lfsr_generator_t    random;

    // Dim level - out of 10
    uint8_t dim_level = 10;

    // Set up the random number generator
    LFSR_Init(&random);

    // Initialize the timer
    Timer_Init();

    // Set GP2 and GP4 as outputs and pull them high
    ANSEL = 0;
    TRISIO &= ~0x14;
    GPIO |= 0x14;

    // Main loop
    while(1)
    { 
        // If random > threshold -- blink
        // Average blink time = .1S * 255 / (255 - Threshold)
        // 250 is an average of 5 seconds.
        if(LFSR_Generate(&random) > THRESHOLD)
        {
            uint8_t goingUp = 0;
            uint8_t counter = 0;
            while(1)
            {
                // The counter will count up to 10 everytime. It will pull the
                // eyes high at 0 and down at dim_level. This creates a sort
                // of PWM based on the delay below for fading blinks.
                if(counter == dim_level)
                    GPIO &= ~0x14;
                else if(counter == 0)
                    GPIO |= 0x14;
                counter++;

                // In every blink, dim_level will decrease on every PWM cycle
                // until it is zero. Once at zero, the counter is set to 11 so
                // that it must increment until it overflows and resets, causing
                // the time that the eyes are off to be longer than any other
                // level. Once at zero, it begins incrementing back up to 10.
                if(counter == 10)
                {
                    counter = 0;
                    if(goingUp != 0)
                    {
                        dim_level++;
                    } else {
                        dim_level--;
                        if(dim_level == 0)
                        {
                            counter = 11;
                            goingUp = 1;
                        }
                    }
                }

                // If we have gone down and back up, break the loop.
                if((goingUp == 1) && (dim_level == 10))
                {
                    GPIO |= 0x14;
                    break;
                }

                // Wait 2 milliseconds
                Timer_Wait(2);
            }
        }

        // Wait 100mS before rolling again.
        Timer_Wait(100);
    }

    // Not gonna hit this.
    return (0);
}

/**
 * @brief Initializes a linear feedback shift register with the seeds defined
 *        above.
 * @param pGenerator Pointer to the generator data structure.
 */
void inline LFSR_Init(lfsr_generator_t *pGenerator)
{
    pGenerator->state[0] = SEED1;
    pGenerator->state[1] = SEED2;
    pGenerator->state[2] = SEED3;
    pGenerator->state[3] = SEED4;
    pGenerator->index = 0;
}

/**
 * @brief Generates a psuedo-random 8 bit variable.
 * @param pGenerator Pointer to the generator data structure.
 * @return Returns a psuedo-random 8 bit variable.
 */
uint8_t inline LFSR_Generate(lfsr_generator_t *pGenerator)
{
    // Generate the random number:
    uint8_t newIndex = (pGenerator->index + 1) & 0x03;

    // Calculate the shifts
    uint8_t shiftA =    pGenerator->state[pGenerator->index];
    uint8_t shiftB =    (pGenerator->state[pGenerator->index] << 2) +
                        (pGenerator->state[newIndex] >> 6);
    uint8_t shiftC =    (pGenerator->state[pGenerator->index] << 6) +
                        (pGenerator->state[newIndex] >> 2);
    uint8_t shiftD =    (pGenerator->state[pGenerator->index] << 7) +
                        (pGenerator->state[newIndex] >> 1);

    // Compute the new byte
    pGenerator->state[pGenerator->index] = shiftA ^ shiftB ^ shiftC ^ shiftD;

    // Advance and return
    pGenerator->index = newIndex;

    return shiftA;
}

/**
 * @brief Initializes Timer 1 to divide by 8 mode (125kHz).
 */
void inline Timer_Init(void)
{
	T1CON = 0x30;
}

/**
 *  @brief Delays the processor pMilliseconds. Blocking call.
 *
 *  Using the 1MHZ Clock and a prescale of 8, each tick is 1/125000 of a second.
 *  The proper way to implement a millisecond would be to multiply 125 by pMilliseconds,
 *  but in order to save processing and 16 bit math, lets assume that 128 is close enough.
 *  There will be roughly a 2% error, but this is acceptable for a Gengar blinking
 *  his eyes.
 */
void inline Timer_Wait(uint8_t pMilliseconds)
{
	// Calculate and set the registers
	TMR1H = 0xFF - (pMilliseconds / 2);
	TMR1L = 0xFF - ((pMilliseconds % 2) << 7);

	// Turn on the timer
	T1CON |= 0x01;

	// Wait it out
	while((PIR1 & 0x01) == 0);

	// Clear the PIR bit.
	PIR1 &= 0xFE;

        // Turn the timer back off.
        T1CON &= ~0x01;
}
