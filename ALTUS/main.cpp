extern "C"
{
#include "windowedqueue.h"
}

#include<iostream>
#include<fstream>
#include<time.h>

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

	std::ifstream myFileIn("testFile.txt");
	std::ofstream myFileOut("testOut.txt");

	if (!myFileIn || !myFileOut) {
		printf("no file!\n");
		return -1;
	}

	clock_t nowTime = clock();
	uint64_t totalProcessedMem = 0;
	while (true) {

		myFileIn.read(buf, TEST_BUF_SIZ);
		if (altusWQPut(myWindow, myPool, seq, TEST_BUF_SIZ / 16, buf) != 0) return 0;
		seq += TEST_BUF_SIZ / 16;

		int poplen;
		if ((poplen = altusWQPop(myWindow, myPool, TEST_BUF_SIZ / 16, buf)) != TEST_BUF_SIZ / 16) return 0;
		myFileOut.write(buf, TEST_BUF_SIZ);
		
		totalProcessedMem += poplen * ALTUS_WQ_BLOCK_SIZE;


		if (totalProcessedMem >= 180704) {
			std::cout << "Memory processed: " << totalProcessedMem << std::endl;
			std::cout << "last seq: " << myWindow->lastSeq << std::endl;
			break;
		}

		if (clock() - nowTime > 1000) {
			nowTime += 1000;
			std::cout << "Memory processed: " << totalProcessedMem << std::endl;
			std::cout << "last seq: " << myWindow->lastSeq << std::endl;
		}
	}
	std::cout << "Average Throughput : " << totalProcessedMem / clock() / 1000 << "MB/s" << std::endl;

	altusFreeWQ(myWindow);
	altusFreeP(myPool);

	myFileIn.close();
	myFileOut.close();
}