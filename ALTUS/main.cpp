#include <iostream>
extern "C"
{
#include "windowedqueue.h"
#include "ublist.h"
}

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

	int a, b;
	char index = 'a';
	char buf[4096];
	while (true) {
		scanf_s("%d", &a);
		scanf_s("%d", &b);

		memset(buf, '_', 4096);
		memset(buf, index, b*ALTUS_WQ_BLOCK_SIZE);
		altusWQPut(myWindow, myPool, a, b, buf);

		altusTestWQ(myWindow);
		altusTestP(myPool);
		if (myWindow->head == myWindow->unsafe_tail) {
			altusTestN(myWindow->head);
		}
		for (WQ_Node* currentN = myWindow->head; currentN != myWindow->unsafe_tail; currentN = (WQ_Node*)(currentN->to_tail)) {
			altusTestN(currentN);
		}
		if (myWindow->head != myWindow->unsafe_tail) {
			altusTestN(myWindow->unsafe_tail);
		}
		ALTUS_UBTest(&(myWindow->ub_list));
		index++;
	}

	altusFreeWQ(myWindow);
}