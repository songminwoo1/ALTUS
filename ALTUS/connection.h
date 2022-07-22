#pragma once
#include <map>
#include <WinSock2.h>
#include <Windows.h>
#include <bcrypt.h>

#include "channel.h"

#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)
#define STATUS_UNSUCCESSFUL         ((NTSTATUS)0xC0000001L)

#define ALTUS_SESSION_KEY_SIZE 32 //for AES-256

class connection //a p2p connection between local host and remote host.
{
private:
	std::map<uint32_t, channel*> ch; //cuid(channel uid) to channel map.
	sockaddr_in remoteAddr;

	BYTE sessionKey[ALTUS_SESSION_KEY_SIZE]; //AES-256
public:
	connection(uint32_t IP, uint16_t port, BYTE* sessionkey, uint32_t initial_cuid); //ip and port in network byte order.
	~connection();

	uint32_t IP();
	uint16_t Port();

	/*int send();
	int sendN();*/
};
