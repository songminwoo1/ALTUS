#include "NodeBase.h"

Node::Node() {
	to_tail = nullptr;
}

NodePool::NodePool() {
	size = 0;
	recycled_head = nullptr;
	recycled_tail = nullptr;
}

NodePool::~NodePool() {
	m.lock();
	Node* currentNode = recycled_head;
	while (currentNode != nullptr) {
		Node* nextNode = currentNode->to_tail;
		delete currentNode;
		currentNode = nextNode;
	}
	m.unlock();
}

Node* NodePool::pop() {
	if (recycled_head == nullptr) return nullptr;
	m.lock();
	Node* poppingNode = recycled_head;
	recycled_head = recycled_head->to_tail;
	size--;
	m.unlock();
	return poppingNode;
}

void NodePool::push(Node* node) {
	node->to_tail = nullptr;

	m.lock();
	size++;
	if (recycled_head == nullptr) {
		recycled_head = node;
		recycled_tail = node;
		m.unlock();
		return;
	}
	
	recycled_tail->to_tail = node;
	m.unlock();
}

int NodePool::resize(int n) {
	m.lock();
	int removeCount = size - n;
	if (removeCount <= 0) {
		m.unlock();
		return 0;
	}

	Node* currentNode = recycled_head;
	for (int i = 1; i < n; i++) {
		currentNode = currentNode->to_tail;
	}
	recycled_tail = currentNode;

	Node* deletingNode = currentNode->to_tail;
	recycled_tail->to_tail = nullptr;
	size = n;
	m.unlock();

	while (deletingNode != nullptr) {
		Node* next = deletingNode->to_tail;
		delete deletingNode;
		deletingNode = next;
	}
	return removeCount;
}	