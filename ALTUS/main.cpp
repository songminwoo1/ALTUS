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

	ALTUS_UB_List myUBs;
	ALTUS_UB_List* myUB = &myUBs;
	memset(myUB, 0, sizeof(ALTUS_UB_List));

	int a, b;
	while (true) {
		scanf_s("%d", &a);
		scanf_s("%d", &b);

		printf("output: %d\n", altusUBCheck(myUB, a, b));
		ALTUS_UBTest(myUB);
	}
}