/*
 * data_handling.c
 *
 *  Created on: 19 Apr 2018
 *      Author: Clement Nussbaumer
 *      Alexandre Devienne
 */

#include "../../../HostBoard/Inc/debug/led.h"
#include <misc/Common.h>
#include <misc/data_handling.h>
#include <misc/datagram_builder.h>
#include "cmsis_os.h"

#include <stdbool.h>
#include <telemetry/simpleCRC.h>
#include <telemetry/telemetry_protocol.h>

extern "C" {
	#include <CAN_communication.h>
	#include <storage/sd_card.h>
}


#define TELE_TIMEMIN 20
#define GPS_TIMEMIN 100
#define MOTOR_TIMEMIN 100
#define WARNING_TIMEMIN 50
#define AB_TIMEMIN 100
//#define TELE_RAW_TIMEMIN 100

volatile static uint32_t Packet_Number = 0;

// for import in C code
extern "C" bool telemetry_sendGPSData(GPS_data data);
extern "C" bool telemetry_sendIMUData(IMU_data data);
extern "C" bool telemetry_sendBaroData(BARO_data data);
extern "C" bool telemetry_sendWarningPacketData(bool id, float value, uint8_t av_state);
extern "C" bool telemetry_sendMotorPressureData(uint32_t pressure);
extern "C" bool telemetry_sendABData();

extern "C" bool telemetry_receiveOrderPacket(uint8_t* RX_Order_Packet);
extern "C" bool telemetry_receiveIgnitionPacket(uint8_t* RX_Ignition_Packet);

extern osMessageQId xBeeQueueHandle;

uint32_t telemetrySeqNumber = 0;
uint8_t current_state = STATE_IDLE;

IMU_data  imu  = {{0,0,0},{0,0,0}, 0};
BARO_data baro = {0,0,0};
uint32_t last_sensor_update = 0;
uint32_t last_motor_update = 0;
uint32_t last_warning_update = 0;
uint32_t last_airbrakes_update = 0;
//uint32_t last_sensor_raw_update = 0;
Telemetry_Message m1;
Telemetry_Message m2;
Telemetry_Message m3;
Telemetry_Message m4;
Telemetry_Message m5;
Telemetry_Message m6;
//Telemetry_Message m7;
//Telemetry_Message m8;

Telemetry_Message event;

//the createXXXDatagram-Methods create the datagrams as described in the Schema (should be correct)

Telemetry_Message createTelemetryDatagram (IMU_data* imu_data, BARO_data* baro_data, uint32_t time_stamp, uint32_t telemetrySeqNumber)
{

	//here, the Datagram is created
	DatagramBuilder builder = DatagramBuilder (SENSOR_DATAGRAM_PAYLOAD_SIZE, TELEMETRY_PACKET, telemetrySeqNumber);

	// ## Beginning of datagram Payload ##
	// time stamp
	builder.write32<uint32_t> (time_stamp);
	builder.write32<uint32_t> (Packet_Number++);
	builder.write32<float32_t> (imu_data->acceleration.x);
	builder.write32<float32_t> (imu_data->acceleration.y);
	builder.write32<float32_t> (imu_data->acceleration.z);

	builder.write32<float32_t> (imu_data->eulerAngles.x);
	builder.write32<float32_t> (imu_data->eulerAngles.y);
	builder.write32<float32_t> (imu_data->eulerAngles.z);

	builder.write32<float32_t> (baro_data->temperature);
	builder.write32<float32_t> (baro_data->pressure);

	builder.write32<float32_t> (can_getSpeed()); // pitot_press
	//baro_data->temperature = 20;


	builder.write32<float32_t> (can_getAltitude());

	return builder.finalizeDatagram ();
}

Telemetry_Message createAirbrakesDatagram (uint32_t time_stamp, uint32_t telemetrySeqNumber)
{
	DatagramBuilder builder = DatagramBuilder (AB_DATAGRAM_PAYLOAD_SIZE, AIRBRAKES_PACKET, telemetrySeqNumber++);
	builder.write32<uint32_t> (time_stamp);
	builder.write32<uint32_t> (Packet_Number++);
	builder.write32<float32_t> (can_getABangle()); // AB_angle

	return builder.finalizeDatagram ();
}

//same structure for the other createXXXDatagrams
Telemetry_Message createGPSDatagram (uint32_t seqNumber, GPS_data gpsData)
{
	DatagramBuilder builder = DatagramBuilder (GPS_DATAGRAM_PAYLOAD_SIZE, GPS_PACKET, seqNumber++);

	builder.write32<uint32_t> (HAL_GetTick ());
	builder.write32<uint32_t> (Packet_Number++);
	builder.write8 (gpsData.sats);
	builder.write32<float32_t> (gpsData.hdop);
	builder.write32<float32_t> (gpsData.lat);
	builder.write32<float32_t> (gpsData.lon);
	builder.write32<int32_t> (gpsData.altitude);

	return builder.finalizeDatagram ();
}

Telemetry_Message createMotorPressurePacketDatagram(uint32_t time_stamp, float32_t pressure, uint32_t seqNumber)
{
	DatagramBuilder builder = DatagramBuilder (MOTORPRESSURE_DATAGRAM_PAYLOAD_SIZE, MOTOR_PACKET, seqNumber++);

	builder.write32<uint32_t> (time_stamp);
	builder.write32<uint32_t> (Packet_Number++);
	builder.write32<float32_t> (pressure);

	return builder.finalizeDatagram();
}
//new
Telemetry_Message createWarningPacketDatagram(uint32_t time_stamp, uint8_t id, float value, uint8_t av_state, uint32_t seqNumber)
{
	DatagramBuilder builder = DatagramBuilder (WARNING_DATAGRAM_PAYLOAD_SIZE, STATUS_PACKET, seqNumber++);

	builder.write32<uint32_t> (time_stamp);
	builder.write32<uint32_t> (Packet_Number++);
	builder.write8 (id);
	builder.write32 (value);
	builder.write8(av_state); // flight status

	return builder.finalizeDatagram();
}

/*
Telemetry_Message createOrderPacketDatagram(uint32_t time_stamp)
{
	DatagramBuilder builder = DatagramBuilder ();
}
*/

/*
Telemetry_Message createTelemetryRawDatagram(uint32_t time_stamp, float32_t euler, float32_t accelerometer, float32_t temp, float32_t pressure, uint32_t seqNumber)
{
	DatagramBuilder builder = DatagramBuilder (TELEMETRY_RAW_DATAGRAM_PAYLOAD_SIZE,time_stamp);

	builder.write32<float32_t> (euler);
	builder.write32<float32_t> (accelerometer);
	builder.write32<float32_t> (temp);
	builder.write32<float32_t> (pressure);

	return builder.finalizeDatagram();
}
*/


// New Packets
/*
	Send:
	telemetry-raw
	telemetry-filtered (after kalman)
	motorPressure
	eventState (FSM)
	warning Packet

	Receive:
	order Packet (fill tank, abort)
	ignition  (go)
*/

//New methods to implement :
//createOrderPackerDatagram
//createIgnitionDatagram


bool telemetry_sendGPSData(GPS_data data) {
	static uint32_t last_update = 0;
	uint32_t now = HAL_GetTick();
	bool handled = false;

	if (now - last_update > GPS_TIMEMIN) {
		m1 = createGPSDatagram (telemetrySeqNumber++, data);
		if (osMessagePut (xBeeQueueHandle, (uint32_t) &m1, 10) != osOK) {
			vPortFree(m1.ptr); // free the datagram if we couldn't queue it
		}
		last_update = now;
		handled = true;
	}
	return handled;
}

bool telemetry_sendIMUData(IMU_data data) {
	uint32_t now = HAL_GetTick();
	bool handled = false;

	imu = data;

	if (now - last_sensor_update > TELE_TIMEMIN) {
		m2 = createTelemetryDatagram (&imu, &baro, now, telemetrySeqNumber++);
		if (osMessagePut (xBeeQueueHandle, (uint32_t) &m2, 10) != osOK) {
			vPortFree(m2.ptr); // free the datagram if we couldn't queue it
		}
		last_sensor_update = now;
		handled = true;
	}
	return handled;
}

bool telemetry_sendBaroData(BARO_data data) {
	uint32_t now = HAL_GetTick();
	bool handled = false;

	baro = data;

	if (now - last_sensor_update > TELE_TIMEMIN) {
		m3 = createTelemetryDatagram (&imu, &baro, now, telemetrySeqNumber++);
		if (osMessagePut (xBeeQueueHandle, (uint32_t) &m3, 10) != osOK) {
			vPortFree(m3.ptr); // free the datagram if we couldn't queue it
		}
		last_sensor_update = now;
		handled = true;
	}
	return handled;
}

bool telemetry_sendMotorPressureData(uint32_t pressure)
{
	uint32_t now = HAL_GetTick();
	bool handled = false;

	if (now - last_motor_update > MOTOR_TIMEMIN) {
		m4 = createMotorPressurePacketDatagram (pressure, now, telemetrySeqNumber++);
		if (osMessagePut (xBeeQueueHandle, (uint32_t) &m4, 10) != osOK) {
			vPortFree(m4.ptr); // free the datagram if we couldn't queue it
		}
		last_motor_update = now;
		handled = true;
	}
	return handled;
}

bool telemetry_sendWarningPacketData(bool id, float value, uint8_t av_state)
{
	uint32_t now = HAL_GetTick();
	bool handled = false;

	if (now - last_warning_update > WARNING_TIMEMIN) {
		m5 = createWarningPacketDatagram (now, id, value, av_state, telemetrySeqNumber++);
		if (osMessagePut (xBeeQueueHandle, (uint32_t) &m5, 10) != osOK) {
			vPortFree(m5.ptr); // free the datagram if we couldn't queue it
		}
		last_warning_update = now;
		handled = true;
	}
	return handled;
}

bool telemetry_sendABData() {
	uint32_t now = HAL_GetTick();
	bool handled = false;

	if (now - last_airbrakes_update > AB_TIMEMIN) {
		m6 = createAirbrakesDatagram (now, telemetrySeqNumber++);
		if (osMessagePut (xBeeQueueHandle, (uint32_t) &m6, 10) != osOK) {
			vPortFree(m6.ptr); // free the datagram if we couldn't queue it
		}
		last_airbrakes_update = now;
		handled = true;
	}
	return handled;
}

// Received Packet Handling

bool telemetry_receiveOrderPacket(uint8_t* RX_Order_Packet) {

	uint32_t ts = RX_Order_Packet[3] | (RX_Order_Packet[2] << 8) | (RX_Order_Packet[1] << 16) | (RX_Order_Packet[0] << 24);
	uint32_t packet_nbr = RX_Order_Packet[7] | (RX_Order_Packet[6] << 8) | (RX_Order_Packet[5] << 16) | (RX_Order_Packet[4] << 24);
	switch (RX_Order_Packet[8])
	{
		case STATE_OPEN_FILL_VALVE:
		{
			current_state = STATE_OPEN_FILL_VALVE;
			break;
		}
		case STATE_CLOSE_FILL_VALVE:
		{
		current_state = STATE_OPEN_PURGE_VALVE;
			break;
		}
		case STATE_OPEN_FILL_VALVE:
		{
			current_state = STATE_OPEN_PURGE_VALVE;
			break;
		}
		case STATE_DISCONNECT_HOSE:
		{
			current_state = STATE_DISCONNECT_HOSE;
			break;
		}
	}
	can_setFrame((int32_t) current_state, DATA_ID_ORDER, ts);
	return 0;
}

bool telemetry_receiveIgnitionPacket(uint8_t* RX_Ignition_Packet) {
	uint32_t ts = RX_Ignition_Packet[3] | (RX_Ignition_Packet[2] << 8) | (RX_Ignition_Packet[1] << 16) | (RX_Ignition_Packet[0] << 24);
	uint32_t packet_nbr = RX_Ignition_Packet[7] | (RX_Ignition_Packet[6] << 8) | (RX_Ignition_Packet[5] << 16) | (RX_Ignition_Packet[4] << 24);
	if( RX_Ignition_Packet[8] == 0x22) {
		can_setFrame((int32_t) 0x22, DATA_ID_IGNITION, ts);
	}
	return 0;
}


