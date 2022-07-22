#include "connection.h"

connection::connection(uint32_t IP, uint16_t port, BYTE* sessionkey, uint32_t initial_cuid) {

	memset(&remoteAddr, 0, sizeof(remoteAddr));
	remoteAddr.sin_addr.S_un.S_addr = IP;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = port;

	memcpy(sessionKey, sessionkey, ALTUS_SESSION_KEY_SIZE);
	memset(sessionkey, 0, ALTUS_SESSION_KEY_SIZE); //one key for one connection.
}

connection::~connection() {
	memset(&remoteAddr, 0, sizeof(remoteAddr));
	memset(sessionKey, 0, ALTUS_SESSION_KEY_SIZE);

	for (const auto& m : ch) {
		delete m.second;
	}
}

uint32_t connection::IP() {
	return remoteAddr.sin_addr.S_un.S_addr;
}

uint16_t connection::Port() {
	return remoteAddr.sin_port;
}
