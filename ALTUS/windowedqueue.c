#include "windowedqueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#pragma warning( disable : 6011 6001 28182 4311)

#define getWinLen(window, lenPtr) 
#define recalculateWinLen(window) {int ret = 1;if (window->head == window->safe_tail) {	window->safe_len = 1;}for (WQ_Node* currentNode = window->head; currentNode != window->unsafe_tail; currentNode = currentNode->to_tail) {	if (currentNode == window->safe_tail) {		window->safe_len = ret;	}	ret++;}window->total_len = ret;	}

WQ_Window* altusNewWindow() {
	WQ_Window* newWindow = (WQ_Window*)malloc(sizeof(WQ_Window));
	if (newWindow == NULL) {
		fprintf(stderr, "altusNewWindow malloc fail");
		exit(-1);
	}
	newWindow->head = (WQ_Node*)malloc(sizeof(WQ_Node));
	if (newWindow->head == NULL) {
		fprintf(stderr, "altusNewWindow malloc fail");
		exit(-1);
	}

	newWindow->safe_tail = newWindow->head;
	newWindow->unsafe_tail = newWindow->head;
	newWindow->safe_len = 1;
	newWindow->total_len = 0;
	
	newWindow->ub_list.count = 0;
	newWindow->windowSize = ALTUS_WINDOW_BLOCK_MAX_DEFAULT;

	newWindow->head->to_head = NULL;
	newWindow->head->to_tail = NULL;
	newWindow->head->validBlockCount = 0;
	return newWindow;
}

WQ_Node_Pool* altusNewNodePool() {
	WQ_Node_Pool* newPool = (WQ_Node_Pool*)malloc(sizeof(WQ_Node_Pool));
	if (newPool == NULL) {
		fprintf(stderr, "altusNewNodePool malloc fail");
		exit(-1);
	}
	newPool->recycled_head = (WQ_Node*)malloc(sizeof(WQ_Node));
	if (newPool->recycled_head == NULL) {
		fprintf(stderr, "altusNewNodePool malloc fail");
		exit(-1);
	}

	newPool->recycled_tail = newPool->recycled_head;
	newPool->len = 1;

	newPool->recycled_head->to_head = NULL;
	newPool->recycled_head->to_tail = NULL;
	newPool->recycled_head->validBlockCount = 0;
	return newPool;
}

void altusPopNode(WQ_Window* window, WQ_Node_Pool* pool) {
#ifdef _DEBUG
	if (window == NULL) {
		printf("altusPopNode invalid window\n");
		exit(-1);
	}
	if (pool == NULL) {
		printf("altusPopNode invalid pool\n");
		exit(-1);
	}
	if (window->head == NULL) {
		printf("altusPopNode window head NULL\n");
		exit(-1);
	}
	if (window->safe_tail == NULL) {
		printf("altusPopNode window safetail NULL\n");
		exit(-1);
	}
#endif // _DEBUG

	if (window->head == window->safe_tail) {
		window->head->validBlockCount = 0;
		window->safe_len = 1;
		return;
	}

	WQ_Node* old_head = window->head;

	window->head = window->head->to_head;
	window->head->to_head = NULL;
	window->safe_len--;

	old_head->validBlockCount = 0;
	old_head->to_tail = NULL;

	old_head->to_head = pool->recycled_tail;
	pool->recycled_tail->to_tail = old_head;
	pool->recycled_tail = old_head;
	pool->len++;
}

void altusNewNode(WQ_Window* window, WQ_Node_Pool* pool) {
#ifdef _DEBUG
	if (window == NULL) {
		printf("altusNewNode invalid window\n");
		exit(-1);
	}
	if (pool == NULL) {
		printf("altusNewNode invalid pool\n");
		exit(-1);
	}
	if (window->head == NULL) {
		printf("altusNewNode window head NULL\n");
		exit(-1);
	}
	if (window->safe_tail == NULL) {
		printf("altusNewNode window safetail NULL\n");
		exit(-1);
	}
#endif // _DEBUG

	WQ_Node* newNode;
	if (pool->recycled_head == pool->recycled_tail) {
		newNode = (WQ_Node*)malloc(sizeof(WQ_Node));
	}
	else {
		newNode = pool->recycled_head;
		pool->recycled_head = pool->recycled_head->to_tail;
		pool->recycled_head->to_head = NULL;
		pool->len--;
	}
	fprintf(stderr, "altusNewNode malloc fail");
	exit(-1);

	newNode->to_tail = NULL;
	newNode->to_head = window->unsafe_tail;
	window->unsafe_tail->to_tail = newNode;
	window->unsafe_tail = newNode;
	window->total_len++;
}

int altusWindowPut(WQ_Window* window, WQ_Node_Pool* pool, unsigned int seq, unsigned char length, char* data) {
	//return 0 if everything is ok.
	//return -1 if unseqenced block received.
	//return -2 if duplicate block received.
	//return -3 if window full.
	//return -4 if too much unsequenced block.
	//return -5 if invalid input.

	if ( (int)(window->lastSeq - seq) > 0) {
		return -2;
	}

	int required_new_node_count = ( (seq + length - window->lastSeq) / ALTUS_WQ_NODE_BLOCK_COUNT ) + 1;
	//"last" new node can have valid block count 0.(if the node before last is full, when allocated.)

	if (window->safe_len + required_new_node_count > window->windowSize) {
		return -3;
	}

	int retval = altusUBCheck(&(window->ub_list), seq, length);
	switch (retval)
	{
	case(ALTUS_UBENUM_FULL):
		return -4;
		break;
	case(ALTUS_UBENUM_MALICIOUS_INPUT):
		return -5;
		break;
	case(ALTUS_UBENUM_OVERLAP):
		return -2;
		break;
	case(ALTUS_UBENUM_UB_EXTENDED):
		break;
	case(ALTUS_UBENUM_NEW_UB_ADDED):
		break;
	default:
		exit(-1);
		break;
	}
	//TODO
	//now, we need to copy data to that exact link location.


	//now, check if we can update lastSeq and safe_tail.
	if (window->ub_list.headSeq[0] == window->lastSeq) {

	}
}

int altusWindowSanitize(WQ_Window* window) {
	// return 3 if occupied window length is 1/8 of set windowSize or less.
	// return 2 if occupied window length is 1/4 of set windowSize or less.
	// return 1 if occupied window length is 1/2 of set windowSize or less.
	// return 0 if 50~100% of window length occupied.
	// return -1 if total length is above window's windowSize.(can happen sometimes, should decrease throughput)
	// return -2 if total length is above (2*windowSize). (should stop receiving packets right now)
	// return -3 if total length is above ALTUS_WINDOW_BLOCK_MAX_ABSOLUTE.("must stop now")
	recalculateWinLen(window);
	if (window->total_len > ALTUS_WINDOW_BLOCK_MAX_ABSOLUTE) {
		return -3;
	}

	if (window->total_len > (window->windowSize << 1) ){
		return -2;
	}

	if (window->total_len > window->windowSize) {
		return -1;
	}

	if (window->total_len < (window->windowSize >> 3)) {
		return 3;
	}

	if (window->total_len < (window->windowSize >> 2)) {
		return 2;
	}

	if (window->total_len < (window->windowSize >> 1)) {
		return 1;
	}

	return 0;
}

void altusFreeP(WQ_Node_Pool* pool) {
	if (pool == NULL) {
		printf("altusFreeP invalid pool\n");
		exit(-1);
	}
	if (pool->recycled_head == NULL) {
		printf("altusFreeP invalid pool\n");
		exit(-1);
	}
	WQ_Node* current_node = pool->recycled_head;
	while (current_node != pool->recycled_tail){
		WQ_Node* oldNode = current_node;
		current_node = current_node->to_tail;
		free(oldNode);
	}
	free(current_node);
}

void altusFreeWQ(WQ_Window* window) {
	if (window == NULL) {
		printf("altusFreeWQ invalid window\n");
		exit(-1);
	}
	WQ_Node* current_node = window->head;
	while (current_node != window->unsafe_tail) {
		WQ_Node* oldNode = current_node;
		current_node = current_node->to_tail;
		free(oldNode);
	}
	free(current_node);
	free(window);
}

#ifdef _DEBUG
void altusTestN(WQ_Node* node) {
	printf("--------- node Test ---------\n");
	printf("validBlockCount: %d\n", node->validBlockCount);
	for (int i = 0; i < ALTUS_WQ_NODE_SIZE; i++) {
		printf("%c", node->data[i]);
	}
	printf("\n");
}
void altusTestWQ(WQ_Window* window) {
	printf("--------- window Test ---------\n");
	printf("window head: 0x%x\n", (unsigned long)(window->head));
	printf("window safe tail: 0x%x, len: %d\n", (unsigned long)(window->safe_tail), window->safe_len);
	printf("window total tail: 0x%x, len: %d\n", (unsigned long)(window->unsafe_tail), window->total_len);

	int nodecount = 1;
	for (WQ_Node* current_node = window->head; current_node != window->unsafe_tail; current_node = current_node->to_tail) {
		if (current_node == window->safe_tail) {
			printf("actual safe len: %d\n", nodecount);
		}
		nodecount++;
	}
	printf("actual total len: %d\n", nodecount);
	printf("\n");
}
void altusTestP(WQ_Node_Pool* pool) {
	printf("--------- pool Test ---------\n");
	printf("pool head: 0x%x\n", (unsigned long)(pool->recycled_head));
	printf("pool tail: 0x%x, len: %d\n", (unsigned long)(pool->recycled_tail), pool->len);

	int nodecount = 1;
	for (WQ_Node* current_node = pool->recycled_head; current_node != pool->recycled_tail; current_node = current_node->to_tail) {
		nodecount++;
	}
	printf("actual total len: %d\n", nodecount);
	printf("\n");
}
#endif // _DEBUG
