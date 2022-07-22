#pragma once
#pragma comment(lib, "ws2_32")
#include <vector>
#include <queue>
#include "ALTUSPeer.h"
#include <WinSock2.h>
#include <mutex>

#define addrTo_uint64(u, a) {*u = a.sin_addr.S_un.S_addr; *u = *u << 32; *u = *u | a.sin_port;}

#define ALTUS_DEFAULT_PORT 1605 //remember?
#define ALTUS_HANDSHAKE_HEADER_SIZE 12
#define ConnectRetryCount 3
#define ConnectWaitTime 1500
#define ConnectExpireTime ConnectRetryCount * ConnectWaitTime

class Comparator {
public:
	bool operator() (std::pair<uint32_t, ALTUS_CPrePeer*> a, std::pair<uint32_t, ALTUS_CPrePeer*> b);
};

class ALTUSHost
{
private:
	BYTE myPublicKey[ALTUS_RSA_KEY_SIZE];
	int myPublicKeySize;
	BYTE myPrivateKey[ALTUS_RSA_KEY_SIZE];
	int myPrivateKeySize;

	std::map<uint64_t, ALTUSPeer*> peers;		//ip+port to peer map
	std::map<unsigned int, ALTUSPeer*> peid_peer; //peid to peer, auto managed.
	unsigned int lastpeid;
	CRITICAL_SECTION peidCreation;
	std::map<uint64_t, ALTUS_SPrePeer*> s_prepeers; //prepare for connection.

	std::map<uint64_t, ALTUS_CPrePeer*> c_prepeers; //client-side pre-peer.
	CRITICAL_SECTION poppingcprepeer;
	std::priority_queue<std::pair<uint32_t, ALTUS_CPrePeer*>, std::vector< std::pair<uint32_t, ALTUS_CPrePeer*>>, Comparator> RetryQuque;
	std::thread connecterThread;
	std::mutex connmut;
	std::condition_variable connecterCond;
	CRITICAL_SECTION RTQEmptyCheck;

	HANDLE ListenerHandle;
	DWORD ListenerThreadID;
	bool _IsAlive; //goes false while deconstruction.
	bool _IsAccepting;

	//while sending packets.
	std::mutex sending;
	std::thread senderThread;

 	SimpleCallBack acceptAction;
	SimpleCallBack connectAction;

	WQ_Node_Pool* pool;
public:
	SOCKET localsocket;
	SOCKADDR_IN localaddr;

	ALTUSHost();
	ALTUSHost(uint32_t port);
	int run();
	~ALTUSHost();

	ALTUSPeer* getPeerbyID(int peid);
	ALTUSPeer* getPeer(uint64_t addrport);

	ALTUS_CPrePeer* findCPrePeer(uint64_t addrport);

	//internal function for accepting connection request.
	int accepter(uint64_t addrport, char* buf, int size);
	void sendAck(uint64_t addrport, ALTUS_SPrePeer* prepeer);
	void prepeerToPeer(uint64_t addrport);
	int removePeer(unsigned int peid);

	void keep_accept();
	void decline_accept();
	bool IsListening(); //only for internal use.
	bool IsAccepting();

	//client-side
	int _Connecter();
	ALTUS_CPrePeer* popCPrePeer(uint64_t addrport); //thread safe.
	int connect(uint32_t ip, uint16_t port, BYTE* serverkey, int keylen);
	int connect(PCWSTR ip, uint16_t port, BYTE* serverkey, int keylen);
	int SendHandShake1(uint64_t addrport);
	int TryCompleteConnection(uint64_t addrport, char* buf, int recvSize);

	//sender
	int Sender();

	//callback function.
	void SetDefaultAcceptAction(SimpleCallBack func);
	void SetDefaultConnectAction(SimpleCallBack func);

	void debug();
	void FlushRetryQueue();
};