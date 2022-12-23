//345678901234567890123456789012345678901234567890123456789012345678901234567890
// Copyright 2013, Paul Young.  All Rights Reserved
//
// MOAS II emulator
//
// This is a not quite faithful emulator of the MOAS II switch.  The actual
// switch is interrupt driven, has limited memory, and has no barrel shifter.
// So some algorithms are different.
//
// In particular, the input and output buffering are quite different.  The
// actual buffering in the switch has to handle receiving characters while
// processing a command and has to queue messages and characters for the
// output because it is a 9600 baud serial line.  Even if the emulation was
// more faithful the switch CPU speed is 16 MHz and it could choke on things
// that the emulator can easily handle with a 32 or 64 bit processor.
//
// Also the emulator has no timers.  All timed events happen instantly.  The
// timers protect the radio hardware from hot switching but that is not an
// issue for the emulator.
//
// This emulator was NOT intended to drive actual hardware and should not
// be used that way without major alteration.

#ifndef MOAS_H
#define MOAS_H

#define MOAS_STATIONS  6
#define MOAS_ANTENNAS  64
#define MOAS_RELAYS    64

// These are the routines which must be called to use the emulator:

// Initialize the switch.  This is always successful.
// Routine:  moas_initialize
//
// Inputs:
// Outputs:
void moas_initialize();

// Give the switch a character
// Routine: moas_character
//
// Inputs:
//    c       Character to give to switch
void moas_character(char c);

// Change the transmit/receive state
// Routine: moas_txrx
//
// Inputs:
//    station Station which is transmitting or receiving
//    state   TRUE if transmitting, FALSE if receiving
void moas_txrx(int station, int state);

// These are the routines which must be defined to use the emulator:

// Write a status or event string
// Routine:  moas_callback_write
// Inputs:
//    buffer  Null terminated string to write
//            The output buffer is owned by the emulator.  It is
//            static and may be overwritten by emulator activity.
void moas_callback_write(const char *buffer);

// Relay and inhibit update
// Routine:  moas_callback_update
// Inputs:
//    relays  Array of size MOAS_RELAYS. TRUE if the relay is selected.
//    inhibits Array of size MOAS_STATIONS.  TRUE if the station is
//            inhibited and cannot transmit.
void moas_callback_update(const int *relays, const int *inhibits);

// Antennas
// Routine:  moas_callback_antennas
// Inputs:
//    tx     Array of size MOAS_STATIONS.  Values represent the
//           current actual transmit antenna for each station
//    rx     Array of size MOAS_STATIONS.  Values represent the
//           current actual receive antenna for each station
void moas_callback_antennas(const int *tx, const int *rx);

#endif