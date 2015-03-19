#ifndef __rtc_common_h__
#define __rtc_common_h__

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <Time.h>

enum alarmMode_t {
  alarmModeUnknown,       // not in spec table
  alarmModePerSecond,     // once per second, A1 only
  alarmModePerMinute,     // once per minute, A2 only
  alarmModeSecondsMatch,  // when seconds match, A1 only
  alarmModeMinutesMatch,  // when minutes [and seconds] match
  alarmModeHoursMatch,    // when hours, minutes [and seconds] match
  alarmModeDateMatch,     // when date (of month), hours, minutes [and seconds] match
  alarmModeDayMatch,      // when day (of week), hours, minutes [and seconds] match
  alarmModeOff            // set to date or day, but value is 0
  };

enum sqiMode_t {
  sqiModeNone, 
  sqiMode1Hz, 
  sqiMode1024Hz, 
  sqiMode4096Hz, 
  sqiMode8192Hz, 
  sqiModeAlarm1, 
  sqiModeAlarm2, 
  sqiModeAlarmBoth
  };

enum tempScanRate_t {
  tempScanRate64sec,
  tempScanRate128sec,
  tempScanRate256sec,
  tempScanRate512sec
  };

typedef struct {
  int8_t Temp; 
  uint8_t Decimal; 
} tpElements_t, TempElements, *tpElementsPtr_t;

// Bits in the Control register
// Based on page 13 of specs; http://www.maxim-ic.com/datasheet/index.mvp/id/4984
const uint8_t DS323X_EOSC        = 0x80;
const uint8_t DS323X_BBSQW       = 0x40;
const uint8_t DS323X_CONV        = 0x20;
const uint8_t DS323X_RS2         = 0x10;
const uint8_t DS323X_RS1         = 0x08;
const uint8_t DS323X_INTCN       = 0x04;
const uint8_t DS323X_A2IE        = 0x02;
const uint8_t DS323X_A1IE        = 0x01;

const uint8_t DS323X_RS_1HZ      = 0x00;
const uint8_t DS323X_RS_1024HZ   = 0x08;
const uint8_t DS323X_RS_4096HZ   = 0x10;
const uint8_t DS323X_RS_8192HZ   = 0x18;

// Bits in the Status register
// Based on page 14 of specs; http://www.maxim-ic.com/datasheet/index.mvp/id/4984
const uint8_t DS323X_OSF         = 0x80;
const uint8_t DS323X_BB33KHZ     = 0x40;
const uint8_t DS323X_CRATE1      = 0x20;
const uint8_t DS323X_CRATE0      = 0x10;
const uint8_t DS323X_EN33KHZ     = 0x08;
const uint8_t DS323X_BSY         = 0x04;
const uint8_t DS323X_A2F         = 0x02;
const uint8_t DS323X_A1F         = 0x01;

const uint8_t DS323X_CRATE_64    = 0x00;
const uint8_t DS323X_CRATE_128   = 0x10;
const uint8_t DS323X_CRATE_256   = 0x20;
const uint8_t DS323X_CRATE_512   = 0x30;

// Addresses
const uint8_t DS323X_TIME_REGS   = 0x00;
const uint8_t DS323X_DATE_REGS   = 0x03;
const uint8_t DS323X_ALARM1_REGS = 0x07;
const uint8_t DS323X_ALARM2_REGS = 0x0B;
const uint8_t DS323X_CONTROL_REG = 0x0E;
const uint8_t DS323X_STATUS_REG  = 0x0F;
const uint8_t DS323X_TEMP_MSB    = 0x11;
const uint8_t DS323X_TEMP_LSB    = 0x12;

const uint8_t NO_TEMPERATURE     = 0x7F;

class DSRTC {
    public:
        // Date and Time
        void read(tmElements_t &tm);
        uint8_t dec2bcd(const uint8_t val);
        uint8_t bcd2dec(const uint8_t val);
        bool available();
        time_t get();
        void write(tmElements_t &tm);
        void writeTime(tmElements_t &tm);
        void writeDate(tmElements_t &tm);
        // Alarms
        void readAlarm(uint8_t alarm, alarmMode_t &mode, tmElements_t &tm);
        void writeAlarm(uint8_t alarm, alarmMode_t mode, tmElements_t tm);
        // Control Register
        void setBBOscillator(bool enable);
        void setBBSqareWave(bool enable);
        void setSQIMode(sqiMode_t mode);
        bool isAlarmInterrupt(uint8_t alarm);
        uint8_t readControlRegister();
        void writeControlRegister(uint8_t value);
        // Status Register
        bool isOscillatorStopFlag();
        void setOscillatorStopFlag(bool enable);
        void setBB33kHzOutput(bool enable);
        void setTCXORate(tempScanRate_t rate);
        void set33kHzOutput(bool enable);
        bool isTCXOBusy();
        bool isAlarmFlag(uint8_t alarm);
        uint8_t isAlarmFlag();
        void clearAlarmFlag(uint8_t alarm);
        uint8_t readStatusRegister();
        void writeStatusRegister(uint8_t value);
        // Temperature
        void readTemperature(tpElements_t &tmp);
    protected:
        void populateTimeElements( tmElements_t &tm, uint8_t TimeDate[] );
        void populateDateElements( tmElements_t &tm, uint8_t TimeDate[] );
    private:
        virtual uint8_t read1(uint8_t addr);
        virtual void write1(uint8_t addr, uint8_t data);
        virtual void readN(uint8_t addr, uint8_t buf[], uint8_t len);
        virtual void writeN(uint8_t addr, uint8_t buf[], uint8_t len);
};

#endif
