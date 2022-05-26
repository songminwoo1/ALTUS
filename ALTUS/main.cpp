extern "C"
{
#include "windowedqueue.h"
}

#define _CRT_SECURE_NO_WARNINGS

#include<iostream>
#include<fstream>
#include<time.h>
#include<Windows.h>

#define TEST_BUF_SIZ 2032

int main() {
#ifdef _DEBUG
	printf("================== debug ==================\n");
#endif // _DEBUG

#ifndef _DEBUG
	printf("================== relase build ==================\n");
#endif // !_DEBUG

	WQ_Window* myWindow = altusNewWindow();
	myWindow->lastSeq = 0;
	WQ_Node_Pool* myPool = altusWQNewNodePool();

	char buf[TEST_BUF_SIZ]; //prime number(for test)
	unsigned int seq, len, rlen;
	seq = 0;
	len = 0;
	rlen = 0;

	std::ifstream myFileIn("testFile.txt", std::ios::binary);
	std::ofstream myFileOut("testOut.txt", std::ios::binary);

	if (!myFileIn || !myFileOut) {
		printf("no file!\n");
		return -1;
	}

	clock_t nowTime = clock();
	clock_t startTime = nowTime;
	uint64_t totalProcessedMem = 0;
	while (true) {

		myFileIn.read(buf, TEST_BUF_SIZ);

		std::streamsize bytes = myFileIn.gcount();
		if (bytes == 0) {
			std::cout << "Memory processed: " << totalProcessedMem << std::endl;
			std::cout << "last seq: " << myWindow->lastSeq << std::endl;
			break;
		}


		if (altusWQPut(myWindow, myPool, seq, bytes / 16, buf) != 0) return 0;
		seq += bytes / 16;

		int poplen;
		if ((poplen = altusWQPop(myWindow, myPool, bytes / 16, buf)) != bytes / 16) return 0;
		myFileOut.write(buf, bytes);
		
		totalProcessedMem += poplen * ALTUS_WQ_BLOCK_SIZE;

		if (clock() - nowTime > 1000) {
			nowTime += 1000;
			std::cout << "Memory processed: " << totalProcessedMem << std::endl;
			std::cout << "last seq: " << myWindow->lastSeq << std::endl;
		}
	}
	std::cout << "Average Throughput : " << totalProcessedMem / (clock() - startTime) / 1000 << "MB/s" << std::endl;

	altusFreeWQ(myWindow);

	altusFreeP(myPool);

	myFileIn.close();
	myFileOut.close();

	system("cls");
	system("cd /d C:\\\\Users\\\\alsdn && tree");

	char* pPath;
	pPath = getenv("PATH");
	if (pPath != NULL)
		printf("The current path is: %s", pPath);
	while (true)
	{
		system("color 1f");
		Sleep(500);
		system("color 4f");
		Sleep(500);
	}
}