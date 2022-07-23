#include "channel.h"
#include <stdio.h>
#include <stdlib.h>


channel::channel(WQ_Node_Pool* _pool, bool IsReceiver) {
	pool = _pool;
	_IsReceiving = IsReceiver;
	type = channelType::invalid;

	NextSendSeq = 0;
	lastLostLen = 0;
	_IsUpdated = false;
	buffer = nullptr;
	storage = nullptr;
	storage_size = 0;
}

channel::~channel() {
	switch (type)
	{
	case channelType::invalid:
	case channelType::shared_struct:
		break;
	case channelType::rdt_stream:
	case channelType::lossy_stream:
	case channelType::rdt_event:
	case channelType::lossy_event:
		altusFreeWQ(buffer);
		break;
	default:
		fprintf(stderr, "NonExistingChannelType(not invalid):heap corruption detected.\n");
		exit(-1);
		break;
	}
	return;
}

//channelType channel::Type() {
//	return type;
//}

bool channel::IsReceiving() {
	return _IsReceiving;
}

bool channel::IsUpdated() {
	return _IsUpdated;
}

int channel::LinkStruct(void* target, uint8_t length) {
	if (type != channelType::invalid) return -1;
	storage_size = length;
	storage = target;
	type = channelType::shared_struct;
	return 0;
}

WQ_Window* channel::LinkRDTWindow() {
	buffer = altusNewWindow();
	buffer->lastSeq = 0;
	buffer->headSeq = 0;
	type = channelType::rdt_stream;
	return buffer;
}

WQ_Window* channel::LinkLossyWindow() {
	buffer = altusNewWindow();
	buffer->lastSeq = 0;
	buffer->headSeq = 0; //this will be unused, though.
	type = channelType::lossy_stream;
	return buffer;
}

int channel::receive(char* buf, int packetSize) {
	if (_IsReceiving) {
		//cation: dataSize may include padding(sub 16 bytes).
		switch (type)
		{
		case channelType::invalid:
			return -1;
		case channelType::shared_struct:
			if (packetSize < storage_size) return -1;
			memcpy(storage, buf, storage_size);
			return 0;
		case channelType::rdt_stream:
		{
			uint8_t paddingLen = buf[2];
			uint8_t flags = buf[3];
			unsigned int currentSeq;
			memcpy(&currentSeq, buf + 4, 4);
			currentSeq = ntohl(currentSeq);
			altusWQPut(buffer, pool, currentSeq, packetSize - ALTUS_RDT_HEADER_SIZE - paddingLen, buf + ALTUS_RDT_HEADER_SIZE);
			if (flags | ALTUS_FLAG_ACK) return 1; //1 means require sending Ack.
			return 0;
		}
		case channelType::lossy_stream:
		{
			//TODO: replace this quick and dirty implementation.
			altusWQPut(buffer, pool, buffer->lastSeq, packetSize >> 4, buf);
			return 0;
		}
		case channelType::rdt_event:
		{
			return -2;
		}
		case channelType::lossy_event:
			return -2;
		case channelType::array_reserved:
			return -2;
		default:
			fprintf(stderr, "NonExistingChannelType(not invalid):heap corruption detected.\n");
			exit(-1);
			break;
		}
	}
	else {
		switch (type)
		{
		case channelType::invalid:
			break;
		case channelType::shared_struct:
			break;
		case channelType::rdt_stream:
		{
			//Acknowledgement packet received.
			//need to handle loss.
			unsigned short LostLen;
			memcpy(&LostLen, buf + 2, 2);
			LostLen = ntohs(LostLen);
			unsigned int Seq;
			memcpy(&Seq, buf + 4, 4);
			Seq = ntohl(Seq);

			int deltaSeq = (int)(Seq - lastAckedSeq);
			if (deltaSeq < 0 || (int)(NextSendSeq - Seq) < 0) return 0; //delayed ack.(ignore)

			if (LostLen != 0) {
				printf("TODO: loss handling");
			}
			else {
				altusWQPop(buffer, pool, deltaSeq, NULL);
			}

			break;
		}
		case channelType::lossy_stream:
			break;
		case channelType::rdt_event:
			break;
		case channelType::lossy_event:
			break;
		case channelType::array_reserved:
			break;
		default:
			break;
		}
	}
	return -1;
}