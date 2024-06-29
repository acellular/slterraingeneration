#pragma once

//#include "ofMain.h"
#include <cstddef>
#include <iostream>
#include <string.h>
#include <vector>
#include <fstream>

//https://forum.openframeworks.cc/t/text-wrapping/2953
std::string wrapString(ofTrueTypeFont* font, std::string text, int width);
std::vector<std::string> splitString(std::string text, std::string delimiter);
std::string strToLower(std::string str);
std::string strRemoveCharacters(std::string str, char c);
std::string strToUpper(std::string str);
std::string strCapitalizeWord(std::string str);
std::string strCapitalizeSentence(std::string str);
std::string strCapitalizeEveryWord(std::string str);
bool saveString(std::string& s, ofstream& fout);
bool loadString(std::string& s, ifstream& fin);


namespace winfileutil {
	void GetFilesInDirectory(std::vector<string>& out, string directory);
}


template <typename T>
bool saveVector(std::vector<T>& vector, ofstream& fout) { //must be binary file
	//TODO-->checks on must be binary file and vector??-->try catch error around whole thing????
	printf(":SAVE MATRIX:\n");
	int size = (int)(vector.size());
	fout.write((char*)(&size), sizeof(int));

	printf("SSize of T: %d\n", sizeof(T));
	printf("SSize of vector: %d\n", size);
	fout.write((char*)(&vector[0]), size * sizeof(T));
	return true;
}


template <typename T>
bool loadVector(std::vector<T>& vector, ifstream& fin) { //must be binary file
	//TODO-->checks on must be binary file and vector??-->try catch error around whole thing????
	printf(":LOAD MATRIX:\n");
	int size;
	fin.read((char*)(&size), sizeof(int));

	printf("Size of T: %d\n", sizeof(T));
	printf("Size of vector: %d\n", size);

	vector = std::vector<T>(size);

	fin.read((char*)(&vector[0]), size * sizeof(T));
	return true;
}


template <typename T>
bool saveMatrix(std::vector<std::vector<T>>& matrix, ofstream& fout) { //must be binary file
	//TODO-->checks on must be binary file and matrix??-->try catch error around whole thing????
	printf(":SAVE MATRIX:\n");
	int sizeY = (int)(matrix.size());
	int sizeX = (int)(matrix[0].size());
	fout.write((char*)(&sizeY), sizeof(int));
	fout.write((char*)(&sizeX), sizeof(int));

	printf("SSize of T: %d\n", sizeof(T));
	printf("SSize of matrix: %d, %d\n", sizeY, sizeX);

	for (int i = 0; i < sizeY; i++) {
		fout.write((char*)(&matrix[i][0]), sizeX * sizeof(T));
	}
	return true;
}

//GPT SUGGESTS
//template <typename T>
//bool saveMatrix(std::vector<std::vector<T>>* matrix, std::ofstream* fout) {
//	// TODO: Add checks and error handling
//
//	printf(":SAVE MATRIX:\n");
//	int sizeY = static_cast<int>((*matrix).size());
//	int sizeX = static_cast<int>((*matrix)[0].size());
//	fout->write(reinterpret_cast<char*>(&sizeY), sizeof(int));
//	fout->write(reinterpret_cast<char*>(&sizeX), sizeof(int));
//	for (int i = 0; i < sizeY; i++) {
//		fout->write(reinterpret_cast<char*>((*matrix)[i].data()), (*matrix)[i].size() * sizeof(T));
//	}
//	return true;
//}

template <typename T>
bool loadMatrix(std::vector<std::vector<T>>& matrix, ifstream& fin) { //must be binary file
	//TODO-->checks on must be binary file and matrix??-->try catch error around whole thing????
	printf(":LOAD MATRIX:\n");
	int y, x;
	fin.read((char*)(&y), sizeof(int));
	fin.read((char*)(&x), sizeof(int));

	printf("Size of T: %d\n", sizeof(T));
	printf("Size of matrix: %d, %d\n", y, x);

	matrix = std::vector <std::vector<T>>(y, std::vector<T>(x));

	for (int i = 0; i < y; i++) {
		fin.read((char*)(&matrix[i][0]), x * sizeof(T));//doesn't seem to work with bool?? read access violation
	}
	return true;
}




std::vector<std::string> getNextCSVLineSplit(std::istream& str);

ofColor colourConverter(const char* hex);

class slTimer {
	float startTime = 0;
	float duration = 0;
	bool isFinished = true;
public:
	slTimer(float _duration) {
		duration = _duration;
	};
	void start() {
		startTime = ofGetElapsedTimef();
		isFinished = false;
	};
	void reset() {
		start();
	};
	void stop() {
		isFinished = true;
	};
	bool isDone() {
		if (isFinished) {
			return true;
		}
		else {
			if (ofGetElapsedTimef() - startTime > duration) {
				isFinished = true;
				return true;
			}
			else {
				return false;
			}
		}
	};
	bool isRunning() {
		return !isFinished;
	};
	float getPercentDone() {
		if (isFinished) {
			return 1.0;
		}
		else {
			return (ofGetElapsedTimef() - startTime) / duration;
		}
	};
	float getPercentLeft() {
		if (isFinished) {
			return 0.0;
		}
		else {
			return 1.0 - (ofGetElapsedTimef() - startTime) / duration;
		}
	};
	float getSecondsLeft() {
		if (isFinished) {
			return 0.0;
		}
		else {
			return duration - (ofGetElapsedTimef() - startTime);
		}
	};
	float getSecondsDone() {
		if (isFinished) {
			return 0.0;
		}
		else {
			return ofGetElapsedTimef() - startTime;
		}
	};
	float getDuration() {
		return duration;
	};
	float getStartTime() {
		return startTime;
	};

};

// Object pool structures for particles
// node
class PoolNode {
public:
	PoolNode* next;
	int value;
};

// object pool list head
class ObjectPoolIndex {
	PoolNode* head;
public:
	int size;
	ObjectPoolIndex() {
		head = nullptr;
		size = 0;
	}
	int pop() {
		if (head != nullptr) {
			PoolNode* node = head;
			head = head->next;
			int ret = node->value;
			delete node;//once there was no next node?? how could that happen?
			size--;
			return ret;
		}
		else {
			return -1; //list is empty
		}
	}
	void push(int index) {
		PoolNode* node = new PoolNode();
		node->value = index;
		node->next = head;
		head = node;
		size++;
	}
	void clear() {
		while (head != nullptr) {
			PoolNode* node = head;
			head = head->next;
			delete node;
		}
		size = 0;
	}

};
