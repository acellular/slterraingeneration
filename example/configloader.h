#pragma once
#include <unordered_map>
#include <string>
#include <fstream>

// basic config loader for example purposes
// WARNING: does not handle most errors or work with categories/subheadings
class ConfigLoader {
public:
	void load() {
		std::ifstream file("config.ini");
		if (file.is_open()) {
			std::string line;
			while (std::getline(file, line)) {
				if (line.empty() || line[0] == '#' || line[0] == ';') {
					continue;
				}
				//check for same-line comments
				if (line.find('#') != std::string::npos) {
					line = line.substr(0, line.find('#'));
				}
				if (line.find(';') != std::string::npos) {
					line = line.substr(0, line.find(';'));
				}

				std::string key = line.substr(0, line.find('='));
				std::string value = line.substr(line.find('=') + 1);
				_config[key] = value;
			}
		}
		file.close();
	}

	std::string get(std::string key) {
		if (_config.find(key) == _config.end()) {
			printf(":::::CONFIG WARNING::::: key not found: %s\n", key.c_str());
			return "";
		}
		if (_config[key].empty()) {
			printf(":::::CONFIG WARNING::::: key returned empty: %s\n", key.c_str());
			return "";
		}
		if (_config[key][0] == ' ') {
			printf(":::::CONFIG WARNING::::: key returned space: %s\n", key.c_str());
			return "";
		}
		return _config[key];
	}

	float operator[](std::string key) {
		return std::stof(get(key));
	}

private:
	static std::unordered_map<std::string, std::string> _config;
};

// static member initialization
std::unordered_map<std::string, std::string> ConfigLoader::_config;