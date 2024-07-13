/***************************************************************************
 *   Copyright (C) 2009, 2010, 2011, 2012 by Terraneo Federico             *
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

#ifndef MK22_GPIO_H
#define MK22_GPIO_H

#include "interfaces/arch_registers.h"

namespace miosix {

/**
 * This class just encapsulates the Mode_ enum so that the enum names don't
 * clobber the global namespace.
 */
class Mode
{
public:
    /**
     * GPIO mode (INPUT, OUTPUT, ...)
     * \code pin::mode(Mode::INPUT);\endcode
     */
    enum Mode_
    {
        INPUT,                  ///Input Floating
        INPUT_PULL_UP,          ///Input PullUp
        INPUT_PULL_DOWN,        ///Input PullDown
        INPUT_ANALOG,           ///Input Analog
        OUTPUT,                 ///Push Pull Output
        OPEN_DRAIN,             ///Open Drain Output
        OPEN_DRAIN_PULL_UP,     ///Open Drain Output PU
        ALTERNATE,              ///Alternate function
        ALTERNATE_OD,           ///Alternate Open Drain
        ALTERNATE_OD_PULL_UP,   ///Alternate Open Drain PU
    };
private:
    Mode(); //Just a wrapper class, disallow creating instances
};

/**
 * This class just encapsulates the Speed_ enum so that the enum names don't
 * clobber the global namespace.
 */
class Speed
{
public:
    /**
     * GPIO speed
     * \code pin::speed(Speed::_50MHz);\endcode
     */
    enum Speed_
    {
        _2MHz     = 0x0,
        _50MHz    = 0x1, // Anything MEDIUM and above will be treated as 50MHz
    };
private:
    Speed(); //Just a wrapper class, disallow creating instances
};

/**
 * Base class to implement non template-dependent functions that, if inlined,
 * would significantly increase code size
 */
class GpioBase
{
protected:
    static void modeImpl(unsigned int g, unsigned int p, unsigned char n, Mode::Mode_ m);
    static void afImpl(unsigned int g, unsigned int p, unsigned char n, unsigned char af);
};

/**
 * This class allows to easily pass a Gpio as a parameter to a function.
 * Accessing a GPIO through this class is slower than with just the Gpio,
 * but is a convenient alternative in some cases. Also, an instance of this
 * class occupies a few bytes of memory, unlike the Gpio class.
 */
class GpioPin : private GpioBase
{
public:
    /**
     * Constructor
     * \param g GPIOA_BASE, GPIOB_BASE, ... as #define'd in MK22F51212.h
     * \param n which pin (0 to 15)
     */
    GpioPin(unsigned int g, unsigned char n)
        : g(reinterpret_cast<GPIO_Type*>(g)), n(n)
        {
            switch(g)
            {
                case GPIOA_BASE:
                    p = PORTA;
                    break;
                case GPIOB_BASE:
                    p = PORTB;
                    break;
                case GPIOC_BASE:
                    p = PORTC;
                    break;
                case GPIOD_BASE:
                    p = PORTD;
                    break;
                case GPIOE_BASE:
                    p = PORTE;
                    break;
                default:
                    p = 0;
                    break;
            }
        }

    /**
     * Set the GPIO to the desired mode (INPUT, OUTPUT, ...)
     * \param m enum Mode_
     */
    void mode(Mode::Mode_ m)
    {
        modeImpl(reinterpret_cast<unsigned int>(g),reinterpret_cast<unsigned int>(p),n,m);
    }

    /**
     * Set the GPIO speed
     * \param s speed value
     */
    void speed(Speed::Speed_ s)
    {
        if(s < Speed::_2MHz)
        {
            p->PCR[n] &= ~PORT_PCR_SRE(1);
        }
        else
        {
            p->PCR[n] |= PORT_PCR_SRE(1);
        }
    }

    /**
     * Select which of the many alternate functions is to be connected with the
     * GPIO pin.
     * \param af alternate function number, from 0 to 15
     */
    void alternateFunction(unsigned char af)
    {
        afImpl(reinterpret_cast<unsigned int>(g),reinterpret_cast<unsigned int>(p),n,af);
    }

    /**
     * Set the pin to 1, if it is an output
     */
    void high()
    {
        g->PSOR = (1<<n);
    }

    /**
     * Set the pin to 0, if it is an output
     */
    void low()
    {
        g->PCOR = (1<<n);
    }

    /**
     * Allows to read the pin status
     * \return 0 or 1
     */
    int value()
    {
        return (g->PDIR & (1<<n))? 1 : 0;
    }

    /**
     * \return the pin port. One of the constants PORTA_BASE, PORTB_BASE, ...
     */
    unsigned int getPort() const { return reinterpret_cast<unsigned int>(p); }

    /**
     * \return the pin gpio bank. One of the constants GPIOA_BASE, GPIOB_BASE, ...
     */
    unsigned int getGpio() const { return reinterpret_cast<unsigned int>(g); }

    /**
     * \return the pin number, from 0 to 15
     */
    unsigned char getNumber() const { return n; }

private:
    GPIO_Type *g; // Pointer to the GPIO (in/out and values)
    PORT_Type *p; // Pointer to the port (speed, mode, ...)
    unsigned char n; //Number of the GPIO within the port
};

/**
 * Gpio template class
 * \param P PORTA_BASE, PORTB_BASE, ... as #define'd in MK22FN51212.h
 * \param N which pin (0 to 31)
 * The intended use is to make a typedef to this class with a meaningful name.
 * \code
 * typedef Gpio<PORTA_BASE, 0> green_led;
 * green_led::mode(Mode::OUTPUT);
 * green_led::high();//Turn on LED
 * \endcode
 */
template<unsigned int G, unsigned int P, unsigned char N>
class Gpio : private GpioBase
{
public:

    /**
     * Set the GPIO to the desired mode (INPUT, OUTPUT, ...)
     * \param m enum Mode_
     */
    static void mode(Mode::Mode_ m)
    {
        modeImpl(G,P,N,m);
    }

    /**
     * Set the GPIO speed
     * \param s speed value
     */
    static void speed(Speed::Speed_ s)
    {
        if(s < Speed::_2MHz)
        {
            reinterpret_cast<PORT_Type*>(P)->PCR[N] &= ~PORT_PCR_SRE(1);
        }
        else
        {
            reinterpret_cast<PORT_Type*>(P)->PCR[N] |= PORT_PCR_SRE(1);
        }
    }

    /**
     * Select which of the many alternate functions is to be connected with the
     * GPIO pin.
     * \param af alternate function number, from 0 to 15
     */
    static void alternateFunction(unsigned char af)
    {
        afImpl(G,P,N,af);
    }

    /**
     * Set the pin to 1, if it is an output
     */
    static void high()
    {
        reinterpret_cast<GPIO_Type*>(G)->PSOR = 1<<N;
    }

    /**
     * Set the pin to 0, if it is an output
     */
    static void low()
    {
        reinterpret_cast<GPIO_Type*>(G)->PCOR = 1<<N;
    }

    /**
     * Allows to read the pin status
     * \return 0 or 1
     */
    static int value()
    {
        return (reinterpret_cast<GPIO_Type*>(G)->PDIR & (1<<N)? 1 : 0);
    }

    /**
     * \return this Gpio converted as a GpioPin class
     */
    static GpioPin getPin()
    {
        return GpioPin(G,N);
    }

    /**
     * \return the pin port. One of the constants PORTA_BASE, PORTB_BASE, ...
     */
    unsigned int getPort() const { return P; }

    /**
     * \return the pin GPIO bank. One of the constants GPIOA_BASE, GPIOB_BASE, ...
     */
    unsigned int getGpio() const { return G; }

    /**
     * \return the pin number, from 0 to 31
     */
    unsigned char getNumber() const { return N; }

private:
    Gpio() //Only static member functions, disallow creating instances
    {
        // G = getGpioFromPort();
    }

    unsigned int getGpioFromPort() const
    {
        unsigned int gpio;
        switch(P)
        {
            case PORTA_BASE:
                gpio = GPIOA_BASE;
                break;
            case PORTB_BASE:
                gpio = GPIOB_BASE;
                break;
            case PORTC_BASE:
                gpio = GPIOC_BASE;
                break;
            case PORTD_BASE:
                gpio = GPIOD_BASE;
                break;
            case PORTE_BASE:
                gpio = GPIOE_BASE;
                break;
            default:
                gpio = 0;
                break;
        }

        return gpio;
    }

    // unsigned int G; /* GPIO struct base address */
};

} //namespace miosix

#endif  //STM32_GPIO_H
