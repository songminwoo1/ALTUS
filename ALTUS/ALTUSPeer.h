#pragma once

#include "channel.h"


#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)
#define STATUS_UNSUCCESSFUL         ((NTSTATUS)0xC0000001L)

#define ALTUS_SESSION_KEY_SIZE 32 //for AES-256

#define ALTUS_RSA_KEY_SIZE 700 //require over - 4096 key.
#define ALTUS_HANDSHAKE_WAITTIME 5000

class ALTUS_CPrePeer { //client-side
public:
	bool Expired;
	sockaddr_in remoteAddr;
	BYTE* publicKey; //server's pubkey.
	uint16_t keySize;
	BYTE* sessionKey; //received session key.
	uint32_t starttime;

	explicit ALTUS_CPrePeer(uint32_t ip, uint16_t port, BYTE* serverkey, int keylen);
	~ALTUS_CPrePeer();
};

class ALTUS_SPrePeer { //server-side
public:
	explicit ALTUS_SPrePeer(uint32_t _starttime);
	~ALTUS_SPrePeer();
	BYTE* publicKey;
	uint16_t keySize;
	BYTE* sessionKey;
	uint32_t starttime;
	bool A;
	bool B;
	bool C;
	bool D;
	bool IsAckSent;
};

class ALTUSPeer
{
private:
	SOCKET localsocket;
	std::map<uint16_t, channel*> ch; //cuid(channel uid) to channel map.
	uint16_t lastcuid;
	WQ_Node_Pool* pool;
public:
	sockaddr_in remoteAddr;
	int packetRate; //packets per second
	BYTE* publicKey; //peer's key
	int keySize;
	BYTE* sessionKey; //AES-256

	explicit ALTUSPeer(
		ALTUS_SPrePeer* prepeer,
		uint32_t IP, uint16_t port, SOCKET _localsocket, WQ_Node_Pool* _pool);
	explicit ALTUSPeer(uint64_t addrport, ALTUS_CPrePeer* prepeer, SOCKET _localsocket, WQ_Node_Pool* _pool);
	~ALTUSPeer();

	int newChannel(bool IsReceiving);
	channel* getChannelByID(uint16_t cuid);
	int processPacket(char* buf, int recvSize);

	int CreateUpstream(uint16_t cuid, char* buf, int len);
	//TODO
	//int CreateUpstream(uint16_t cuid, std::iostream stream, int len);

	//int Wait(uint16_t cuid); //wait for stream channel upload.
	int Send();
};
