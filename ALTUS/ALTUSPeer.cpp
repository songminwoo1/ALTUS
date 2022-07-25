#include "ALTUSPeer.h"

ALTUS_CPrePeer::ALTUS_CPrePeer(uint32_t ip, uint16_t port, BYTE* serverkey, int keylen) {
	Expired = false;
	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_addr.S_un.S_addr = ip;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = port;
	publicKey = (BYTE*)malloc(keylen);
	keySize = keylen;

	sessionKey = (BYTE*)malloc(ALTUS_SESSION_KEY_SIZE);
	if (publicKey == NULL || sessionKey == NULL) {
		throw std::bad_alloc();
	}
	memcpy(publicKey, serverkey, keylen);

	{
		using namespace std::chrono;
		starttime = static_cast<uint32_t>(duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count());
	}
}

ALTUS_CPrePeer::~ALTUS_CPrePeer() {
	if (publicKey != nullptr) {
		memset(publicKey, 0, keySize);
		free(publicKey);
	}
	if (sessionKey != nullptr) {
		memset(sessionKey, 0, ALTUS_SESSION_KEY_SIZE);
		free(sessionKey);
	}
}

ALTUS_SPrePeer::ALTUS_SPrePeer(uint32_t _starttime) {
	publicKey = (BYTE*)malloc(ALTUS_RSA_KEY_SIZE);
	sessionKey = (BYTE*)malloc(ALTUS_SESSION_KEY_SIZE);
	if (publicKey == NULL || sessionKey == NULL) {
		throw std::bad_alloc();
	}
	keySize = 0;
	starttime = _starttime;
	A = false;
	B = false;
	C = false;
	D = false;
	IsAckSent = false;
}

ALTUS_SPrePeer::~ALTUS_SPrePeer() {
	if (publicKey != nullptr) {
		memset(publicKey, 0, keySize);
		free(publicKey);
	}
	if (sessionKey != nullptr) {
		memset(sessionKey, 0, ALTUS_SESSION_KEY_SIZE);
		free(sessionKey);
	}
}

ALTUSPeer::ALTUSPeer( //server-side
		ALTUS_SPrePeer* prepeer,
		uint32_t IP, uint16_t port, SOCKET _localsocket, WQ_Node_Pool* _pool, NodePool* _newPool)
	{
	//printf("server peer\n");

	localsocket = _localsocket;
	lastcuid = 1;
	pool = _pool; //deprecated
	newPool = _newPool;

	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_addr.S_un.S_addr = IP;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = port;
	packetRate = 0;
	publicKey = prepeer->publicKey;
	keySize = prepeer->keySize;
	sessionKey = prepeer->sessionKey;

	prepeer->publicKey = nullptr;	//prevent freeing at prepeer deconstructor;
	prepeer->sessionKey = nullptr;	//prevent freeing at prepeer deconstructor;

	//deprecated
	channel* receivingCH = new channel(_newPool, _pool, true);
	receivingCH->LinkRDTWindow();
	channel* sendingCH = new channel(_newPool, _pool, false);
	sendingCH->LinkRDTWindow();
	sendingCH->_IsReceiving = false;

	channel* fileSenderCH = new channel(_newPool, _pool, false);
	fileSenderCH->LinkRDTWindow();
	fileSenderCH->_IsReceiving = false;

	ch.insert(std::pair<uint32_t, channel*>(0, receivingCH)); //receiving
	ch.insert(std::pair<uint32_t, channel*>(1, sendingCH)); //sending
	ch.insert(std::pair<uint32_t, channel*>(2, 
		new channel(newPool, pool, channelType::file_stream_rcv, nullptr, 0)));
	ch.insert(std::pair<uint32_t, channel*>(3,
		fileSenderCH));
}

//client-side.
ALTUSPeer::ALTUSPeer(uint64_t addrport, ALTUS_CPrePeer* prepeer, SOCKET _localsocket, WQ_Node_Pool* _pool, NodePool* _newPool) {
	//printf("client peer\n");
	uint32_t ip = addrport >> 32;
	uint16_t port = addrport & 0xFFFF;

	localsocket = _localsocket;
	lastcuid = 1;
	pool = _pool; //deprecated
	newPool = _newPool;

	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_addr.S_un.S_addr = ip;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = port;
	packetRate = 0;
	publicKey = prepeer->publicKey;
	keySize = prepeer->keySize;
	sessionKey = prepeer->sessionKey;

	prepeer->publicKey = nullptr;	//prevent freeing at prepeer deconstructor;
	prepeer->sessionKey = nullptr;	//prevent freeing at prepeer deconstructor;

	channel* sendingCH = new channel(_newPool, _pool, false);
	sendingCH->LinkRDTWindow();
	sendingCH->_IsReceiving = false;
	channel* receivingCH = new channel(_newPool, _pool, true);
	receivingCH->LinkRDTWindow();

	channel* fileSenderCH = new channel(_newPool, _pool, false);
	fileSenderCH->LinkRDTWindow();
	fileSenderCH->_IsReceiving = false;

	ch.insert(std::pair<uint32_t, channel*>(0, sendingCH)); //sending
	ch.insert(std::pair<uint32_t, channel*>(1, receivingCH)); //receiving
	ch.insert(std::pair<uint32_t, channel*>(2,
		fileSenderCH));
	ch.insert(std::pair<uint32_t, channel*>(3,
		new channel(newPool, pool, channelType::file_stream_rcv, nullptr, 0)));
}

ALTUSPeer::~ALTUSPeer() {
	if (publicKey != nullptr) {
		memset(publicKey, 0, keySize);
		free(publicKey);
	}
	if (sessionKey != nullptr) {
		memset(sessionKey, 0, ALTUS_SESSION_KEY_SIZE);
		free(sessionKey);
	}

	for (std::pair< uint16_t, channel*> chi : ch) {
		delete chi.second;
	}
}

int ALTUSPeer::newChannel(bool IsReceiving) {
	uint16_t newcuid = ++lastcuid;
	while (ch.find(newcuid) != ch.end()) {
		newcuid++;
		if (newcuid == lastcuid) return -1;
	}
	try {
		ch.insert(std::pair<uint16_t, channel*>(newcuid, new channel(newPool, pool, IsReceiving)));
	}
	catch(std::invalid_argument e) {
		std::cerr << e.what();
		return -1;
	}
	return newcuid;
}

channel* ALTUSPeer::getChannelByID(uint16_t cuid) {
	auto searchRes = ch.find(cuid);
	if (searchRes == ch.end()) return nullptr;
	return searchRes->second;
}

int ALTUSPeer::processPacket(char* buf, int recvSize) {
	//TODO: decrypt packet with session key.

	uint16_t _cuid;
	memcpy(&_cuid, buf, 2);
	uint16_t cuid = ntohs(_cuid);
	auto searchRes = ch.find(cuid);
	if (searchRes == ch.end()) return -1;

	auto thisChannel = searchRes->second;
	switch (thisChannel->receive(buf, recvSize)) {
	case 0:
		break;
	case 1: //rdt_stream need to send back acknowledgement.
	{
		union
		{
			uint32_t u[2];
			struct {
				uint16_t cuid;
				uint16_t lostLen;
				uint32_t LostSeq;
			};
		} response;
		response.cuid = _cuid; //origin channel.
		if (thisChannel->buffer->ub_list.count > 0) {
			//when there is unsequenced block
			uint32_t unseq_size = thisChannel->buffer->ub_list.headSeq[0] - thisChannel->buffer->lastSeq;
			if (unseq_size > ALTUS_RETRY_SIZE) {
				response.lostLen = htons(ALTUS_RETRY_SIZE);
			}
			response.lostLen = htons(unseq_size);
		}
		else {
			response.lostLen = 0;
		}
		response.LostSeq = htonl(thisChannel->buffer->lastSeq);

		//TODO: aes-128 encryption.
		sendto(localsocket, (char*)&response, sizeof(response), 0, (SOCKADDR*)&remoteAddr, sizeof(remoteAddr));
	}
	default:
		return -1;
	}
	return 0;
}

int ALTUSPeer::CreateUpstream(uint16_t cuid, char* buf, int len) {
	auto searchRes = ch.find(cuid);
	if (searchRes == ch.end()) return -1;
	channel* thisChannel = searchRes->second;
	if (thisChannel->type != channelType::rdt_stream || thisChannel->IsReceiving()) return -2;

	int retval = altusWQPut(thisChannel->buffer, pool, thisChannel->buffer->lastSeq, len, buf);
	thisChannel->_IsUpdated = true;
	printf("upstream created\n");
	return retval;
}

int ALTUSPeer::Send() {
	for (std::pair<uint16_t, channel*> c : ch) {
		auto cd = c.second;
		for (int i = 0; i < 16 && cd->_IsUpdated; i++) {
			if (!cd->_IsReceiving) {
				switch (cd->type) {
					case channelType::invalid:
						break;
					case channelType::shared_struct:
					{
						sendto(localsocket, (const char*)cd->storage, cd->storage_size, 0, (SOCKADDR*)&remoteAddr, sizeof(remoteAddr));
						break;
					}
					case channelType::rdt_stream:
					{
						unsigned int nextSeq = cd->NextSendSeq;
						//int count = 0; //packet sending count per Send() call.
						//TODO: flow control.
						int buf[ALTUS_PACKET_SIZE/4];
						((short*)buf)[0] = ntohs(c.first);
						char* pos;
						unsigned int availableLen = altusWQPeek(cd->buffer, nextSeq, &pos);
						if (availableLen >= ALTUS_RDT_DATA_SIZE) {
							((char*)buf)[2] = 0;
							((char*)buf)[3] = ALTUS_FLAG_ACK;
							buf[1] = ntohl(nextSeq);
							memcpy((char*)buf + ALTUS_RDT_HEADER_SIZE, pos, ALTUS_RDT_DATA_SIZE);
							std::atomic_compare_exchange_strong(&(cd->NextSendSeq), &nextSeq, nextSeq + ALTUS_RDT_DATA_SIZE);
							//TODO: encrypt.
							sendto(localsocket, (char*)buf, ALTUS_PACKET_SIZE, 0, (SOCKADDR*)&remoteAddr, sizeof(remoteAddr));
						}
						else {
							char* pos2;
							unsigned int sendingLen = availableLen;
							unsigned int availLen2 = altusWQPeek(cd->buffer, nextSeq + availableLen, &pos2);
							sendingLen += availLen2;
							if (sendingLen > ALTUS_RDT_DATA_SIZE) sendingLen = ALTUS_RDT_DATA_SIZE;
							
							uint8_t paddingLen = (ALTUS_PACKET_SIZE - sendingLen - ALTUS_RDT_HEADER_SIZE) % 16;
							((char*)buf)[2] = paddingLen;
							((char*)buf)[3] = ALTUS_FLAG_ACK;
							buf[1] = ntohl(nextSeq);
							memcpy((char*)buf + ALTUS_RDT_HEADER_SIZE, pos, availableLen);
							if (pos2 != NULL) {
								memcpy((char*)buf + ALTUS_RDT_HEADER_SIZE + availableLen, pos2, sendingLen - availableLen);
							}
							else {
								cd->_IsUpdated = false;
							}
							std::atomic_compare_exchange_strong(&(cd->NextSendSeq), &nextSeq, nextSeq + ALTUS_RDT_DATA_SIZE);
							//TODO: encrypt(maybe with padding).
							sendto(localsocket, (char*)buf, ALTUS_RDT_HEADER_SIZE + sendingLen + paddingLen, 0, (SOCKADDR*)&remoteAddr, sizeof(remoteAddr));
						}
						break;
					}
					case channelType::lossy_stream:
					default:
						return -2;
				}
			}
		}
	}
	return 0;
}