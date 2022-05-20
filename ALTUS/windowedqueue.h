#pragma once
#define ALTUS_WQ_BLOCK_SIZE 16 //block size of AES, 128-bit. (smallest unit of data in a node)
#define ALTUS_WQ_NODE_BLOCK_COUNT 256 //number of blocks in one node.
#define ALTUS_WQ_NODE_SIZE 4096 //DATA_BLOCK_SIZE * WINDOW_NODE_BLOCK_COUNT

typedef struct WQ_Node_Struct { //component of WQ_WINDOW
	void* to_head;
	void* to_tail;

	char data[ALTUS_WQ_NODE_SIZE]; //4096 byte of data section.
	unsigned char validBlockCount; //number of valid blocks in data section.(max is 256)
} WQ_Node;

typedef struct WQ_WINDOW_STRUCT {
	//all node pointer must *not* be NULL. at least one node is in the window.
	WQ_Node* head;
	WQ_Node* safe_tail;
	WQ_Node* unsafe_tail; //when there is no unsafe node, this is same with safe_tail.
	unsigned int safe_len; //at least 1. this parameter is not reliable.
	unsigned int unsafe_len; //min is 0. this parameter is not reliable.

	//this parameters are for unsequenced data handling.
	unsigned int lastSafeSeq; //last 'readable' seq.
} WQ_Window;

typedef struct WQ_NODE_POOL_STRUCT {
	//all node pointer must *not* be NULL. at least one node is in the pool.
	WQ_Node* recycled_head;
	WQ_Node* recycled_tail;
	unsigned int len; //min is 1. this parameter is not reliable.
} WQ_Node_Pool;

WQ_Window* altusNewWindow();

WQ_Node_Pool* altusNewNodePool();

void altusPopNode(WQ_Window* window, WQ_Node_Pool* pool); //pop head node from window and recycle it.

void altusNewNode(WQ_Window* window, WQ_Node_Pool* pool); //pop a node from recycled node pool and append it to the window. if no node available in pool, malloc new one.

void altusFreeP(WQ_Node_Pool* pool); //free all nodes in pool.

void altusFreeWQ(WQ_Window* window); //free all nodes in window.

#ifdef _DEBUG
void altusTestN(WQ_Node* node);
void altusTestWQ(WQ_Window* window);
void altusTestP(WQ_Node_Pool* pool);
#endif // _DEBUG