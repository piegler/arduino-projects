/*
 * temphum
 *
 * Copyright (c) 2011 Daniel Berenguer <dberenguer@usapiens.com>
 * 
 * This file is part of the panStamp project.
 * 
 * panStamp  is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 * 
 * panStamp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with panStamp; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 * 
 * Author: Daniel Berenguer
 * Creation date: 11/31/2011
 *
 * Device:
 * Temperature sensor
 * Dual Temperature + Humidity sensor
 * Dual Pressure + Temperature sensor
 *
 * Description:
 * This sketch can generate three different devices depending on the
 * definition of a pre-compiler contstant in sensor.h (only enable one
 * of these options):
 *
 * TEMP: Device measuring temperature from a TMP36 sensor.
 * Pins: PIN_ADCTEMP (ADC input) and PIN_PWRTEMP (power pin)
 *
 * TEMPHUM: Device measuring temperature and humidity from a DHT11/DHT22
 * sensor. In case you use a DHT11 sensor, you will need this library:
 * http://arduino.cc/playground/Main/DHT11Lib
 *
 * Pins: DHT_DATA (digital I/O) and PIN_PWRDHT (power pin)
 *
 * TEMPPRESS: Device measuring temperature and barometric pressure from
 * an I2C BMP085 sensor. This sketch makes use of Adafruit's BMP085 library
 * for Arduino: http://learn.adafruit.com/bmp085/using-the-bmp085
 * Pins: I2C port and PIN_PWRPRESS (Power pin)
 *
 * These devices are low-power enabled so they will enter low-power mode
 * just after reading the sensor values and transmitting them over the
 * SWAP network.
 * Edit sensor.h for your sensor settings (type of sensor and pins)
 *
 * Associated Device Definition Files, defining registers, endpoints and
 * configuration parameters:
 * temp.xml (Temperature sensor)
 * temphum.xml (Dual Humidity + Temperature sensor)
 * temppress.xml (Dual Pressure + Temperature sensor)
 */
#include <Arduino.h> 
#include "regtable.h"
#include "panstamp.h"

#include <EEPROM.h>
#include "product.h"
#include "sensor.h"


/**
 * Uncomment if you are reading Vcc from A7. All battery-boards do this
 */
//#define VOLT_SUPPLY_A7

/**
 * LED pin
 */
#define LEDPIN               4

void setup();
void loop();

void initSensor(void);
const void updtVoltSupply(byte rId);
const void updtSensor(byte rId);

int sensor_ReadByte(void);
int sensor_ReadTempHum(void);
int sensor_ReadTemp(void);
int sensor_ReadTempPress(void);
void initSensor(void);

/**
 * setup
 *
 * Arduino setup function
 */
void setup()
{
	int i;
  Serial.println("Starting up");

	pinMode(LEDPIN, OUTPUT);
	digitalWrite(LEDPIN, LOW);

	// Initialize sensor pins
	initSensor();

	// Init panStamp
	panstamp.init();
	panstamp.cc1101.setCarrierFreq(CFREQ_433);

	// Transmit product code
	getRegister(REGI_PRODUCTCODE)->getData();

	// Enter SYNC state
	panstamp.enterSystemState(SYSTATE_SYNC);

	// During 3 seconds, listen the network for possible commands whilst the LED blinks
	for(i=0 ; i<6 ; i++)
	{
		digitalWrite(LEDPIN, HIGH);
		delay(100);
		digitalWrite(LEDPIN, LOW);
		delay(400);
	}

	// Transmit periodic Tx interval
	getRegister(REGI_TXINTERVAL)->getData();
	// Transmit power voltage
	getRegister(REGI_VOLTSUPPLY)->getData();
	// Switch to Rx OFF state
	panstamp.enterSystemState(SYSTATE_RXOFF);
}

/**
 * loop
 *
 * Arduino main loop
 */
void loop()
{
	//  digitalWrite(LEDPIN, HIGH);
	// Transmit sensor data
	getRegister(REGI_SENSOR)->getData();
	// Transmit power voltage
	getRegister(REGI_VOLTSUPPLY)->getData();
	//  digitalWrite(LEDPIN, LOW);

	// Sleep
	panstamp.goToSleep();
}




#ifdef TEMPHUM
#ifdef DHT11
#include "dht11.h"
#endif
#elif TEMPPRESS
#include "Wire.h"
#include "Adafruit_BMP085.h"
Adafruit_BMP085 bmp;
#endif

/**
 * Local functions
 */
#ifdef TEMPHUM

/**
 * sensor_ReadByte
 *
 * Read data byte from DHT11 sensor
 */
int sensor_ReadByte(void)
{
	byte i, result = 0;
	unsigned int count = 20000;

	for(i=0; i< 8; i++)
	{
		while(!getDataPin())
		{
			if (--count == 0)
				return -1;
		}
		delayMicroseconds(30);

		if (getDataPin())
			result |=(1<<(7-i));

		count = 20000;

		while(getDataPin())
		{
			if (--count == 0)
				return -1;
		}
	}
	return result;
}

/**
 * sensor_ReadTempHum
 *
 * Read temperature and humidity values from DHT11 sensor
 *
 * Return -1 in case of error. Return 0 otherwise
 */
int sensor_ReadTempHum(void)
{
	int temperature, humidity, chk;

#ifdef DHT11

	dht11 sensor;

	dhtSensorON();
	delay(1500);   
	chk = sensor.read(PIN_DHT_DATA);
	dhtSensorOFF();

	if (chk != DHTLIB_OK)
		return -1;

	temperature = sensor.temperature * 10 + 500;
	humidity = sensor.humidity * 10;

#elif DHT22

	byte dhtData[5];
	byte i, dhtCrc;
	boolean success = false;

	// Power ON sensor
	dhtSensorON();
	delay(1500);

	setDataOutput();
	setDataPin();

	// Start condition
	clearDataPin();
	delay(18);
	setDataPin();
	delayMicroseconds(40);	
	setDataInput();
	delayMicroseconds(40);

	if (!getDataPin())
	{
		// Start condition met
		delayMicroseconds(80);	
		if (getDataPin())
		{
			// Start condition met
			delayMicroseconds(80);

			// Now ready for data reception 
			for (i=0; i<5; i++)
			{
				if ((chk = sensor_ReadByte()) < 0)
					return -1;

				dhtData[i] = (byte)chk;
			}
			success = true;
		}
	}

	setDataOutput();
	//setDataPin();
	clearDataPin();

	// Power OFF sensor
	dhtSensorOFF();

	if (!success)
		return -1;

	dhtCrc = dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3];

	// check check_sum
	if(dhtData[4]!= dhtCrc)
		return -1;  // CRC error

	// Prepare values for 2-decimal format:
	int sign = 1;
	if (dhtData[2] & 0x80)
	{
		sign = -1;
		dhtData[2] &= 0x7F; 
	}
	temperature = sign * word(dhtData[2], dhtData[3]) + 500;  // 50.0 ºC offset in order to accept negative temperatures
	humidity = word(dhtData[0], dhtData[1]);

#endif

	dtSensor[0] = (temperature >> 8) & 0xFF;
	dtSensor[1] = temperature & 0xFF;
	dtSensor[2] = (humidity >> 8) & 0xFF;
	dtSensor[3] = humidity & 0xFF;

	return 0;
}

#elif TEMP
/**
 * sensor_ReadTemp
 *
 * Read temperature from TMP36 sensor
 *
 * Return -1 in case of error. Return 0 otherwise
 */
int sensor_ReadTemp(void)
{ 
	// Switch on sensor
	tempSensorON();
	delay(200);

	// Read voltage from ADC pin
	unsigned int reading = analogRead(PIN_ADCTEMP);  

	// Switch off sensor
	tempSensorOFF();

	// Convert reading to voltage (mV)
	float fVolt = (reading * voltageSupply) / 1024.0;
	unsigned int voltage = fVolt;

	// Fill register
	dtSensor[0] = (voltage >> 8) & 0xFF;
	dtSensor[1] = voltage & 0xFF;

	return 0;
}

#elif TEMPPRESS
/**
 * sensor_ReadTempPress
 *
 * Read temperature and pressure from BMP085 sensor
 *
 * Return -1 in case of error. Return 0 otherwise
 */
int sensor_ReadTempPress(void)
{
	delay(400);
	unsigned int temperature = bmp.readTemperature() * 10 + 500;
	unsigned long pressure = bmp.readPressure(); // Pa

	dtSensor[0] = (temperature >> 8) & 0xFF;
	dtSensor[1] = temperature & 0xFF;
	dtSensor[2] = (pressure >> 24) & 0xFF;
	dtSensor[3] = (pressure >> 16) & 0xFF;
	dtSensor[4] = (pressure >> 8) & 0xFF;
	dtSensor[5] = pressure & 0xFF;

	return 0;
}

#endif


/**
 * initSensor
 *
 * Initialize sensor pins
 */
void initSensor(void)
{
#ifdef TEMP
	pinMode(PIN_PWRTEMP, OUTPUT);   // Configure Power pin as output
	tempSensorOFF();
#elif TEMPHUM
	pinMode(PIN_PWRDHT, OUTPUT);    // Configure Power pin as output
	dhtSensorOFF();
#elif TEMPPRESS
	pinMode(PIN_PWRPRESS, OUTPUT);  // Configure Power pin as output
	pressSensorON();
	delay(200);
	bmp.begin();
#endif
}


/**
 * Declaration of common callback functions
 */
DECLARE_COMMON_CALLBACKS()

	/**
	 * Definition of common registers
	 */
DEFINE_COMMON_REGISTERS()

	/*
	 * Definition of custom registers
	 */
	// Voltage supply
	static unsigned long voltageSupply = 3300;
	static byte dtVoltSupply[2];
	REGISTER regVoltSupply(dtVoltSupply, sizeof(dtVoltSupply), &updtVoltSupply, NULL);
	// Sensor value register
	REGISTER regSensor(dtSensor, sizeof(dtSensor), &updtSensor, NULL);

	/**
	 * Initialize table of registers
	 */
DECLARE_REGISTERS_START()
	&regVoltSupply,
	&regSensor
DECLARE_REGISTERS_END()

	/**
	 * Definition of common getter/setter callback functions
	 */
DEFINE_COMMON_CALLBACKS()

	/**
	 * Definition of custom getter/setter callback functions
	 */

	/**
	 * updtVoltSupply
	 *
	 * Measure voltage supply and update register
	 *
	 * 'rId'  Register ID
	 */
const void updtVoltSupply(byte rId)
{  
	unsigned long result;

	// Read 1.1V reference against AVcc
	ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	delay(2); // Wait for Vref to settle
	ADCSRA |= _BV(ADSC); // Convert
	while (bit_is_set(ADCSRA,ADSC));
	result = ADCL;
	result |= ADCH << 8;
	result = 1126400L / result; // Back-calculate AVcc in mV
	voltageSupply = result;     // Update global variable Vcc

#ifdef VOLT_SUPPLY_A7

	// Read voltage supply from A7
	unsigned short ref = voltageSupply;
	result = analogRead(7);
	result *= ref;
	result /= 1024;
#endif

	/**
	 * register[eId]->member can be replaced by regVoltSupply in this case since
	 * no other register is going to use "updtVoltSupply" as "updater" function
	 */

	// Update register value
	regTable[rId]->value[0] = (result >> 8) & 0xFF;
	regTable[rId]->value[1] = result & 0xFF;
}

/**
 * updtSensor
 *
 * Measure sensor data and update register
 *
 * 'rId'  Register ID
 */
const void updtSensor(byte rId)
{
#ifdef TEMPHUM
	// Read temperature and humidity from sensor
	sensor_ReadTempHum();
#elif TEMP
	// Read temperature from sensor
	sensor_ReadTemp();
#elif TEMPPRESS
	// Read temperature and pressure from sensor
	sensor_ReadTempPress();
#endif
}
