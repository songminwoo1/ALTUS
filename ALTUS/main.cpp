#include "ALTUSModule.h"

void AcceptSend(void* _host, int a) {
	ALTUSHost* host = (ALTUSHost*)_host;
	ALTUSPeer* peer = host->getPeerbyID(a);
	char buf[] = "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of de Finibus Bonorum et Malorum (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, Lorem ipsum dolor sit amet.., comes from a line in section 1.10.32.";

	int retval = peer->CreateUpstream(1, buf, strlen(buf));
}

int main() {
	ALTUS_PrintLogo();
#ifdef _DEBUG
	printf("<== debug ==>\n");
#endif // _DEBUG

#ifndef _DEBUG
	printf("<== relase build ==>\n");
#endif // !_DEBUG

	ALTUS_Initialize();

	std::unique_ptr<ALTUSHost> host1(new ALTUSHost);
	std::unique_ptr<ALTUSHost> host2(new ALTUSHost(ALTUS_DEFAULT_PORT + 1));

	host1->SetDefaultAcceptAction(AcceptSend);
	host2->SetDefaultAcceptAction(AcceptSend);
	host1->SetDefaultConnectAction([](void* host, int a) {printf("Host1: connected a new peer[%d]\n", a); Sleep(5000); printf("22\n"); });
	host2->SetDefaultConnectAction([](void* host, int a) {printf("Host2: connected a new peer[%d]\n", a); Sleep(5000); printf("33\n");  });

	host1->run();
	host2->run();

	BYTE* remoteKey = (BYTE*)malloc(ALTUS_RSA_KEY_SIZE);

	host1->connect(L"127.0.0.1", 1600, remoteKey, ALTUS_RSA_KEY_SIZE);
	//Sleep(5);
	//host1->connect(L"127.0.0.1", ALTUS_DEFAULT_PORT + 2, remoteKey, ALTUS_RSA_KEY_SIZE);

	Sleep(1000);
	auto recvPeer = host1->getPeerbyID(1);
	//TODO: read received data.
	auto recvChannel = recvPeer->getChannelByID(1);
	for (int i = 0; i < 100; i++) {
		char buf[101];
		int length = altusWQPop(recvChannel->buffer, recvChannel->pool, 100, buf);
		buf[length] = 0;
		if (length == 0) {
			std::cout << std::endl;
			break;
		}
		std::cout << buf;
		Sleep(100);
	}
	Sleep(4000);
	printf("done\n");
	host1->FlushRetryQueue();

	host1 = nullptr; //wait until this destruction ends.
	host2 = nullptr;
	ALTUS_Terminate();

	free(remoteKey);
	return 0;
}