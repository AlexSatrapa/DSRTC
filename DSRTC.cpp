#include <RTC.h>

// helpers

uint8_t DSRTC::dec2bcd(const uint8_t val)
{
    return ((val / 10 * 16) + (val % 10));
}

uint8_t DSRTC::bcd2dec(const uint8_t val)
{
    return ((val / 16 * 10) + (val % 16));
}

bool DSRTC::available() {
	return 1;
}

time_t DSRTC::get()
{
	tmElements_t tm;
	read(tm);
	return makeTime(tm);
}

void DSRTC::read( tmElements_t &tm )
{
	uint8_t TimeDate[7];      //second,minute,hour,dow,day,month,year
	uint8_t century = 0;

	readN(0x00, TimeDate, 7);

	tm.Second = bcd2dec(TimeDate[0] & 0x7F);
	tm.Minute = bcd2dec(TimeDate[1] & 0x7F);
	if ((TimeDate[2] & 0x40) != 0)
	{
		// Convert 12-hour format to 24-hour format
		tm.Hour = bcd2dec(TimeDate[2] & 0x1F);
		if((TimeDate[2] & 0x20) != 0) tm.Hour += 12;
	} else {
		tm.Hour = bcd2dec(TimeDate[2] & 0x3F);
	}
	tm.Wday = bcd2dec(TimeDate[3] & 0x07);
	tm.Day = bcd2dec(TimeDate[4] & 0x3F);
	tm.Month = bcd2dec(TimeDate[5] & 0x1F);
	tm.Year = bcd2dec(TimeDate[6]);
	century = (TimeDate[5] & 0x80);
	if (century != 0) tm.Year += 100;
	tm.Year = y2kYearToTm(tm.Year);
}

void DSRTC::writeDate( tmElements_t &tm )
{
	uint8_t y;
	uint8_t TimeDate[7];

	if( tm.Wday == 0 || tm.Wday > 7)
	{
		tmElements_t tm2;
		breakTime( makeTime(tm), tm2 );  // Calculate Wday by converting to Unix time and back
		tm.Wday = tm2.Wday;
	}
	TimeDate[3] = tm.Wday;
	TimeDate[4] = dec2bcd(tm.Day);
	TimeDate[5] = dec2bcd(tm.Month);
	y = tmYearToY2k(tm.Year);
	if (y > 99)
	{
		TimeDate[5] |= 0x80; // century flag
		y -= 100;
	}
	TimeDate[6] = dec2bcd(y);

	// Write date to RTC
	writeN(0x03, TimeDate + 3 * sizeof(uint8_t), 3); // Request write into date registers
}

void DSRTC::writeTime( tmElements_t &tm )
{
	uint8_t TimeDate[7];

	TimeDate[0] = dec2bcd(tm.Second);
	TimeDate[1] = dec2bcd(tm.Minute);
	TimeDate[2] = dec2bcd(tm.Hour);

	writeN(0x00, TimeDate, 3);
}

void DSRTC::readTemperature(tpElements_t &tmp)
{
	uint8_t data[2];

	readN(0x11, data, 2);

	tmp.Temp = data[0];
	tmp.Decimal = (data[1] >> 6) * 25;
}

void DSRTC::readAlarm(uint8_t alarm, alarmMode_t &mode, tmElements_t &tm)
{
	uint8_t data[4];
	uint8_t flags;
	uint8_t addr, offset, length;

	memset(&tm, 0, sizeof(tmElements_t));
	mode = alarmModeUnknown;
	if ((alarm < 1) || (alarm > 2)) return;
	if (alarm == 1)
	{
		addr = 0x07;
		offset = 0;
		length = 4;
	} else {
		addr = 0x08;
		offset = 1;
		length = 3;
	}

	data[0] = 0;
	readN(addr, data + offset * sizeof(uint8_t), length);

	flags =
		((data[0] & 0x80) >> 7) |
		((data[1] & 0x80) >> 6) |
		((data[2] & 0x80) >> 5) |
		((data[3] & 0x80) >> 4);
	if (flags == 0) flags = ((data[3] & 0x40) >> 2);
	switch (flags) {
		case 0x04: mode = alarmModePerSecond; break;  // X1111
		case 0x0E: mode = (alarm == 1) ? alarmModeSecondsMatch : alarmModePerMinute; break;  // X1110
		case 0x0A: mode = alarmModeMinutesMatch; break;  // X1100
		case 0x08: mode = alarmModeHoursMatch; break;  // X1000
		case 0x00: mode = alarmModeDateMatch; break;  // 00000
		case 0x10: mode = alarmModeDayMatch; break;  // 10000
	}

	if (alarm == 1) tm.Second = bcd2dec(data[0] & 0x7F);
	tm.Minute = bcd2dec(data[1] & 0x7F);
	if ((data[2] & 0x40) != 0) {
		// 12 hour format with bit 5 set as PM
		tm.Hour = bcd2dec(data[2] & 0x1F);
		if ((data[2] & 0x20) != 0) tm.Hour += 12;
	} else {
		// 24 hour format
		tm.Hour = bcd2dec(data[2] & 0x3F);
	}
	if ((data[3] & 0x40) == 0) {
		// Alarm holds Date (of Month)
		tm.Day = bcd2dec(data[3] & 0x3F);
	} else {
		// Alarm holds Day (of Week)
		tm.Wday = bcd2dec(data[3] & 0x07);
	}

	// TODO : Not too sure about this.
	/*
	If the alarm is set to trigger every Nth of the month
	(or every 1-7 week day), but the date/day are 0 then
	what?  The spec is not clear about alarm off conditions.
	My assumption is that it would not trigger is date/day
	set to 0, so I've created a Alarm-Off mode.
	*/
	if ((mode == alarmModeDateMatch) && (tm.Day == 0)) {
		mode = alarmModeOff;
	} else if ((mode == alarmModeDayMatch) && (tm.Wday == 0)) {
		mode = alarmModeOff;
	}
}

void DSRTC::writeAlarm(uint8_t alarm, alarmMode_t mode, tmElements_t tm) {
	uint8_t data[4];
	uint8_t addr, offset, length;

	switch (mode) {
		case alarmModePerSecond:
			data[0] = 0x80;
			data[1] = 0x80;
			data[2] = 0x80;
			data[3] = 0x80;
			break;
		case alarmModePerMinute:
			data[0] = 0x00;
			data[1] = 0x80;
			data[2] = 0x80;
			data[3] = 0x80;
			break;
		case alarmModeSecondsMatch:
			data[0] = 0x00 | dec2bcd(tm.Second);
			data[1] = 0x80;
			data[2] = 0x80;
			data[3] = 0x80;
			break;
		case alarmModeMinutesMatch:
			data[0] = 0x00 | dec2bcd(tm.Second);
			data[1] = 0x00 | dec2bcd(tm.Minute);
			data[2] = 0x80;
			data[3] = 0x80;
			break;
		case alarmModeHoursMatch:
			data[0] = 0x00 | dec2bcd(tm.Second);
			data[1] = 0x00 | dec2bcd(tm.Minute);
			data[2] = 0x00 | dec2bcd(tm.Hour);
			data[3] = 0x80;
			break;
		case alarmModeDateMatch:
			data[0] = 0x00 | dec2bcd(tm.Second);
			data[1] = 0x00 | dec2bcd(tm.Minute);
			data[2] = 0x00 | dec2bcd(tm.Hour);
			data[3] = 0x00 | dec2bcd(tm.Day);
			break;
		case alarmModeDayMatch:
			data[0] = 0x00 | dec2bcd(tm.Second);
			data[1] = 0x00 | dec2bcd(tm.Minute);
			data[2] = 0x00 | dec2bcd(tm.Hour);
			data[3] = 0x40 | dec2bcd(tm.Wday);
			break;
		case alarmModeOff:
			data[0] = 0x00;
			data[1] = 0x00;
			data[2] = 0x00;
			data[3] = 0x00;
			break;
		default: return;
	}

	if (alarm == 1)
	{
		addr = 0x07;
		offset = 0;
		length = 4;
	} else {
		addr = 0x08;
		offset = 1;
		length = 3;
	}

	writeN( addr, data + offset * sizeof(uint8_t), length);
}

bool DSRTC::isAlarmInterrupt(uint8_t alarm)
{
	if ((alarm > 2) || (alarm < 1)) return false;
	uint8_t value = readControlRegister() & 0x07;  // sends 0Eh - Control register
	if (alarm == 1) {
		return ((value & 0x05) == 0x05);
	} else {
		return ((value & 0x06) == 0x06);
	}
}

uint8_t DSRTC::isAlarmFlag()
{
	uint8_t value = readStatusRegister(); // Status register
	return (value & (DS323X_A1F | DS323X_A2F));
}

bool DSRTC::isAlarmFlag(uint8_t alarm)
{
	uint8_t value = isAlarmFlag();
	return ((value & alarm) != 0);
}

void DSRTC::clearAlarmFlag(uint8_t alarm)
{
	uint8_t alarm_mask, value;

	if ((alarm != 1) and (alarm != 2)) return;
	alarm_mask = ~alarm;

	value = readStatusRegister();
	value &= alarm_mask;
	writeStatusRegister(value);
}

/*
 * Control Register
 *
 *   ~EOSC  BBSQW  CONV  RS2  RS1  INTCN  A2IE  A1IE
 */

uint8_t DSRTC::readControlRegister()
{
	return read1(0x0E);
}

void DSRTC::writeControlRegister(uint8_t value)
{
	write1(0x0E, value);
}

void DSRTC::setBBOscillator(bool enable)
{
	uint8_t value = readControlRegister();
	if (enable)
	{
		value |= DS323X_EOSC;
	} else {
		value &= ~DS323X_EOSC;
	}
	writeControlRegister(value);
}

void DSRTC::setBBSqareWave(bool enable)
{
	uint8_t value = readControlRegister();
	if (enable)
	{
		value |= DS323X_BBSQW;
	} else {
		value &= ~DS323X_BBSQW;
	}
	writeControlRegister(value);
}

void DSRTC::setSQIMode(sqiMode_t mode)
{
	uint8_t value = readControlRegister();
	switch (mode)
	{
		case sqiModeNone: value |= DS323X_INTCN; break;
		case sqiMode1Hz: value |= DS323X_RS_1HZ;  break;
		case sqiMode1024Hz: value |= DS323X_RS_1024HZ; break;
		case sqiMode4096Hz: value |= DS323X_RS_4096HZ; break;
		case sqiMode8192Hz: value |= DS323X_RS_8192HZ; break;
		case sqiModeAlarm1: value |= (DS323X_INTCN | DS323X_A1IE); break;
		case sqiModeAlarm2: value |= (DS323X_INTCN | DS323X_A2IE); break;
		case sqiModeAlarmBoth: value |= (DS323X_INTCN | DS323X_A1IE | DS323X_A2IE); break;
	}
	writeControlRegister(value);
}

/*
 * Status Register
 *
 *   OSF  BB33KHZ  CRATE1  CRATE0  EN33KHZ  BSY  A2F  A1F
 */
uint8_t DSRTC::readStatusRegister()
{
	return read1(0x0F);
}

void DSRTC::writeStatusRegister(uint8_t value)
{
	write1(0x0F, value);
}

bool DSRTC::isOscillatorStopFlag()
{
	return ((readStatusRegister() & DS323X_OSF) != 0);
}

void DSRTC::setOscillatorStopFlag(bool enable)
{
	uint8_t value = readStatusRegister();
	if (enable)
	{
		value |= DS323X_OSF;
	} else {
		value &= ~DS323X_OSF;
	}
	writeStatusRegister(value);
}

void DSRTC::setBB33kHzOutput(bool enable)
{
	uint8_t value = readStatusRegister();
	if (enable)
	{
		value |= DS323X_BB33KHZ;
	} else {
		value &= ~DS323X_BB33KHZ;
	}
}

void DSRTC::setTCXORate(tempScanRate_t rate)
{
	uint8_t value = readStatusRegister() & 0xCF; // clear the rate bits
	switch (rate)
	{
		case tempScanRate64sec: value |= DS323X_CRATE_64; break;
		case tempScanRate128sec: value |= DS323X_CRATE_128; break;
		case tempScanRate256sec: value |= DS323X_CRATE_256; break;
		case tempScanRate512sec: value |= DS323X_CRATE_512; break;
	}
	writeStatusRegister(value);
}

void DSRTC::set33kHzOutput(bool enable)
{
	uint8_t value = readStatusRegister();
	if (enable)
	{
		value |= DS323X_EN33KHZ;
	} else {
		value &= ~DS323X_EN33KHZ;
	}
	writeStatusRegister(value);
}

bool DSRTC::isTCXOBusy()
{
	return((readStatusRegister() & DS323X_BSY) != 0);
}
