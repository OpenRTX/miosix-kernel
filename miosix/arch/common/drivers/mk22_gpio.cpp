/***************************************************************************
 *   Copyright (C) 2012 by Terraneo Federico                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "mk22_gpio.h"

namespace miosix {

void GpioBase::modeImpl(unsigned int g, unsigned int p, unsigned char n, Mode::Mode_ m)
{
    GPIO_Type* gpio=reinterpret_cast<GPIO_Type*>(g);
    PORT_Type* port=reinterpret_cast<PORT_Type*>(p);

    switch(m)
    {
        case Mode::INPUT:
            port->PCR[n] = PORT_PCR_MUX(1);   /* Enable pin in GPIO mode */
            gpio->PDDR   &= ~(1 << n);        /* Input mode              */
            break;

        case Mode::INPUT_PULL_UP:
            port->PCR[n] = PORT_PCR_MUX(1)    /* Enable pin in GPIO mode */
                         | PORT_PCR_PS(1)     /* Pull up mode            */
                         | PORT_PCR_PE(1);    /* Pull up/down enable     */
            gpio->PDDR    &= ~(1 << n);       /* Input mode              */
            break;

        case Mode::INPUT_PULL_DOWN:
            port->PCR[n]   = PORT_PCR_MUX(1)    /* Enable pin in GPIO mode */
                           | PORT_PCR_PE(1);    /* Pull up/down enable     */
            gpio->PDDR    &= ~(1 << n);         /* Input mode              */

            break;

        case Mode::INPUT_ANALOG:
            port->PCR[n] = PORT_PCR_MUX(0);   /* Enable pin in AF0 mode  */
            gpio->PDDR   &= ~(1 << n);        /* Input mode              */
            break;

        case Mode::OUTPUT:
            port->PCR[n] = PORT_PCR_MUX(1);   /* Enable pin in GPIO mode  */
            gpio->PDDR    |= (1 << n);        /* Output mode              */
            break;

        case Mode::OPEN_DRAIN:
            port->PCR[n] = PORT_PCR_MUX(1)    /* Enable pin in GPIO mode  */
                         | PORT_PCR_ODE(1);   /* Enable open drain mode   */
            gpio->PDDR   |= (1 << n);         /* Output mode              */
            break;

        case Mode::OPEN_DRAIN_PULL_UP:
            port->PCR[n] = PORT_PCR_MUX(1)    /* Enable pin in GPIO mode */
                         | PORT_PCR_ODE(1)    /* Enable open drain mode  */
                         | PORT_PCR_PS(1)     /* Pull up mode            */
                         | PORT_PCR_PE(1);    /* Pull up/down enable     */
            gpio->PDDR   |= (1 << n);         /* Output mode             */
            break;


        case Mode::ALTERNATE:
            port->PCR[n] = 0;                /* Reset all pull up/down, OD, ...*/
            break;

        case Mode::ALTERNATE_OD:
            port->PCR[n] = PORT_PCR_ODE(1);  /* Enable open drain mode */
            break;

        case Mode::ALTERNATE_OD_PULL_UP:
            port->PCR[n] = PORT_PCR_ODE(1)   /* Enable open drain mode   */
                         | PORT_PCR_PS(1)    /* Pull up mode             */
                         | PORT_PCR_PE(1);   /* Pull up/down enable      */
            break;

        default:
            port->PCR[n] = PORT_PCR_MUX(1);   /* Enable pin in GPIO mode */
            gpio->PDDR   &= ~(1 << n);        /* Input mode              */
            break;
    }
}

void GpioBase::afImpl(unsigned int g, unsigned int p, unsigned char n, unsigned char af)
{
    (void) g;
    if(af > 1)
    {
        reinterpret_cast<PORT_Type*>(p)->PCR[n] &= ~PORT_PCR_MUX_MASK;   /* Clear old configuration */
        reinterpret_cast<PORT_Type*>(p)->PCR[n] |= PORT_PCR_MUX(af);     /* Set new AF, range 0 - 7 */
    }
}

} //namespace miosix
