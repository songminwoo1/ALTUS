#pragma once
#include <map>
#include <WinSock2.h>
#include <Windows.h>
#include <bcrypt.h>
#include <chrono>
#include <iostream>
#include <atomic>
#include "CallbackFunctionTypedef.h"

extern "C"
{
#include "windowedqueue.h"
}

//rdt_stream flag.
#define ALTUS_FLAG_ACK 0b00000001 //this asks for immediate acknowledgement.
#define ALTUS_FLAG_FIN 0b00000010 //this asks for delayed 3-way handshake, in order to intentionally cause HOL blocking.
//**important! using ACK with FIN leads to unpredicted behavior.
#define ALTUS_FLAG_RET 0b00000100 //for retransmitted packet.(for detecting seq overflow-turnaround.)
#define ALTUS_FLAG_TRM 0b00001000 //FIN handshake termination.

//packet - related.
#define ALTUS_PACKET_SIZE 496 // 16(AES block size)*31.
#define ALTUS_RETRY_SIZE 62464 //(496-8)*128.

#define ALTUS_RDT_HEADER_SIZE 8
#define ALTUS_RDT_DATA_SIZE 488 //496-(header size)

enum class channelType {
	invalid = 0,
	shared_struct, //this does not ensure sequence. only final state matters. can be larger than packet size.
	rdt_stream, //mostly for file transfer.
	lossy_stream, //mostly for audio.
	//TODO: 다시 생각하기.
	rdt_event, //reliable, atomic event.
	lossy_event, //sequence-unreliable, but atomic event.
	array_reserved //high-performance array transmission.(rdt)
};

class channel {
public:
	channelType type;
	bool _IsReceiving;
	WQ_Node_Pool* pool;

	std::atomic<unsigned int> NextSendSeq;
	unsigned int lastAckedSeq;
	unsigned short lastLostLen;
	//std::map<uint16_t, EventCallBack> events; //required?
	
	bool _IsUpdated;
	union {
		WQ_Window* buffer; //for rdt.
		void* storage; //for shared struct.
	};
	uint8_t storage_size;

	channel(WQ_Node_Pool* _pool, bool IsReceiver);
	~channel();

	//channelType Type();
	bool IsReceiving();
	bool IsUpdated();

	int LinkStruct(void* target, uint8_t length);
	WQ_Window* LinkRDTWindow();
	WQ_Window* LinkLossyWindow();
	//int LinkRDTEvent(EventCallBack event);
	//int LinkLossyEvent(EventCallBack event);

	//handle incoming packet.
	int receive(char* buf, int packetSize);
};