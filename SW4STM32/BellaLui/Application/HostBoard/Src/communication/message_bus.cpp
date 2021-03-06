/*
 * message_bus.cpp
 *
 *  Created on: 10 Dec 2019
 *      Author: Arion
 */

/*
 * Usage of c++ has been discussed and backed during meeting of 12/12/19
 * Dynamic allocation should be avoided. If needed, use pvPortMalloc and pvPortFree instead of new and delete respectively.
 */

#include <communication/message_bus.h>


/*
 * Initialises the protocol when the bus is created.
 *
 * Requests the implementation to register (once)
 * all message IDs and handlers before any I/O transmission
 */
MessageBus::MessageBus() {
	lock = false;
	init_protocol();
	lock = true;
}


/*
 * Registers a message identifier for the specified structure/class.
 *
 * Allows I/O communication for the given message ID according to the bus specification.
 * Using std::typeindex allows more flexibility insofar as the message ID is directly inferred
 * using the template of send_packet.
 *
 * Warning: this method is not thread-safe.
 */
template<typename T> void MessageBus::register_object(uint8_t identifier) {
	size_t size = sizeof(T);

	if(size > 256) {
		return; // Impossible to register a packet whose length is greater than 256
	}

	size_t type_hash = typeid(T).hash_code();

	identifiers.insert(identifiers.begin(), { type_hash, identifier });

	safe_cast_reference[identifier] = (uint8_t) size;
}




/*
 * Registers a handler for this event bus.
 *
 * Accepts a function reference as message handler.
 *
 * Warning: this method is not thread-safe.
 */
template<typename T> void MessageBus::register_handler(void (*handler)(T*)) {
	size_t type_hash = typeid(T).hash_code();

	auto it = identifiers.find(type_hash);

	if(it != identifiers.end()) {
		handlers.insert(handlers.begin(), { it->second, (void (*)(void*)) handler });
	} else {
		// Error: attempt to register a handler for a non-registered packet
	}
}




/*
 * Sends the given message using the implemented transmission protocol.
 */
template<typename T> void MessageBus::send(T *message) {
	size_t type_hash = typeid(T).hash_code();

	auto iterator = identifiers.find(type_hash);

		if(iterator != identifiers.end()) {
			write(&iterator->second, 1);
			write((uint8_t*) message, sizeof(T));
		} else {
			// Error: invalid packet ID
		}
}




/*
 * Handles the reception of a message.
 *
 * Provided an external thread calls this method with a buffer to the next incoming message,
 * dispatches the message to the appropriate message handlers.
 */
void MessageBus::receive(uint8_t *pointer, uint8_t length) {
	if(length > 0) {
		// Safe-cast verification
		uint8_t packet_id = *pointer++;

		if(length - 1 != safe_cast_reference[packet_id]) {
			return; // Error
		}

		for(iunordered_multimap<uint8_t, void (*)(void *)>::iterator it = handlers.find(packet_id); it != handlers.end(); it++) {
			it->second(pointer);
		}
	}
}






/*

Usage example:

struct PacketIMU {
	double ax;
	double ay;
	double az;
	double vx;
	double vy;
	double vz;
} __packed;


void handle_IMU1(PacketIMU* packet) {
	std::cout << "handle_IMU1" << std::endl;
}

void handle_IMU2(PacketIMU* packet) {
	std::cout << "handle_IMU2" << std::endl;
}

int main0() {

	register_packet<PacketIMU>(5); // IMU packet has id 5

	register_handler(handle_IMU1);
	register_handler(handle_IMU2);

	PacketIMU imu = { 0.0, 1.2, 0.3, 22.0, 43.0, 100.0 };

	send_packet(&imu);



	return 0;
}*/
