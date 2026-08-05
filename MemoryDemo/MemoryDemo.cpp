#include <iostream>

int target(int a, int b) {
	int result = a + b;
	return result;
}

int main() {
	void* address = (void*)target;
	std::cout << "Our target function lives in virtual memory at address: " << address << std::endl;

	unsigned char* bytepointer = (unsigned char*)address;
	std::cout << "Reading the first 15 bytes of the target function" << std::endl;

	for (int i = 0; i < 15; i++) {
		std::cout << std::hex << (int)bytepointer[i] << " ";
	}

	std::cout << std::dec << std::endl;
	return 0;
}