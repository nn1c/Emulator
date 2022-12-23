// Copyright 2013, 2014 Paul Young.  All Rights Reserved
//
// MOAS II emulator

#include "moas.h"

#undef FALSE
#undef TRUE
#undef NULL

#define FALSE   0
#define TRUE    1
#define NULL    0

// MOAS II major and minor version numbers
#define VER_MAJOR 1
#define VER_MINOR 1

static const char sixbit[] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
	'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
	'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z', '{', '}' };

static void do_pins();
static void do_resolver();

// These are mainly taken from the actual switch.  There is no
// concept of a local variable in the switch...

// Some things which were bytes or even bits are made into ints.
// The switch also has individual names for elements of some arrays
// because several loops are unrolled for speed.

// Memory in the switch is also arranged to allow code optimizations
// and that isn't done here.

#define COMMAND_BUFFER_LEN 128

// These are the global relays which are always set
static int global_relays [MOAS_RELAYS];

// These are the actual station antennas and which are used
// when setting up the physical relays
static int actual_tx_antennas[MOAS_STATIONS];
static int actual_rx_antennas[MOAS_STATIONS];

static int actual_tx_relays [MOAS_RELAYS][MOAS_STATIONS];
static int actual_rx_relays [MOAS_RELAYS][MOAS_STATIONS];

// These are the current antennas and relays.  The conflict
// resolver has accepted them.
static int current_tx_antennas[MOAS_STATIONS];
static int current_rx_antennas[MOAS_STATIONS];

static int current_tx_relays [MOAS_RELAYS][MOAS_STATIONS];
static int current_rx_relays [MOAS_RELAYS][MOAS_STATIONS];

// These are the pending antennas and relays.  They were
// set by serial port commands.
static int pending_tx_antennas[MOAS_STATIONS];
static int pending_rx_antennas[MOAS_STATIONS];

static int pending_tx_relays [MOAS_RELAYS][MOAS_STATIONS];
static int pending_rx_relays [MOAS_RELAYS][MOAS_STATIONS];

// These are the alternate antennas.  There is no current
// or pending because the emulator is synchronous.  This
// is quite different than the actual code.
static int alternate_antennas[MOAS_STATIONS];
static int alternate_relays[MOAS_RELAYS][MOAS_STATIONS];
static int actual_alternate_relays[MOAS_RELAYS][MOAS_STATIONS];

// These are the actual relays and inhibits
static int actual_relays[MOAS_RELAYS];
static int actual_inhibits[MOAS_STATIONS];

// These are the antenna pending flags.  In the actual
// switch they are bits in a register.
static int tx_pending;
static int rx_pending;
static int extra_pending;
static int alt_pending;

static int trbits;
static int tr_last;

// Wait/inhibit mode (1=wait, 0=inhibit)
static int wait_mode;
static int command_wait_mode;
static int same_antenna_wait_mode;

static int conflict_sent_rx[MOAS_STATIONS];
static int conflict_sent_tx[MOAS_STATIONS];

static int inhibit_polarity[MOAS_STATIONS];

static int inhibit_type[MOAS_STATIONS];

// These are the cross-station inhibits (where
// one station transmitting inhibits others)
static int cross_inhibits[MOAS_STATIONS];

// These are the last inhibits sent to the
// emulator program
static int old_inhibits;

// These are the cross-station alternates (where
// one station transmitting forces another to use
// the alternate antenna
static int alternates[MOAS_STATIONS];
 
// The actual switch has a circular buffer but
// it isn't needed here.
static char command_buffer[COMMAND_BUFFER_LEN];
static int command_buffer_in;

// Stations inhibited by commands
static int command_inhibits;

// Unit identifier
static int unit_id;

static int antenna_system_table[MOAS_ANTENNAS];

// These are the pending antenna sytems for each station.
// If an antenna change is pending these represent the
// pending antenna, otherwise they are zero.
static int pending_tx_systems[MOAS_STATIONS];
static int pending_rx_systems[MOAS_STATIONS];

// These are the current antenna sytems for each
// station.  Only the transmit entries are used.
static int current_tx_systems[MOAS_STATIONS];

// This is the conflicts table.  In the actual
// switch it is a triangle.
static int conflicts_table[MOAS_ANTENNAS][MOAS_ANTENNAS];

// This is the fast table.  In the actual
// switch it is a triangle.
static int fast_table[MOAS_ANTENNAS][MOAS_ANTENNAS];

// These are the current and pending extra relays to be
// set on transmit.
static int current_extra_relays[MOAS_RELAYS][MOAS_STATIONS];
static int pending_extra_relays[MOAS_RELAYS][MOAS_STATIONS];

// These are the relays to be set when a station transmits
static int set_relays[MOAS_RELAYS][MOAS_STATIONS];

// These are the relays to be reset when a station transmits
static int reset_relays[MOAS_RELAYS][MOAS_STATIONS];

// These are the resulting relays from the set/reset when a station transmits
static int sr_relays[MOAS_RELAYS];

// TRUE if switch is in operate state
static int operate;

// TRUE if the conflict resolver is on
static int resolver_on;

// The events which should be sent to the controlling program
static int antenna_events;
static int tr_events;
static int inhibit_events;
static int extra_relay_events;

static int
sixtodigit(int d)
//----------------------------------------------------------------------
// Convert a sixbit character to an integer value
//----------------------------------------------------------------------
{
	if (d <= '9') {
		return (d - '0');
	}
	if (d <= 'Z') {
		return (d - 'A' + 10);
	}
	if (d <= 'z') {
		return (d - 'a' + 36);
	}
	if (d == '{') {
		return 62;
	}
	return 63;
}

void moas_initialize()
//----------------------------------------------------------------------
// Set up the initial state for the server
//----------------------------------------------------------------------
{
	int i;
	int j;

	for (i=0; i<MOAS_RELAYS; i++) {
		global_relays [i] = FALSE;
		actual_relays[i] = FALSE;
		sr_relays[i] = FALSE;
	}

	old_inhibits = 0x3f;

	for (i=0; i<MOAS_STATIONS; i++) {
		actual_tx_antennas[i] = MOAS_ANTENNAS-1;
		actual_rx_antennas[i] = MOAS_ANTENNAS-1;
		current_tx_antennas[i] = MOAS_ANTENNAS-1;
		current_rx_antennas[i] = MOAS_ANTENNAS-1;
		pending_tx_antennas[i] = MOAS_ANTENNAS-1;
		pending_rx_antennas[i] = MOAS_ANTENNAS-1;
		alternate_antennas[i] = MOAS_ANTENNAS-1;

		conflict_sent_rx[i] = FALSE;
		conflict_sent_tx[i] = FALSE;
		inhibit_polarity[i] = FALSE;
		inhibit_type[i] = FALSE;
		actual_inhibits[i] = FALSE;

		pending_tx_systems[i] = 0;
		pending_rx_systems[i] = 0;
		current_tx_systems[i] = 0;

		alternates[i] = 0;

		cross_inhibits[i] = 0;

		for (j=0; j<MOAS_RELAYS; j++) {
			actual_tx_relays [j][i] = FALSE;
			actual_rx_relays [j][i] = FALSE;
			current_tx_relays [j][i] = FALSE;
			current_rx_relays [j][i] = FALSE;
			pending_tx_relays [j][i] = FALSE;
			pending_rx_relays [j][i] = FALSE;
			current_extra_relays[j][i] = FALSE;
			pending_extra_relays[j][i] = FALSE;
			alternate_relays[j][i] = FALSE;
			actual_alternate_relays[j][i] = FALSE;
			set_relays[j][i] = FALSE;
			reset_relays[j][i] = FALSE;
		}
	}

	for (i=0; i<MOAS_ANTENNAS; i++) {
		antenna_system_table[i] = FALSE;

		for (j=0; j<MOAS_ANTENNAS; j++) {
			conflicts_table[j][i] = FALSE;
			fast_table[j][i] = FALSE;
		}
	}

	unit_id = 0;
	command_buffer_in = 0;

	trbits = 0;
	tr_last = 0;

	tx_pending = 0;
	rx_pending = 0;
	extra_pending = 0;
	alt_pending = 0;

	wait_mode = 0x3f;
	command_wait_mode = 0;
	same_antenna_wait_mode = 0;

	command_inhibits = 0;

	operate = FALSE;
	resolver_on = TRUE;

	antenna_events = FALSE;
	tr_events = FALSE;
	inhibit_events = FALSE;
	extra_relay_events = FALSE;

	do_pins();
}

static void
command_antenna()
//----------------------------------------------------------------------
// Process an antenna command
//----------------------------------------------------------------------
{
	int station;
	int antenna;
	int ry[MOAS_RELAYS];
	int i;

	for (i=0; i<MOAS_RELAYS; i++) {
		ry[i] = FALSE;
	}

	if ((command_buffer[1] == ';') ||
		(command_buffer[2] == ';') ||
		(command_buffer[3] == ';')) {
		moas_callback_write("?A;");
		return;
	}

	for (i=4; command_buffer[i]!=';'; i++) {
		ry[sixtodigit(command_buffer[i])] = TRUE;
	}

	// Station 0 is special - relays go to global relays
	if (command_buffer[1] == '0') {
		for (i=0; i<MOAS_ANTENNAS; i++) {
			global_relays[i] = ry[i];
		}
		do_pins();
		return;
	}

	station = command_buffer[1] - '1';
	if ((station < 0) || (station >= MOAS_STATIONS)) {
		moas_callback_write("?A;");
		return;
	}
	antenna = sixtodigit(command_buffer[3]);

	switch (command_buffer[2]) {
	case 'T':
		pending_tx_antennas[station] = antenna;

		tx_pending |= 1<<station;

		for (i=0; i<MOAS_RELAYS; i++) {
			pending_tx_relays[i][station] = ry[i];
		}
		break;

	case 'R':
		pending_rx_antennas[station] = antenna;

		rx_pending |= 1<<station;

		for (i=0; i<MOAS_RELAYS; i++) {
			pending_rx_relays[i][station] = ry[i];
		}
		break;

	case 'B':
		pending_tx_antennas[station] = antenna;
		pending_rx_antennas[station] = antenna;

		tx_pending |= 1<<station;
		rx_pending |= 1<<station;

		for (i=0; i<MOAS_RELAYS; i++) {
			pending_tx_relays[i][station] = ry[i];
			pending_rx_relays[i][station] = ry[i];
		}
		break;

	case 'A':
		alternate_antennas[station] = antenna;
		// Set RX pending so the conflict resolver will recompute
		// the receive antenna.
		alt_pending |= 1<<station;

		for (i=0; i<MOAS_RELAYS; i++) {
			alternate_relays[i][station] = ry[i];
		}
		break;

	case 'X':
		extra_pending |= 1<<station;

		for (i=0; i<MOAS_RELAYS; i++) {
			pending_extra_relays[i][station] = ry[i];
		}
		break;

	case 'S':
		for (i=0; i<MOAS_RELAYS; i++) {
			set_relays[i][station] = ry[i];
		}
		break;

	case 'C':
		for (i=0; i<MOAS_RELAYS; i++) {
			reset_relays[i][station] = ry[i];
		}
		break;

	default:
		moas_callback_write("?A;");
		break;
	}
	do_resolver();
}

static void
command_conflict_table()
//----------------------------------------------------------------------
// Process a conflict table command
//----------------------------------------------------------------------
{
	int i;
	int j;
	int ant1;
	int ant2;

	switch (command_buffer[1]) {
	case '0':
		for (i=0; i<MOAS_ANTENNAS; i++) {
			for (j=0; j<MOAS_ANTENNAS; j++) {
				conflicts_table[i][j] = FALSE;
			}
		}
		break;
	
	case '1':
		for (i=0; i<MOAS_ANTENNAS; i++) {
			for (j=0; j<MOAS_ANTENNAS; j++) {
				conflicts_table[i][j] = TRUE;
			}
		}
		break;

	case 'C':
		for (i=2; command_buffer[i] != ';'; i+=2) {
			ant1 = sixtodigit(command_buffer[i]);
			if (command_buffer[i+1] == ';') {
				moas_callback_write("?A;");
				break;
			}
			ant2 = sixtodigit(command_buffer[i+1]);
			conflicts_table[ant1][ant2] = TRUE;
			conflicts_table[ant2][ant1] = TRUE;
		}
		break;

	case 'c':
		for (i=2; command_buffer[i] != ';'; i+=2) {
			ant1 = sixtodigit(command_buffer[i]);
			if (command_buffer[i+1] == ';') {
				moas_callback_write("?A;");
				break;
			}
			ant2 = sixtodigit(command_buffer[i+1]);
			conflicts_table[ant1][ant2] = FALSE;
			conflicts_table[ant2][ant1] = FALSE;
		}
		break;

	default:
		moas_callback_write("?a;");
		break;
	}
}

static void
command_fast_table()
//----------------------------------------------------------------------
// Process a fast table command
//----------------------------------------------------------------------
{
	int i;
	int j;
	int ant1;
	int ant2;

	switch (command_buffer[1]) {
	case '0':
		for (i=0; i<MOAS_ANTENNAS; i++) {
			for (j=0; j<MOAS_ANTENNAS; j++) {
				fast_table[i][j] = FALSE;
			}
		}
		break;
	
	case '1':
		for (i=0; i<MOAS_ANTENNAS; i++) {
			for (j=0; j<MOAS_ANTENNAS; j++) {
				fast_table[i][j] = TRUE;
			}
		}
		break;

	case 'F':
		for (i=2; command_buffer[i] != ';'; i+=2) {
			ant1 = sixtodigit(command_buffer[i]);
			if (command_buffer[i+1] == ';') {
				moas_callback_write("?A;");
				break;
			}
			ant2 = sixtodigit(command_buffer[i+1]);
			fast_table[ant1][ant2] = TRUE;
			fast_table[ant2][ant1] = TRUE;
		}
		break;

	case 'f':
		for (i=2; command_buffer[i] != ';'; i+=2) {
			ant1 = sixtodigit(command_buffer[i]);
			if (command_buffer[i+1] == ';') {
				moas_callback_write("?A;");
				break;
			}
			ant2 = sixtodigit(command_buffer[i+1]);
			fast_table[ant1][ant2] = FALSE;
			fast_table[ant2][ant1] = FALSE;
		}
		break;

	default:
		moas_callback_write("?a;");
		break;
	}
}

static void
command_inhibit()
//----------------------------------------------------------------------
// Process an inhibit command
//----------------------------------------------------------------------
{
	int i;
	int station;

	for (i=1; command_buffer[i] != ';'; i++) {
		station = command_buffer[i] - '1';
		if ((station < 0) || (station >= MOAS_STATIONS)) {
			moas_callback_write("?A;");
			return;
		}
		command_inhibits |= 1<<station;
	}

	do_pins();
}

static void
command_inhibit_other_station()
//----------------------------------------------------------------------
// Process an inhibit other station command
//----------------------------------------------------------------------
{
	int i;
	int station;
	int other;

	if (command_buffer[1] == ';') {
		moas_callback_write("?A;");
		return;
	}
	station = command_buffer[1] - '1';
	if ((station < 0) || (station >= MOAS_STATIONS)) {
		moas_callback_write("?A;");
		return;
	}

	cross_inhibits[station] = 0;

	for (i=2; command_buffer[i] != ';'; i++) {
		other = command_buffer[i] - '1';
		if ((other < 0) || (other >= MOAS_STATIONS)) {
			moas_callback_write("?A;");
			return;
		}
		if (other == station) {
			moas_callback_write("?A;");
			return;
		}
		cross_inhibits[station] |= 1<<other;
	}
}

static void
command_inhibit_polarity()
//----------------------------------------------------------------------
// Process an inhibit polarity command
//----------------------------------------------------------------------
{
	int station;
	int i;

	switch (command_buffer[1]) {
	case '0':
		for (i=0; i<MOAS_STATIONS; i++) {
			inhibit_polarity[i] = FALSE;
		}
		break;

	case '1':
		for (i=0; i<MOAS_STATIONS; i++) {
			inhibit_polarity[i] = TRUE;
		}
		break;

	case 'E':
		for (i=2; command_buffer[i] != ';'; i++) {
			station = command_buffer[i] - '1';
			if ((station < 0) || (station >= MOAS_STATIONS)) {
				moas_callback_write("?A;");
				return;
			}
			inhibit_polarity[i] = TRUE;
		}
		break;

	case 'I':
		for (i=2; command_buffer[i] != ';'; i++) {
			station = command_buffer[i] - '1';
			if ((station < 0) || (station >= MOAS_STATIONS)) {
				moas_callback_write("?A;");
				return;
			}
			inhibit_polarity[i] = FALSE;
		}
		break;
	
	default:
		moas_callback_write("?A;");
		break;
	}
}

static void
command_inhibit_type()
//----------------------------------------------------------------------
// Process an inhibit type command
//----------------------------------------------------------------------
{
	int station;
	int i;

	switch (command_buffer[1]) {
	case '0':
		for (i = 0; i < MOAS_STATIONS; i++) {
			inhibit_type[i] = FALSE;
		}
		break;

	case '1':
		for (i = 0; i < MOAS_STATIONS; i++) {
			inhibit_type[i] = TRUE;
		}
		break;

	case 'T':
		for (i = 2; command_buffer[i] != ';'; i++) {
			station = command_buffer[i] - '1';
			if ((station < 0) || (station >= MOAS_STATIONS)) {
				moas_callback_write("?A;");
				return;
			}
			inhibit_type[i] = TRUE;
		}
		break;

	case 'A':
		for (i = 2; command_buffer[i] != ';'; i++) {
			station = command_buffer[i] - '1';
			if ((station < 0) || (station >= MOAS_STATIONS)) {
				moas_callback_write("?A;");
				return;
			}
			inhibit_type[i] = FALSE;
		}
		break;

	default:
		moas_callback_write("?A;");
		break;
	}
}

static void
command_inhibit_time()
//----------------------------------------------------------------------
// Process an inhibit time command
//----------------------------------------------------------------------
{
	// Timers are ignored in the emulator
}

static void
command_interrupt_mode_delay()
//----------------------------------------------------------------------
// Process an interrupt mode delay command
//----------------------------------------------------------------------
{
	// Timers are ignored in the emulator
}

static void
command_mode()
//----------------------------------------------------------------------
// Process a mode command
//----------------------------------------------------------------------
{
	int i;
	int station;

	if ((command_buffer[1] != 'W') && (command_buffer[1] != 'I')) {
		moas_callback_write("?A;");
		return;
	}

	for (i=2; command_buffer[i] != ';'; i++) {
		station = command_buffer[i] - '1';
		if ((station < 0) || (station >= MOAS_STATIONS)) {
			moas_callback_write("?A;");
			return;
		}
		if (command_buffer[1] == 'W') {
			wait_mode |= 1<<station;
		}
		else {
			wait_mode &= ~(1<<station);
		}
	}
}

static void
command_ping()
//----------------------------------------------------------------------
// Process a ping command
//----------------------------------------------------------------------
{
	if (operate) {
		moas_callback_write("';");
	}
	else {
		moas_callback_write(".;");
	}
}

static void
command_receive_delay()
//----------------------------------------------------------------------
// Process a receive delay command
//----------------------------------------------------------------------
{
	// Timers are ignored in the emulator
}

static void
command_relay_status()
//----------------------------------------------------------------------
// Process a relay status command
//----------------------------------------------------------------------
{
	char buffer[(MOAS_RELAYS+5)/6+3];
	int i;
	int j = 1;
	int ry = 0;

	buffer[0] =	'|';

	for (i=63; i>=0; i--) {
		ry = ry << 1;
		if (actual_relays[i]) {
			ry++;
		}
		if (!(i%6)) {
			buffer[j++] = sixbit[ry];
		}
	}

	buffer[j++] = ';';
	buffer[j] = 0;
	moas_callback_write(buffer);
}

static void
command_set_state()
//----------------------------------------------------------------------
// Process a set state command
//----------------------------------------------------------------------
{
	int i;

	for (i=1; command_buffer[i]!=';'; i++) {
		switch (command_buffer[i]) {
		case '0':
			moas_initialize();
			return;

		case '1':
			operate = TRUE;
			do_pins();
			return;

		case 'A':
			antenna_events = TRUE;
			break;

		case 'a':
			antenna_events = FALSE;
			break;

		case 'T':
			tr_events = TRUE;
			break;
			
		case 't':
			tr_events = FALSE;
			break;

		case 'I':
			inhibit_events = TRUE;
			break;

		case 'i':
			inhibit_events = FALSE;
			break;

		case 'R':
			resolver_on = TRUE;
			break;

		case 'r':
			resolver_on = FALSE;
			break;

		case 'X':
			extra_relay_events = TRUE;
			break;

		case 'x':
			extra_relay_events = FALSE;
			break;

		default:
			moas_callback_write("?A;");
			break;
		}
	}
}

static void
command_status()
//----------------------------------------------------------------------
// Process a status command
//----------------------------------------------------------------------
{
	char buffer[4*MOAS_STATIONS+4];
	int i;
	int j;

	if (command_buffer[1] == 'B') {
		buffer[0] = '"';
		buffer[1] = 'B';

		// Add the transmit/receive/inhibit status
		for (i=0; i<MOAS_STATIONS; i++) {
			if (command_inhibits & (1<<i)) {
				buffer[i+2] = 'I';
			}
			else {
				if (trbits & (1<<i)) {
					buffer[i+2] = 'T';
				}
				else {
					buffer[i+2] = 'R';
				}
			}
		}

		// Add the transmit antennas
		for (i=0; i<MOAS_STATIONS; i++) {
			buffer[MOAS_STATIONS+i+2] = sixbit[actual_tx_antennas[i]];
		}

		// Add the receive antennas
		for (i=0; i<MOAS_STATIONS; i++) {
			buffer[(2*MOAS_STATIONS)+i+2] = sixbit[actual_rx_antennas[i]];
		}

		// Add the alternate antennas
		for (i=0; i<MOAS_STATIONS; i++) {
			buffer[(3*MOAS_STATIONS)+i+2] = sixbit[alternate_antennas[i]];
		}

		buffer[(4*MOAS_STATIONS)+2] = ';';
		buffer[(4*MOAS_STATIONS)+3] = '\0';
		moas_callback_write(buffer);
	}
	else {
		if (command_buffer[1] == 'I') {
			buffer[0] = '"';
			buffer[1] = 'I';
			j = 2;

			for (i=0; i<MOAS_STATIONS; i++) {
				if (inhibit_polarity[i]) {
					buffer[j++] = sixbit[i];
				}
			}
			buffer[j++] = ';';
			buffer[j++] = '\0';
			moas_callback_write(buffer);
		}
		else {
			moas_callback_write("?A;");
		}
	}
}

static void
command_system()
//----------------------------------------------------------------------
// Process an antenna system command
//----------------------------------------------------------------------
{
	int antenna;
	int system;
	int i;

	switch (command_buffer[1]) {
	case '0':
		for (i=0; i<MOAS_ANTENNAS; i++) {
			antenna_system_table[i] = 0;
		}
		break;

	case 'S':
		for (i=2; command_buffer[i] != ';'; i+=2) {
			antenna = sixtodigit(command_buffer[i]);
			if (command_buffer[i+1] == ';') {
				moas_callback_write("?A;");
				break;
			}
			system = sixtodigit(command_buffer[i+1]);
			antenna_system_table[antenna] = system;
		}
		break;

	default:
		moas_callback_write("?A;");
		break;
	}
}

static void
command_uninhibit()
//----------------------------------------------------------------------
// Process an uninhibit command
//----------------------------------------------------------------------
{
	int i;
	int station;

	for (i=1; command_buffer[i] != ';'; i++) {
		station = command_buffer[i] - '1';
		if ((station < 0) || (station >= MOAS_STATIONS)) {
			moas_callback_write("?A;");
			return;
		}
		command_inhibits &= ~(1<<station);
	}

	do_pins();
}

static void
command_unit_id()
//----------------------------------------------------------------------
// Process a unit ID command
//----------------------------------------------------------------------
{
	char buffer[8];
	int i;

	// If a unit ID was supplied set it
	if (command_buffer[1] != ';') {
		if ((command_buffer[1] < '0') || (command_buffer[1] > '9')) {
			moas_callback_write("?A;");
			return;
		}
		i = command_buffer[1] - '0';
	
		if (command_buffer[2] != ';') {
			if ((command_buffer[1] < '0') || (command_buffer[1] > '9')) {
				moas_callback_write("?A;");
				return;
			}
			i = (i * 10) + command_buffer[2] - '0';

			if (command_buffer[3] != ';') {
				moas_callback_write("?A;");
				return;
			}
		}
		unit_id = i;
	}

	buffer[0] = ':';
	buffer[1] = VER_MAJOR + '0';
	buffer[2] = (VER_MINOR / 10) + '0';
	buffer[3] = (VER_MINOR % 10) + '0';

	if (unit_id > 9) {
		buffer[4] = (unit_id / 10) + '0';
		buffer[5] = (unit_id % 10) + '0';
		buffer[6] = ';';
		buffer[7] = '\0';
	}
	else {
		buffer[4] = unit_id + '0';
		buffer[5] = ';';
		buffer[6] = '\0';
	}

	moas_callback_write(buffer);
}

static void
command_use_alternate_antenna()
//----------------------------------------------------------------------
// Process a use alternate antenna command
//----------------------------------------------------------------------
{
	int i;
	int station;
	int other;

	if (command_buffer[1] == ';') {
		moas_callback_write("?A;");
		return;
	}
	station = command_buffer[1] - '1';
	if ((station < 0) || (station >= MOAS_STATIONS)) {
		moas_callback_write("?A;");
		return;
	}


	alternates[station] = 0;


	for (i=2; command_buffer[i] != ';'; i++) {
		other = command_buffer[i] - '1';
		if ((other < 0) || (other >= MOAS_STATIONS)) {
			moas_callback_write("?A;");
			return;
		}
		if (other == station) {
			moas_callback_write("?A;");
			return;
		}
		alternates[station] |= 1<<other;
	}
}

static void
command_vendor_extension()
//----------------------------------------------------------------------
// Process a vendor extension command
//----------------------------------------------------------------------
{
}

void moas_character(char c)
//----------------------------------------------------------------------
// Handle a character received from the "serial port"
//----------------------------------------------------------------------
{
	// Ignore characters less than a space.  This includes CR and
	// LF which makes it easier to send commands from a terminal.
	if (c < ' ') {
		return;
	}

	// The dollar sign erases the current command.
	if (c == '$') {
		command_buffer_in = 0;
		return;
	}

	// Commands end with a semicolon.
	command_buffer[command_buffer_in++] = c;
	if (c != ';') {
		return;
	}

	command_buffer_in = 0;

	switch (command_buffer[0]) {

		// Antenna command
		case '!':
			command_antenna();
			break;

		// Status command
		case '"':
			command_status();
			break;

		// Vendor command
		case '#':
			command_vendor_extension();
			break;

		// Conflict command
		case '%':
			command_conflict_table();
			break;

		// Fast command
		case '&':
			command_fast_table();
			break;

		// Ping command
		case '\'':
			command_ping();
			break;

		// Inhibit command
		case '(':
			command_inhibit();
			break;

		// Uninhibit command
		case ')':
			command_uninhibit();
			break;

		// State command
		case '*':
			command_set_state();
			break;

		// Mode command
		case '/':
			command_mode();
			break;

		// Unit ID command
		case ':':
			command_unit_id();
			break;

		// Inhibit Type command
		case '=':
			command_inhibit_type();
			break;

		// Alternate command
		case '@':
			command_use_alternate_antenna();
			break;

		// Inhibit time command
		case '[':
			command_inhibit_time();
			break;

		// RX delay command
		case '\\':
			command_receive_delay();
			break;

		// Force RX time command
		case ']':
			command_interrupt_mode_delay();
			break;

		// Inhibit polarity command
		case '^':
			command_inhibit_polarity();
			break;

		// Antenna system command
		case '_':
			command_system();
			break;

		// Relay status command
		case '|':
			command_relay_status();
			break;

		// Inhibit station command
		case '~':
			command_inhibit_other_station();
			break;

		default:
			moas_callback_write("?U;");
			break;
	}
}

void moas_txrx(int station, int state)
//----------------------------------------------------------------------
// Handle a transmit/receive change
//----------------------------------------------------------------------
{
	char buffer[32];
	int inhibits = command_inhibits;
	int stn;

	// Internal calculations are zero-based
	station--;

	// Figure out which stations are inhibited
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (inhibits & (1<<stn)) {
			continue;
		}
		if (trbits & (1<<stn)) {
			inhibits |= cross_inhibits[stn];
		}
	}

	if (state) {
		trbits |= (1<<station);
		if (tr_events && (!(inhibits & 1<<station))) {
			buffer[0] = '<';
			buffer[1] = station + '1';
			buffer[2] = sixbit[actual_rx_antennas[station]];
			buffer[3] = ';';
			buffer[4] = '\0';
			moas_callback_write(buffer);
		}
	}
	else {
		trbits &= ~(1<<station);
		if (tr_events && (!(inhibits & 1<<station))) {
			buffer[0] = '>';
			buffer[1] = station + '1';
			buffer[2] = sixbit[actual_rx_antennas[station]];
			buffer[3] = ';';
			buffer[4] = '\0';
			moas_callback_write(buffer);
		}
	}

	do_resolver();
}

static void do_pins()
//----------------------------------------------------------------------
// Update outputs due to a possible state change
//----------------------------------------------------------------------
{
	int inhibits = command_inhibits;
	int temp_inhibits[MOAS_STATIONS];
	int tr_temp;

	int alts;

	int stn;
	int i;

	if (!operate) {
		// If not in operate mode show all stations as inhibited
		for (i=0; i<MOAS_STATIONS; i++) {
			temp_inhibits[i] = TRUE;
		}

		moas_callback_update(actual_relays, temp_inhibits);
		return;
	}

	// Figure out which stations are inhibited
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (inhibits & (1<<stn)) {
			continue;
		}
		if (trbits & (1<<stn)) {
			inhibits |= cross_inhibits[stn];
		}
	}

	// Set or reset relays as needed due to stations starting
	// to transmit
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if ((trbits & (1<<stn)) && !(tr_last & (1<<stn))) {
			for (i=0; i<MOAS_RELAYS; i++) {
				if (set_relays[i][stn]) {
					sr_relays[i] = TRUE;
				}
				if (reset_relays[i][stn]) {
					sr_relays[i] = FALSE;
				}
			}
		}
	}

	// Stations which are transmitting are not inhibited
	tr_temp = trbits & (~inhibits);

	// Adjust inhibits based on inhibit only on transmit
	for (stn = 0; stn < MOAS_STATIONS; stn++) {
		if (inhibit_type[stn] && (trbits & (1 << stn))) {
			inhibits &= (~(1 << stn));
		}
	}

	// Figure out which stations need alternates
	alts = 0;
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (tr_temp & (1<<stn)) {
			alts |= alternates[stn];
		}
	}

	// Set the global relays and set/reset relays
	for (i=0; i<MOAS_RELAYS; i++) {
		actual_relays[i] = global_relays[i] || sr_relays[i];
	}

	// Set the relays for each station
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (tr_temp & (1<<stn)) {
			for (i=0; i<MOAS_RELAYS; i++) {
				actual_relays[i] = actual_relays[i] || actual_tx_relays[i][stn];
			}
		}
		else {
			if (alts & (1<<stn)) {
				// Load the alternate antenna if it has no conflict.
				// Otherwise load no relays.
				for (i=0; i<MOAS_RELAYS; i++) {
					actual_relays[i] = actual_relays[i] || actual_alternate_relays[i][stn];
				}
			}
			else {
				for (i=0; i<MOAS_RELAYS; i++) {
					actual_relays[i] = actual_relays[i] || actual_rx_relays[i][stn];
				}
			}
		}
	}

	// Set up inhibits for the callback
	for (i=0; i<MOAS_STATIONS; i++) {
		temp_inhibits[i] = ((inhibits & (1<<i)) != 0);
	}

	// Give the emulator the current information
	moas_callback_update(actual_relays, temp_inhibits);

	tr_last = trbits;
}

static void do_resolver()
//----------------------------------------------------------------------
// Run the conflict resolver and update antennas
//----------------------------------------------------------------------
{
	char buffer[32];
	int stn;
	int dependencies;
	int temp_tx_pending = tx_pending;
	int temp_rx_pending = rx_pending;
	int attempt_tx_pending;
	int attempt_rx_pending;
	int alts;

	int alt_conflicts;
	int has_conflicts;

	int inhibits;
	int tr_temp;

	int i;



	if (!operate) {
		do_pins();
		return;
	}

	// Figure out which stations are inhibited
	inhibits = command_inhibits;
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (inhibits & (1<<stn)) {
			continue;
		}
		if (trbits & (1<<stn)) {
			inhibits |= cross_inhibits[stn];
		}
	}
	tr_temp = trbits & ~inhibits;

	// Handle pending extra relays.  The station must
	// be in receive state to transfer them.
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if ((extra_pending & (1<<stn)) && !(tr_temp & (1<<stn))) {
			for (i=0; i<MOAS_RELAYS; i++) {
				current_extra_relays[i][stn] = pending_extra_relays[i][stn];
				actual_tx_relays[i][stn] = current_tx_relays[i][stn] || current_extra_relays[i][stn];
			}
			if (extra_relay_events) {
				buffer[0] = '!';
				buffer[1] = stn + '1';
				buffer[2] = 'X';
				buffer[3] = ';';
				buffer[4] = '\0';
				moas_callback_write(buffer);
			}
			extra_pending &= ~(1<<stn);
		}
	}

	// Gather the alternate antenna requirements
	alts = 0;
	alt_conflicts = 0;
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (tr_temp & (1<<stn)) {
			alts |= alternates[stn];
		}
	}

	// Check for pending transmit antenna changes
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (tx_pending & (1<<stn)) {

			// Station has a pending antenna.  Check to see
			// if it is part of a shared system and find out
			// what other stations are using the system.
			int ant = pending_tx_antennas[stn];
			int sys = antenna_system_table[ant];
			if (sys) {
				dependencies = 0;
				for (i=0; i<MOAS_STATIONS; i++) {
					// If either the current or pending antennas are
					// part of the system this station must be checked.
					// This takes care of the case where a station is
					// changing to or from an antenna which is not part
					// of the system.
					if ((tx_pending & (1<<i)) &&
						((sys == antenna_system_table[pending_tx_antennas[i]]) ||
						 (sys == antenna_system_table[current_tx_antennas[i]]))) {
						dependencies |= (1<<i);
					}
				}
			}
			else {
				dependencies = 1<<stn;
			}

			// Stations which are in inhibit mode can be in transmit
			// because they can be forced into receive.  So if all
			// dependencies are in inhibit mode it can be done.
			if (dependencies & wait_mode) {

				// If the station is transmitting in wait mode it cannot
				// be changed now.
				if (tr_temp & dependencies) {
					temp_tx_pending &= ~(1<<stn);
				}
			}
		}
	}

	// Check for pending receive antenna changes
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (rx_pending & (1<<stn)) {

			// Station has a pending antenna.  Check to see
			// if it is part of a shared system and find out
			// what other stations are using the system.
			int ant = pending_rx_antennas[stn];
			int sys = antenna_system_table[ant];

			// Normally a receive antenna can be changed any time
			// because if the station is transmitting the change will
			// not take effect until it goes into receive.  However
			// if the station is part of a shared system the change
			// cannot be done when any station is transmitting because
			// it will hot-switch the system.
			if (sys) {
				dependencies = 0;
				for (i=0; i<MOAS_STATIONS; i++) {
					// If either the current or pending antennas are
					// part of the system this station must be checked.
					// This takes care of the case where a station is
					// changing to or from an antenna which is not part
					// of the system.
					if ((tx_pending & (1<<i)) &&
						((sys == antenna_system_table[pending_tx_antennas[i]]) ||
						 (sys == antenna_system_table[current_tx_antennas[i]]))) {
						dependencies |= (1<<i);
					}
				}
	
				// Stations which are in inhibit mode can be in transmit
				// because they can be forced into receive.  So if all
				// dependencies are in inhibit mode it can be done.
				if (dependencies & wait_mode) {

					// If the station is transmitting in wait mode it cannot
					// be changed now.
					if (tr_temp & dependencies) {
						temp_rx_pending &= ~(1<<stn);
					}
				}
			}
		}
	}

	// Check for conflicts with pending transmit antenna changes
	attempt_tx_pending = temp_tx_pending;
	attempt_rx_pending = temp_rx_pending;

	while (attempt_tx_pending) {
		has_conflicts = FALSE;

		for (stn=0; stn<MOAS_STATIONS; stn++) {
			if (attempt_tx_pending & (1<<stn)) {
				int ant = pending_tx_antennas[stn];

				// Check for conflicts with other antennas
				for (i=0; i<MOAS_STATIONS; i++) {
					int other_tx;
					int other_rx;

					if (i == stn) {
						continue;
					}

					if (attempt_tx_pending & (1<<i)) {
						other_tx = pending_tx_antennas[i];
					}
					else {
						other_tx = current_tx_antennas[i];
					}

					if (attempt_rx_pending & (1<<i)) {
						other_rx = pending_rx_antennas[i];
					}
					else {
						other_rx = current_rx_antennas[i];
					}

					if ((conflicts_table[ant][other_tx]) ||
						(conflicts_table[ant][other_rx])) {
						has_conflicts = TRUE;
						if (antenna_events && !conflict_sent_tx[stn]) {
							buffer[0] = '!';
							buffer[1] = stn + '1';
							buffer[2] = 'C';
							buffer[3] = sixbit[pending_tx_antennas[stn]];
							buffer[4] = ';';
							buffer[5] = '\0';
							moas_callback_write(buffer);
							conflict_sent_tx[stn] = TRUE;
						}
						break;
					}
				}
			}
		}
		if (!has_conflicts) {
			break;
		}
		
		// This algorithm tries all combinations of antennas.
		// And it tries each exactly once.
		attempt_tx_pending = (attempt_tx_pending - 1) & temp_tx_pending;
	}

	// Check for conflicts with pending receive antenna changes
	while (attempt_rx_pending) {
		has_conflicts = FALSE;

		for (stn=0; stn<MOAS_STATIONS; stn++) {
			if (attempt_rx_pending & (1<<stn)) {
				int ant = pending_rx_antennas[stn];

				// Check for conflicts with other antennas
				for (i=0; i<MOAS_STATIONS; i++) {
					int other_tx;
					int other_rx;

					if (i == stn) {
						continue;
					}

					if (attempt_tx_pending & (1<<i)) {
						other_tx = pending_tx_antennas[i];
					}
					else {
						other_tx = current_tx_antennas[i];
					}

					if (attempt_rx_pending & (1<<i)) {
						other_rx = pending_rx_antennas[i];
					}
					else {
						other_rx = current_rx_antennas[i];
					}

					if ((conflicts_table[ant][other_tx]) ||
						(conflicts_table[ant][other_rx])) {
						has_conflicts = TRUE;
						if (antenna_events && !conflict_sent_rx[stn]) {
							buffer[0] = '!';
							buffer[1] = stn + '1';
							buffer[2] = 'c';
							buffer[3] = sixbit[pending_rx_antennas[stn]];
							buffer[4] = ';';
							buffer[5] = '\0';
							moas_callback_write(buffer);
							conflict_sent_rx[stn] = TRUE;
						}
						break;
					}
				}
			}
		}
		if (!has_conflicts) {
			break;
		}

		// This algorithm tries all combinations of antennas.
		// And it tries each exactly once.
		attempt_rx_pending = (attempt_rx_pending - 1) & temp_rx_pending;
	}

	//Check for conflicts with alternates
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (alts & (1<<stn)) {
			int ant = alternate_antennas[stn];
			int conflict = FALSE;

			// Check for conflicts with other antennas
			for (i=0; i<MOAS_STATIONS; i++) {
				int other_tx;
				int other_rx;

				if (i == stn) {
					continue;
				}

				if (attempt_tx_pending & (1<<i)) {
					other_tx = pending_tx_antennas[i];
				}
				else {
					other_tx = current_tx_antennas[i];
				}

				if (attempt_rx_pending & (1<<i)) {
					other_rx = pending_rx_antennas[i];
				}
				else {
					other_rx = current_rx_antennas[i];
				}

				if ((conflicts_table[ant][other_tx]) ||
					(conflicts_table[ant][other_rx])) {
						conflict = TRUE;
						break;
				}
			}
			if (conflict) {
				for (i=0; i<MOAS_RELAYS; i++) {
					actual_alternate_relays[i][stn] = FALSE;
				}
			}
			else {
				for (i=0; i<MOAS_RELAYS; i++) {
					actual_alternate_relays[i][stn] = alternate_relays[i][stn];
				}
			}
		}
	}


	// If there is nothing pending update the outputs
	// and return
	if (!attempt_tx_pending && !attempt_rx_pending) {
		do_pins();
		return;
	}

	// Move pending transmit antennas to current and actual
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		if (attempt_tx_pending & (1<<stn)) {
			int ant = pending_tx_antennas[stn];
			current_tx_antennas[stn] = ant;
			actual_tx_antennas[stn] = ant;
			for (i=0; i<MOAS_RELAYS; i++) {
				int relay = pending_tx_relays[i][stn];
				current_tx_relays[i][stn] = relay;
				actual_tx_relays[i][stn] = relay || current_extra_relays[i][stn];
			}
			conflict_sent_tx[stn] = FALSE;
		}
	}

	// Move pending receive antennas to current and actual
	// The actual antenna could be the current receive
	// antenna or the current transmit antenna.
	for (stn=0; stn<MOAS_STATIONS; stn++) {
		int ant;

		if (attempt_rx_pending & (1<<stn)) {
			ant = pending_rx_antennas[stn];

			current_rx_antennas[stn] = ant;			
			for (i=0; i<MOAS_RELAYS; i++) {
				int relay = pending_rx_relays[i][stn];
				current_rx_relays[i][stn] = relay;
			}
			conflict_sent_rx[stn] = FALSE;
		}
		else {
			ant = current_rx_antennas[stn];
		}

		if (fast_table[ant][current_tx_antennas[stn]]) {
			actual_rx_antennas[stn] = current_rx_antennas[stn];
			for (i=0; i<MOAS_RELAYS; i++) {
				int relay = current_rx_relays[i][stn];
				actual_rx_relays[i][stn] = relay;
			}
		}
		else {
			actual_rx_antennas[stn] = current_tx_antennas[stn];
			for (i=0; i<MOAS_RELAYS; i++) {
				int relay = current_tx_relays[i][stn];
				actual_rx_relays[i][stn] = relay;
			}
		}
	}

	// Notify controlling program of any antenna changes if desired
	if (antenna_events) {
		for (stn=0; stn<MOAS_STATIONS; stn++) {
			if (attempt_tx_pending & (1<<stn)) {
				buffer[0] = '!';
				buffer[1] = stn + '1';
				if (fast_table[current_tx_antennas[stn]][current_rx_antennas[stn]]) {
					buffer[2] = 'F';
				}
				else {
					buffer[2] = 'S';
				}
				buffer[3] = sixbit[current_tx_antennas[stn]];
				buffer[4] = ';';
				buffer[5] = '\0';
				moas_callback_write(buffer);
			}

			if (attempt_rx_pending & (1<<stn)) {
				buffer[0] = '!';
				buffer[1] = stn + '1';
				if (fast_table[current_tx_antennas[stn]][current_rx_antennas[stn]]) {
					buffer[2] = 'f';
				}
				else {
					buffer[2] = 's';
				}
				buffer[3] = sixbit[current_rx_antennas[stn]];
				buffer[4] = ';';
				buffer[5] = '\0';
				moas_callback_write(buffer);
			}

			if (alt_pending & (1<<stn)) {
				buffer[0] = '!';
				buffer[1] = stn + '1';
				if (alt_conflicts & (1<<stn)) {
					buffer[2] = 'a';
				}
				else {
					buffer[2] = 'A';
				}
				buffer[3] = sixbit[alternate_antennas[stn]];
				buffer[4] = ';';
				buffer[5] = '\0';
				moas_callback_write(buffer);
			}
		}
	}

	// Remove completed transitions from pending
	tx_pending &= ~attempt_tx_pending;
	rx_pending &= ~attempt_rx_pending;
	alt_pending = 0;

	moas_callback_antennas(actual_tx_antennas, actual_rx_antennas);
	do_pins();
}