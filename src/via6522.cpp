/*
Copyright (C) 2016 Beardypig

This file is part of Vectrexia.

Vectrexia is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Vectrexia is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Vectrexia.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <via6522.h>
#include <stddef.h>
#include "via6522.h"

uint8_t VIA6522::Read(uint8_t reg)
{
    uint8_t data = 0;

    switch (reg & 0xf) {
        case REG_ORB:
            data = read_portb();
            break;

        case REG_ORA:
            if ((registers.PCR & CA2_MASK) == CA2_OUTPUT) {
                // CA2 goes low to signal "data taken", in pulse mode that will be restored to 1 at the end of the
                // VIA emulation
                ca2_state = 0;
            }
        case REG_ORA_NO_HANDSHAKE:
            data = read_porta();
            break;

            // Timer 1
        case REG_T1CL:  // timer 1 low-order counter
            // stop timer 1
            //via_debug_timer("Timer 1 has been disabled\r\n");
            timer1.enabled = false;

            // Set PB7 if Timer 1 has control of PB7
            if (registers.ACR & T1_PB7_CONTROL)
                pb7 = 0x80;

            // clear the timer 1 interrupt
            set_ifr(TIMER1_INT, 0);

            data = (uint8_t)(timer1.counter & 0xff);
            break;
        case REG_T1CH:  // timer 1 high-order counter
            data = (uint8_t)(timer1.counter >> 8);
            break;
        case REG_T1LL:  // timer 1 low-order latch
            data = registers.T1LL;
            break;
        case REG_T1LH:  // timer 2 high-order latch
            data = registers.T1LH;
            break;

            // Timer 2
        case REG_T2CL:  // timer 2 low-order counter
            // stop timer 2
            //via_debug_timer("Timer 2 has been disabled\r\n");
            timer2.enabled = false;

            // clear the timer 2 interrupt
            set_ifr(TIMER2_INT, 0);

            data = (uint8_t)(timer2.counter & 0xff);
            break;
        case REG_T2CH:  // timer 2 high-order counter
            data = (uint8_t)(timer2.counter >> 8);
            break;

            // Shift Register
        case REG_SR:
            // clear the SR interrupt
            set_ifr(SR_INT, 0);
            // reset shifted bits counter
            sr.shifted = 0;
            sr.enabled = true;
            data = registers.SR;
            break;

            // Interrupt Registers
        case REG_IER:
            // interrupt enable register, the MSB is always set when reading.
            data = registers.IER | IRQ_MASK;
            break;

            // Basic Reads
        case REG_DDRB:
            // DDR register 0 for output, 1 for input
            data = registers.DDRB;
            break;
        case REG_DDRA:
            data = registers.DDRA;
            break;
        case REG_ACR:
            data = registers.ACR;
            break;
        case REG_PCR:
            data = registers.PCR;
            break;
        case REG_IFR:
            data = registers.IFR;
            break;
        default:
            break;
            //via_debug("[VIA] ERROR: Unhanled register address read: $%02x\r\n", reg & 0xf);
    }
    // invalid address
    //via_debug("Reading Register: %s ($%02x) = 0x%02x\r\n", REG_STRING[reg & 0xf], reg & 0x0f, data);
    return data;
}

void VIA6522::Write(uint8_t reg, uint8_t data)
{
    switch (reg & 0xf) {

        case REG_ORB:
            // Port B lines (CB1, CB2) handshake on a write operation only.
            if ((registers.PCR & CB2_MASK) == CB2_OUTPUT) {
                cb2_state = 0;
            }

            registers.ORB = data;
            break;
        case REG_ORA:
            // If CA2 is output
            if ((registers.PCR & CA2_MASK) == CA2_OUTPUT) {
                ca2_state = 1;
            }
        case REG_ORA_NO_HANDSHAKE:
            registers.ORA = data;
            break;

            // Timer 1
        case REG_T1CL:  // timer 1 low-order counter
            registers.T1LL = data;
            break;
        case REG_T1CH:  // timer 1 high-order counter
            registers.T1LH = data;
            timer1.counter = (registers.T1LH << 8) | registers.T1LL;

            //via_debug_timer("Timer 1 has been enabled: %d in mode: %x\r\n", timer1.counter, (registers.ACR & T1_MASK));

            // start timer 1
            timer1.enabled = true;
            timer1.one_shot = false;
            // Does Timer 1 control PB7
            if ((registers.ACR & T1_MASK) == T1_PB7_CONTROL)
                // set timer 1's pb7 state to low
                pb7 = 0;

            // clear the timer 1 interrupt
            set_ifr(TIMER1_INT, 0);
            break;
        case REG_T1LL:  // timer 1 low-order latch
            registers.T1LL = data;
            break;
        case REG_T1LH:  // timer 2 high-order latch
            registers.T1LH = data;
            break;

            // Timer 2
        case REG_T2CL:  // timer 2 low-order counter
            registers.T2CL = data;
            break;
        case REG_T2CH:  // timer 2 high-order counter
            registers.T2CH = data;
            timer2.counter = (registers.T2CH << 8) | registers.T1CL;

            // start timer 2
            //via_debug_timer("Timer 2 has been enabled: %d in mode %x\r\n", timer2.counter, (registers.ACR & T2_MASK));
            timer2.enabled = true;
            timer2.one_shot = false;

            // clear the timer 2 interrupt
            set_ifr(TIMER2_INT, 0);
            break;

            // Shift Register
        case REG_SR:
            // clear the SR interrupt
            set_ifr(SR_INT, 0);
            // reset shifted bits counter
            sr.shifted = 0;
            registers.SR = data;
            sr.enabled = true;
            break;

            // Interrupt Registers
        case REG_IFR:
            // interrupt flag register
            set_ifr(data, 0);
            break;

        case REG_IER:
            // interrupt enable register
            // enable or disable interrupts based on the MSB and using the rest of the data as a mask
            set_ier(data & ~IRQ_MASK, (uint8_t) (data & 0x80 ? 1 : 0));
            break;

        case REG_PCR:
            registers.PCR = data;
            // if CA/B2 is in OUT LOW mode, set to low, otherwise high
            ca2_state = (uint8_t) (((registers.PCR & CA2_MASK) == CA2_OUT_LOW) ? 0 : 1);
            cb2_state = (uint8_t) (((registers.PCR & CB2_MASK) == CB2_OUT_LOW) ? 0 : 1);
            break;

            // Basic Writes
        case REG_DDRB:
            registers.DDRB = data;
            break;
        case REG_DDRA:
            registers.DDRA = data;
            break;
        case REG_ACR:
            registers.ACR = data;
            break;
        default:
            //via_debug("[VIA] ERROR: Unhanled register address write: $%02x\r\n", address & 0xf);
            break;
    }
}

void VIA6522::Reset()
{
    // registers
    registers.ORB = 0;
    registers.ORA = 0;
    registers.DDRB = 0;
    registers.DDRA = 0;
    registers.T1CL = 0;
    registers.T1CH = 0;
    registers.T1LL = 0;
    registers.T1LH = 0;
    registers.T2CL = 0;
    registers.T2CH = 0;
    registers.SR = 0;
    registers.ACR = 0;
    registers.PCR = 0;
    registers.IFR = 0;
    registers.IER = 0;
    registers.IRA = 0;
    registers.IRB = 0;
    registers.IRA_latch = 0;
    registers.IRB_latch = 0;

    ca1_state = 0;
    ca2_state = 1;
    cb1_state = 0;
    cb1_state_sr = 0;
    cb2_state = 1;
    cb2_state_sr = 0;

    // timer data
    timer1.counter = 0;
    timer1.enabled = false;
    timer1.one_shot = false;
    timer2.counter = 0;
    timer2.enabled = false;
    timer2.one_shot = false;
    pb7 = 0x80;

    // shift register
    sr.enabled = false;
    sr.shifted = 0;
    sr.counter = 0;

    clk = 0;
}

void VIA6522::Execute()
{
    uint8_t portb;

    // Timers
    if (timer1.enabled) {
        // decrement the counter and test if it has rolled over
        if (--timer1.counter == 0xffff) {
            // is the continuous interrupt bit set
            if (registers.ACR & T1_CONTINUOUS) {
                // set the timer 1 interrupt
                set_ifr(TIMER1_INT, 1);

                //via_debug_timer("Timer 1 has triggered\r\n");
                // toggle PB7 (ACR & 0x80)
                if (registers.ACR & T1_PB7_CONTROL) {
                    //via_debug_timer2(", PB7=0x%02x", via6522.pb7);
                    pb7 ^= 0x80;
                }
                //via_debug_timer("\r\n");

                // reload counter from latches
                timer1.counter = (registers.T1LH << 8) | registers.T1LL;
            } else { // one-shot mode
                if (!timer1.one_shot) {
                    // set timer 1 interrupt
                    set_ifr(TIMER1_INT, 1);

                    //via_debug_timer("Timer 1 has trigger a one-shot interrupt");
                    if (registers.ACR & T1_PB7_CONTROL) {
                        //via_debug_timer2(", PB7 set");
                        // restore PB7 to 1, it was set to 0 by writing to T1C-H
                        pb7 = 0x80;
                    }
                    //via_debug_timer2("\r\n");
                    timer1.one_shot = true;
                }
            }
        }
    }

    if (timer2.enabled) {
        // pulsed mode is not used
        if ((registers.ACR & T2_MASK) == T2_TIMED) { // timed, one-shot mode
            // In one-shot mode the timer keeps going, but the interrupt is only triggered once
            if (--timer2.counter == 0xffff && !timer2.one_shot) {
                //via_debug_timer("Timer 2 has rolled over.\r\n");
                // set the Timer 2 interrupt
                set_ifr(TIMER2_INT, 1);
                timer2.one_shot = true;
            }
        }
    }

    //via_debug("SR enabled: %d shift: %d: cb2: %d\r\n", sr.enabled, via6522.sr.shifted, via6522.cb2_state_sr);

    switch (registers.ACR & SR_MASK) {
        case SR_DISABLED:
        case SR_OUT_EXT:
        case SR_IN_EXT:
            // This mode is controlled by CB1 positive edge, however in the Vectrex CB1 is not connected.
            // For these modes CB1 is an input
            break;
        case SR_IN_T2:
        case SR_OUT_T2:
        case SR_OUT_T2_FREE:
            // CB1 becomes an output
            // when counter T2 counter rolls
            if (sr.counter == 0x00) {
                // Toggle CB1 on the clock time out
                sr.update(*this, (uint8_t) (cb1_state_sr ^ 1));
            }
            break;
        case SR_IN_O2:
        case SR_OUT_O2:
            // CB1 is an output
            // Toggle CB1 on the phase 2 clock
            sr.update(*this, (uint8_t) (cb1_state_sr ^ 1));
        default:break;
    }

    if (--sr.counter == 0xff) {
        // reset the Shift Reigster T2 counter to T2 low-order byte
        sr.counter = registers.T2CL;
    }

    //via_debug("CA2: %d\r\n", registers.ca2_state);

    if (update_callback_func)
    {
        update_callback_func(update_callback_ref,
                             registers.ORA, read_portb(),
                             ca1_state, ca2_state,
                             // CB1 outputs from the SR except when SR is driving by an external clock (CB1)
                             (registers.ACR & SR_EXT) == SR_EXT ? cb1_state : cb1_state_sr,
                             // When SR is outputting use the CB2 value as per SR
                             (registers.ACR & SR_IN_OUT) ? cb2_state_sr : cb2_state);
    }

    // End of pulse mode handshake
    // If PORTA is using pulse mode handshaking, restore CA2 to 1
    if ((registers.PCR & CA2_MASK) == CA2_OUT_PULSE)
        ca2_state = 1;

    // Same for PORTB
    if ((registers.PCR & CB2_MASK) == CB2_OUT_PULSE)
        cb2_state = 1;

    clk++;
}

void VIA6522::SetPortAReadCallback(VIA6522::port_callback_t func, intptr_t ref)
{
    porta_callback_func = func;
    porta_callback_ref = ref;
}

void VIA6522::SetPortBReadCallback(VIA6522::port_callback_t func, intptr_t ref)
{
    portb_callback_func = func;
    portb_callback_ref = ref;
}

void VIA6522::SetUpdateCallback(update_callback_t func, intptr_t ref)
{
    update_callback_func = func;
    update_callback_ref = ref;
}

uint8_t VIA6522::GetIRQ()
{
    return registers.IFR & IRQ_MASK;
}

VIA6522::Timer &VIA6522::GetTimer1()
{
    return timer1;
}

VIA6522::Timer &VIA6522::GetTimer2()
{
    return timer2;
}

VIA6522::ShiftRegister &VIA6522::GetShiftregister()
{
    return sr;
}

VIA6522::Registers VIA6522::GetRegisterState()
{

    Registers register_state;

    register_state.ORB = read_portb();
    register_state.ORA = read_porta();
    register_state.T1CL = (uint8_t)(timer1.counter & 0xff);
    register_state.T1CH = (uint8_t)(timer1.counter >> 8);
    register_state.T1LL = registers.T1LL;
    register_state.T1LH = registers.T1LH;
    register_state.T2CL = (uint8_t)(timer2.counter & 0xff);
    register_state.T2CH = (uint8_t)(timer2.counter >> 8);
    register_state.SR = registers.SR;
    register_state.IER = registers.IER | IRQ_MASK;
    register_state.DDRB = registers.DDRB;
    register_state.DDRA = registers.DDRA;
    register_state.ACR = registers.ACR;
    register_state.PCR = registers.PCR;
    register_state.IFR = registers.IFR;

    return register_state;
}
