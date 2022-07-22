#pragma comment(lib, "ws2_32")
#include <WS2tcpip.h>
#include "ALTUSHost.h"
#include <iostream>

DWORD WINAPI Receiver(LPVOID lpParam);

bool Comparator::operator() (std::pair<uint32_t, ALTUS_CPrePeer*> a, std::pair<uint32_t, ALTUS_CPrePeer*> b) {
	int32_t res = a.first - b.first;
	if (res > 0) return true;
	return false;
}

ALTUSHost::ALTUSHost() {
	printf("newhost\n");
	myPublicKeySize = ALTUS_RSA_KEY_SIZE;
	myPrivateKeySize = ALTUS_RSA_KEY_SIZE;
	lastpeid = 0;
	InitializeCriticalSection(&peidCreation);
	InitializeCriticalSection(&poppingcprepeer);
	InitializeCriticalSection(&RTQEmptyCheck);

	acceptAction = NullFunc;
	connectAction = NullFunc;

	pool = altusWQNewNodePool();

	localsocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (localsocket == INVALID_SOCKET) {
		exit(-1);
	}

	// Set Timeout for recv call
	DWORD tv = 1000;
	setsockopt(localsocket, SOL_SOCKET, SO_RCVTIMEO,
		(const char*)&tv, sizeof(DWORD));

	ZeroMemory(&localaddr, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	localaddr.sin_port = htons(ALTUS_DEFAULT_PORT);

	if (bind(localsocket, (SOCKADDR*)&(localaddr),
		sizeof(localaddr)) == SOCKET_ERROR) {
		exit(-1);
	}

	_IsAlive = false;
	_IsAccepting = false;
}

ALTUSHost::ALTUSHost(uint32_t port) {
	printf("newhost\n");
	myPublicKeySize = ALTUS_RSA_KEY_SIZE;
	myPrivateKeySize = ALTUS_RSA_KEY_SIZE;
	lastpeid = 0;
	InitializeCriticalSection(&peidCreation);
	InitializeCriticalSection(&poppingcprepeer);
	InitializeCriticalSection(&RTQEmptyCheck);

	acceptAction = NullFunc;
	connectAction = NullFunc;

	pool = altusWQNewNodePool();

	localsocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (localsocket == INVALID_SOCKET) {
		exit(-1);
	}

	// Set Timeout for recv call
	DWORD tv = 1000;
	setsockopt(localsocket, SOL_SOCKET, SO_RCVTIMEO,
		(const char*)&tv, sizeof(DWORD));

	ZeroMemory(&localaddr, sizeof(SOCKADDR_IN));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	localaddr.sin_port = htons(port);

	if (bind(localsocket, (SOCKADDR*)&(localaddr),
		sizeof(localaddr)) == SOCKET_ERROR) {
		exit(-1);
	}

	_IsAlive = false;
	_IsAccepting = false;
}

int ALTUSHost::run() {
	_IsAlive = true;
	_IsAccepting = true;
	ListenerHandle = CreateThread(
		NULL,
		0,
		Receiver,
		this,
		0,
		&ListenerThreadID
	);
	if (ListenerHandle == NULL) {
		exit(-1);
	}
	CloseHandle(ListenerHandle);

	connecterThread = std::thread([&]() {
		this->_Connecter();
		});

	senderThread = std::thread([&]() {
		this->Sender();
		});

	return 0;
}

ALTUSHost::~ALTUSHost() {
	bool wasAlive = _IsAlive;
	_IsAlive = false;
	_IsAccepting = false;
	WaitForSingleObject(ListenerHandle, 300);
	closesocket(localsocket);

	for (std::pair<const uint64_t, ALTUSPeer*> peer : peers) {
		delete peer.second;
	}
	for (std::pair<uint64_t, ALTUS_SPrePeer*> peer : s_prepeers) {
		delete peer.second;
	}
	for (std::pair<uint64_t, ALTUS_CPrePeer*> peer : c_prepeers) {
		delete peer.second;
	}

	//connecterCond.notify_all();
	if (wasAlive) {
		connecterCond.notify_all();
		connecterThread.join();
	}
	senderThread.join();
	altusFreeP(pool);

	DeleteCriticalSection(&peidCreation);
	DeleteCriticalSection(&poppingcprepeer);
	DeleteCriticalSection(&RTQEmptyCheck);
}

ALTUSPeer* ALTUSHost::getPeerbyID(int peid) {
	auto loc = peid_peer.find(peid);
	if (loc == peid_peer.end()) {
		return nullptr;
	}

	return loc->second;
}

ALTUSPeer* ALTUSHost::getPeer(uint64_t addrport) {
	auto loc = peers.find(addrport);
	if (loc == peers.end()) {
		return nullptr;
	}

	return loc->second;
}

ALTUS_CPrePeer* ALTUSHost::findCPrePeer(uint64_t addrport) {
	auto loc = c_prepeers.find(addrport);
	if (loc == c_prepeers.end()) {
		return nullptr;
	}

	return loc->second;
}

int ALTUSHost::accepter(uint64_t addrport, char* buf, int size) {

	if (size < 12) {
		return -2;
	}

	//TODO:decrypt message with my private key.
	//message might contain padding. 
	//however, meaningfull length is fixed to ALTUS_PACKET_SIZE.

	if (strncmp("ALTUS/", buf, 6) != 0) { //wrong packet
		printf("wrong packet!\n");
		return -2;
	}
	if (strncmp("1.0", buf + 6, 3) != 0) { //version error
		return -1;
	}

	char page = *(buf + 9);

	uint32_t currentTime;
	{
		using namespace std::chrono;
		currentTime = static_cast<uint32_t>(duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count());
	}

	auto searchres = s_prepeers.find(addrport);
	if (searchres != s_prepeers.end() && (currentTime - searchres->second->starttime) > ALTUS_HANDSHAKE_WAITTIME) {
		printf("timed out request!\n");
		delete searchres->second;
		s_prepeers.erase(searchres);
		searchres = s_prepeers.end();
	}

	if (searchres == s_prepeers.end()) {
		auto newpeer = new ALTUS_SPrePeer(currentTime);
		//set key size
		{
			uint16_t keysize;
			memcpy(&keysize, buf + 10, 2);
			keysize = ntohs(keysize); //remember that keysize is in network byte order.

			if (keysize < 128) {
				delete newpeer;
				return -3;
			}
			newpeer->keySize = keysize;
		}
		switch (page)
		{
		case('A'):
			memcpy(newpeer->publicKey, buf + ALTUS_HANDSHAKE_HEADER_SIZE, size - 12);
			newpeer->A = true;
			if (newpeer->keySize == size - 12) { //one-page key.
				this->prepeerToPeer(addrport);
			}
			break;
		case('B'):
			memcpy(newpeer->publicKey + (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE), buf + ALTUS_HANDSHAKE_HEADER_SIZE, size - 12);
			newpeer->B = true;
			break;
		case('C'):
			memcpy(newpeer->publicKey + (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE) * 2, buf + ALTUS_HANDSHAKE_HEADER_SIZE, size - 12);
			newpeer->C = true;
			break;
		case('D'):
			memcpy(newpeer->publicKey + (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE) * 3, buf + ALTUS_HANDSHAKE_HEADER_SIZE, size - 12);
			newpeer->D = true;
			break;
		default:
			delete newpeer;
			return -3;
			break;
		}

		s_prepeers.insert(std::pair<uint64_t, ALTUS_SPrePeer*>(addrport, newpeer));
		return 1;
	}

	auto peer = searchres->second;
	switch (page)
	{
	case('A'):
		memcpy(peer->publicKey, buf, size - ALTUS_HANDSHAKE_HEADER_SIZE);
		peer->A = true;
		break;
	case('B'):
		memcpy(peer->publicKey + (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE), buf, size - ALTUS_HANDSHAKE_HEADER_SIZE);
		peer->B = true;
		break;
	case('C'):
		memcpy(peer->publicKey + (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE) * 2, buf, size - ALTUS_HANDSHAKE_HEADER_SIZE);
		peer->C = true;
		break;
	case('D'):
		memcpy(peer->publicKey + (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE) * 3, buf, size - ALTUS_HANDSHAKE_HEADER_SIZE);
		peer->D = true;
		break;
	case('Z'): //handshake done packet. now, the client will expect server's .aml packet.
		if (peer->IsAckSent) {
			this->prepeerToPeer(addrport); //this deletes prepeer from prepeers.
			return 0;
		}
		break;
	default:
		break;
	}

	if (   (peer->keySize < (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE) * 2) && (peer->A || peer->B)
		|| (peer->keySize < (ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE) * 3) && (peer->A || peer->B || peer->C)
		|| (peer->A && peer->B && peer->C && peer->D) ) {
		this->sendAck(addrport, peer);
	}
	return 0;
}

void ALTUSHost::sendAck(uint64_t addrport, ALTUS_SPrePeer* prepeer) {
	SOCKADDR_IN remoteAddr;
	uint32_t ip = addrport >> 32;
	uint16_t port = addrport & 0xFFFF;

	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_addr.S_un.S_addr = ip;
	remoteAddr.sin_port = port;
	
	char msg[ALTUS_HANDSHAKE_HEADER_SIZE + ALTUS_SESSION_KEY_SIZE];
	memcpy(msg, "ALTUS/1.0Y", 10);
	memset(msg + 10, 0, 2);
	memset(msg + ALTUS_HANDSHAKE_HEADER_SIZE, 0, ALTUS_SESSION_KEY_SIZE);
	//TODO:replace line above.
	//TODO:generate session key.
	//TODO:encrypt message with server's private key.
	//TODO:encrypt message with client's public key.
	
	//for now, we don't care the session key.(also not encrypted)
	sendto(localsocket, msg, ALTUS_HANDSHAKE_HEADER_SIZE + ALTUS_SESSION_KEY_SIZE, 
		0, (SOCKADDR*)&remoteAddr, sizeof(remoteAddr));

	prepeer->IsAckSent = true;
}

void ALTUSHost::prepeerToPeer(uint64_t addrport) {
	uint32_t ip = addrport >> 32;
	uint16_t port = addrport & 0xFFFF;

	ALTUS_SPrePeer* src = s_prepeers.find(addrport)->second;
	auto collision = peers.find(addrport);
	if (collision != peers.end()) {
		//TODO: collision handler.
		exit(-1);
	}

	ALTUSPeer* dst = new ALTUSPeer(src, ip, port, localsocket, pool); //server-side initial cuid is 0.

	peers.insert(std::pair<uint64_t, ALTUSPeer*>(addrport, dst));

	EnterCriticalSection(&peidCreation);
	unsigned int newpeid = lastpeid + 1;
	while (peid_peer.find(newpeid) != peid_peer.end()) {
		newpeid++;
	}
	lastpeid = newpeid;
	peid_peer.insert(std::pair<unsigned int, ALTUSPeer*>(newpeid, dst));
	LeaveCriticalSection(&peidCreation);

	delete src;
	s_prepeers.erase(addrport);

	acceptAction(this, newpeid);
}

int ALTUSHost::removePeer(unsigned int peid) {
	auto peer = peid_peer.find(peid);
	if (peer == peid_peer.end()) {
		return -1;
	}

	uint64_t addrport;
	addrTo_uint64(&addrport, peer->second->remoteAddr);
	peers.erase(addrport);
	delete peer->second;
	peid_peer.erase(peid);
	return 0;
}

void ALTUSHost::keep_accept() {
	_IsAccepting = true;
}

void ALTUSHost::decline_accept() {
	_IsAccepting = false;
}

bool ALTUSHost::IsAccepting() {
	return _IsAccepting;
}

bool ALTUSHost::IsListening() {
	return _IsAlive;
}

ALTUS_CPrePeer* ALTUSHost::popCPrePeer(uint64_t addrport) {
	EnterCriticalSection(&poppingcprepeer);
	auto searchRes = c_prepeers.find(addrport);
	if (searchRes == c_prepeers.end()) {
		LeaveCriticalSection(&poppingcprepeer);
		return nullptr;
	}
	auto oldCPrePeer = searchRes->second;
	c_prepeers.erase(searchRes);
	LeaveCriticalSection(&poppingcprepeer);

	return oldCPrePeer;
}

int ALTUSHost::connect(uint32_t ip, uint16_t _port, 
		BYTE* serverkey, int keylen) { //port in host byte order.
	uint16_t port = htons(_port);

	//Creating new Client-side pre-peer.

	uint64_t addrport;
	addrport = ip;
	addrport = addrport << 32;
	addrport = addrport | port;
	if (c_prepeers.find(addrport) != c_prepeers.end()) return -1;
	ALTUS_CPrePeer* newpeer = new ALTUS_CPrePeer(ip, port, serverkey, keylen);
	c_prepeers.insert(std::pair<uint64_t, ALTUS_CPrePeer*>(addrport, newpeer));

	//send initial handshake packet.
	SendHandShake1(addrport);

	//put new cprepeer in Retry queue.
	for (int i = 1; i <= ConnectRetryCount; i++) {
		RetryQuque.push(std::pair<uint32_t, ALTUS_CPrePeer*>(newpeer->starttime + i * ConnectWaitTime, newpeer));
	}
	return 0;
}

int ALTUSHost::_Connecter() {
	while (_IsAlive) {
		if (RetryQuque.empty()) {
			std::unique_lock<std::mutex> locker(connmut);
			connecterCond.wait(locker);
			if (this->RetryQuque.empty()) break; //this the case when deconstructing host.
		}

		std::pair<uint32_t, ALTUS_CPrePeer*> t = RetryQuque.top();

		uint32_t currentTime;
		{
			using namespace std::chrono;
			currentTime = static_cast<uint32_t>(duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()).count());
		}

		int32_t sleepTime = t.first - currentTime;
		if (sleepTime > 0) {
			Sleep(sleepTime);
			using namespace std::chrono;
			currentTime = static_cast<uint32_t>(duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()).count());
		}

		//check if this cprepeer already became peer.
		if (t.second->Expired) {
			std::cout << "Expired cprepeer" << std::endl;
			RetryQuque.pop();
			continue;
		}

		uint64_t addrport;
		addrTo_uint64(&addrport, t.second->remoteAddr);

		int32_t peerTime = t.first - t.second->starttime;
		if (peerTime >= ConnectExpireTime) { //this was the cprepeer expiring time.
			c_prepeers.erase(addrport);
			delete t.second;
			RetryQuque.pop();
			std::cout << "deleting expired cprepeer" << std::endl;
			continue;
		}

		//now, send handshake 1 packet again.
		std::cout << "send handshake 1 packet again" << std::endl;
		SendHandShake1(addrport);
		RetryQuque.pop();
	}
	return 0;
}

int ALTUSHost::connect(PCWSTR ip, uint16_t port, BYTE* serverkey, int keylen) {
	uint32_t remoteip;
	InetPtonW(AF_INET, ip, &remoteip);
	return connect(remoteip, port, serverkey, keylen);
}

int ALTUSHost::SendHandShake1(uint64_t addrport) {
	auto prepeer = findCPrePeer(addrport);
	if (prepeer == nullptr) return -1;

	SOCKADDR_IN remoteAddr;
	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_addr.S_un.S_addr = addrport >> 32;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = addrport & 0xFFFF;
	char page = 'A';
	for (int sentLen = 0; sentLen < myPublicKeySize;) {
		char msg[ALTUS_PACKET_SIZE];
		memcpy(msg, "ALTUS/1.0", 9);
		msg[9] = page;
		u_short nboks = htons(myPublicKeySize);
		memcpy(msg + 10, &nboks, 2);

		int sendinglength = ALTUS_PACKET_SIZE - ALTUS_HANDSHAKE_HEADER_SIZE;
		if (sendinglength > myPublicKeySize - sentLen) {
			sendinglength = myPublicKeySize - sentLen;
		}
		memcpy(msg + ALTUS_HANDSHAKE_HEADER_SIZE, myPublicKey, sendinglength);
		//msg를 prepeer->publicKey 로 암호화할 것.

		sendto(localsocket, msg, sendinglength, 0, (SOCKADDR*)&remoteAddr, sizeof(remoteAddr));
		//printf("sent page: %c\n", page);
		sentLen += sendinglength;
		page++;
	}
	return 0;
}

int ALTUSHost::TryCompleteConnection( //handshake3.
		uint64_t addrport, char* buf, int recvSize) {
	//check if this cprepeer exists.
	ALTUS_CPrePeer* cprepeer = popCPrePeer(addrport);
	if (cprepeer == nullptr) return -1;

	// TODO: 서버 퍼블릭키로 복호화.
	// TODO: 클라이언트 프라이빗키로 복호화.

	//check if this packet is valid, and save session key.
	if (recvSize != ALTUS_HANDSHAKE_HEADER_SIZE + ALTUS_SESSION_KEY_SIZE) return -1;
	if (strncmp(buf, "ALTUS/1.0Y\0\0", ALTUS_HANDSHAKE_HEADER_SIZE) != 0) {
		return -1;
	}
	cprepeer->Expired = true;
	memcpy(cprepeer->sessionKey, buf + ALTUS_HANDSHAKE_HEADER_SIZE, ALTUS_SESSION_KEY_SIZE);


	//check if same ip/port peer already exist.
	auto collision = peers.find(addrport);
	if (collision != peers.end()) {
		//TODO: run collision handler.
		exit(-1);
	}

	ALTUSPeer* newPeer = new ALTUSPeer(addrport, cprepeer, localsocket, pool); //server-side initial cuid is 0.

	peers.insert(std::pair<uint64_t, ALTUSPeer*>(addrport, newPeer));

	EnterCriticalSection(&peidCreation);
	unsigned int newpeid = lastpeid + 1;
	while (peid_peer.find(newpeid) != peid_peer.end()) {
		newpeid++;
	}
	lastpeid = newpeid;
	peid_peer.insert(std::pair<unsigned int, ALTUSPeer*>(newpeid, newPeer));
	LeaveCriticalSection(&peidCreation);

	//let user-defined connection behavior run in separate thread.
	std::thread action = std::thread([&]() {
		this->connectAction(this, newpeid);
		});
	action.detach();
	
	char msg[] = "ALTUS/1.0Z\0\0";
	sendto(localsocket, msg, ALTUS_HANDSHAKE_HEADER_SIZE, 0, (SOCKADDR*)&(newPeer->remoteAddr), sizeof(newPeer->remoteAddr));
	return 0;
}

int ALTUSHost::Sender() {
	while (_IsAlive)
	{
		sending.lock();
		for (std::pair<uint64_t, ALTUSPeer*> p : peers) {
			p.second->Send();
		}
		sending.unlock();
		Sleep(300);
	}
	if (!_IsAlive) return 0;
	return -1;
}

void ALTUSHost::SetDefaultAcceptAction(SimpleCallBack func) {
	acceptAction = func;
}

void ALTUSHost::SetDefaultConnectAction(SimpleCallBack func) {
	connectAction = func;
}

void ALTUSHost::FlushRetryQueue() {
	while (!RetryQuque.empty()) {
		printf("time: %u\n", RetryQuque.top().first);
		RetryQuque.pop();
	}
}

void ALTUSHost::debug() {
	printf("====debug====\n");
	for (std::pair<unsigned int, ALTUSPeer*> peer : peid_peer) {
		printf("peid[%d]\n", peer.first);
	}
}

DWORD WINAPI Receiver(LPVOID lpParam)
{
	ALTUSHost* host = (ALTUSHost*)lpParam;

	SOCKADDR_IN remoteAddr;
	int addrlen;
	addrlen = sizeof(remoteAddr);
	char buf[ALTUS_PACKET_SIZE];

	uint64_t addrport = 0;
	ALTUSPeer* peer;

	while (host->IsListening()) {
		int recvSize = recvfrom(host->localsocket, buf, ALTUS_PACKET_SIZE, 0,
			(SOCKADDR*)&remoteAddr, &addrlen);
		if (recvSize == SOCKET_ERROR) {
			continue;
		}

		addrTo_uint64(&addrport, remoteAddr);

		if( ( peer = host->getPeer(addrport) ) == nullptr ) {
			//check if it's in the c_prepeers;
			ALTUS_CPrePeer* cprepeer = host->findCPrePeer(addrport);
			if (cprepeer != nullptr) {
				host->TryCompleteConnection(addrport, buf, recvSize);
				continue;
			}

			if (host->IsAccepting()) {
				host->accepter(addrport, buf, recvSize);
			}
			continue;
		}

		peer->processPacket(buf, recvSize);
	}
    return 0;
}
