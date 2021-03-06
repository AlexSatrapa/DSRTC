#include <DSRTC.h>

byte DSRTC::dec2bcd(const byte val)
{
	return ((val / 10 * 16) + (val % 10));
}

byte DSRTC::bcd2dec(const byte val)
{
	return ((val / 16 * 10) + (val % 16));
}

bool DSRTC::available() {
	return 1;
}

void DSRTC::read( dsrtc_calendar_t &tm )
{
	byte TimeDate[7];      //second,minute,hour,dow,day,month,year
	byte century = 0;

	readN(DS323X_TIME_REGS, TimeDate, 7);

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
}

inline void DSRTC::populateTimeElements( dsrtc_calendar_t &tm, byte TimeDate[] )
{
	TimeDate[0] = dec2bcd(tm.Second);
	TimeDate[1] = dec2bcd(tm.Minute);
	TimeDate[2] = dec2bcd(tm.Hour);
}

inline void DSRTC::populateDateElements( dsrtc_calendar_t &tm, byte TimeDate[] )
{
	if( tm.Wday == 0 || tm.Wday > 7)
	{
		tm.Wday = 1;
	}
	TimeDate[3] = tm.Wday;
	TimeDate[4] = dec2bcd(tm.Day);
	TimeDate[5] = dec2bcd(tm.Month);
	TimeDate[6] = dec2bcd(tm.Year % 100);
} 

void DSRTC::writeDate( dsrtc_calendar_t &tm )
{
	byte TimeDate[7];

	populateDateElements(tm, TimeDate);

	writeN(DS323X_DATE_REGS, TimeDate + 3 * sizeof(byte), 4);
}

void DSRTC::writeTime( dsrtc_calendar_t &tm )
{
	byte TimeDate[7];
	populateTimeElements( tm, TimeDate );

	writeN(DS323X_TIME_REGS, TimeDate, 3);
}

void DSRTC::write( dsrtc_calendar_t &tm )
{
	byte TimeDate[7];

	populateTimeElements(tm, TimeDate);
	populateDateElements(tm, TimeDate);

	writeN(DS323X_TIME_REGS, TimeDate, 7);
}

void DSRTC::readTemperature(tpElements_t &tmp)
{
	byte data[2];

	readN(DS323X_TEMP_MSB, data, 2);

	tmp.Temp = data[0];
	tmp.Decimal = (data[1] >> 6) * 25;
}

void DSRTC::readAlarm(byte alarm, alarmMode_t &mode, dsrtc_calendar_t &tm)
{
	byte data[4];
	byte flags;
	byte addr, offset, length;

	memset(&tm, 0, sizeof(dsrtc_calendar_t));
	mode = alarmModeUnknown;
	if ((alarm < 1) || (alarm > 2)) return;
	if (alarm == 1)
	{
		addr = DS323X_ALARM1_REGS;
		offset = 0;
		length = 4;
	} else {
		addr = DS323X_ALARM2_REGS;
		offset = 1;
		length = 3;
	}

	data[0] = 0;
	readN(addr, data + offset * sizeof(byte), length);

	flags =
		((data[0] & 0x80) >> 7) |
		((data[1] & 0x80) >> 6) |
		((data[2] & 0x80) >> 5) |
		((data[3] & 0x80) >> 4);
	if (flags == 0) flags = ((data[3] & 0x40) >> 2);
	switch (flags) {
		case 0x04: mode = alarmModePerSecond; break;  // X1111
		case 0x0E: mode = (alarm == 1) ? alarmModeSecondsMatch : alarmModePerMinute; break;  // X1110
		case 0x0C: mode = alarmModeMinutesMatch; break;  // X1100
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

void DSRTC::writeAlarm(byte alarm, alarmMode_t mode, dsrtc_calendar_t tm) {
	byte data[4];
	byte addr, offset, length;

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
		addr = DS323X_ALARM1_REGS;
		offset = 0;
		length = 4;
	} else {
		addr = DS323X_ALARM2_REGS;
		offset = 1;
		length = 3;
	}

	writeN( addr, data + offset * sizeof(byte), length);
}

bool DSRTC::isAlarmInterrupt(byte alarm)
{
	if ((alarm > 2) || (alarm < 1)) return false;
	byte value = readControlRegister() & (DS323X_A1IE | DS323X_A2IE | DS323X_INTCN);
	if (alarm == 1) {
		return ((value & (DS323X_A1IE | DS323X_INTCN)) == (DS323X_A1IE | DS323X_INTCN));
	} else {
		return ((value & (DS323X_A2IE | DS323X_INTCN)) == (DS323X_A2IE | DS323X_INTCN));
	}
}

byte DSRTC::isAlarmFlag()
{
	byte value = readStatusRegister();
	return (value & (DS323X_A1F | DS323X_A2F));
}

bool DSRTC::isAlarmFlag(byte alarm)
{
	byte value = isAlarmFlag();
	return ((value & alarm) != 0);
}

void DSRTC::clearAlarmFlag(byte alarm)
{
	byte alarm_mask, value;

	if ((alarm != 1) and (alarm != 2) and (alarm != 3)) return;
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

byte DSRTC::readControlRegister()
{
	return read1(DS323X_CONTROL_REG);
}

void DSRTC::writeControlRegister(byte value)
{
	write1(DS323X_CONTROL_REG, value);
}

void DSRTC::setBBOscillator(bool enable)
{
	byte value = readControlRegister();
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
	byte value = readControlRegister();
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
	byte value = readControlRegister() & 0xE0;
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
byte DSRTC::readStatusRegister()
{
	return read1(DS323X_STATUS_REG);
}

void DSRTC::writeStatusRegister(byte value)
{
	write1(DS323X_STATUS_REG, value);
}

bool DSRTC::isOscillatorStopFlag()
{
	return ((readStatusRegister() & DS323X_OSF) != 0);
}

void DSRTC::setOscillatorStopFlag(bool enable)
{
	byte value = readStatusRegister();
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
	byte value = readStatusRegister();
	if (enable)
	{
		value |= DS323X_BB33KHZ;
	} else {
		value &= ~DS323X_BB33KHZ;
	}
}

void DSRTC::setTCXORate(tempScanRate_t rate)
{
	byte value = readStatusRegister() & ~(DS323X_CRATE1|DS323X_CRATE0); // clear the rate bits
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
	byte value = readStatusRegister();
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
