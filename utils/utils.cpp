#include "utils.h"

//https://forum.openframeworks.cc/t/finding-bounding-width-height-depth-of-a-mesh/26190/2
ofVec3f getMeshBoundingBoxDimension(const ofMesh& mesh) {

	auto xExtremes = minmax_element(mesh.getVertices().begin(), mesh.getVertices().end(),
		[](const ofPoint& lhs, const ofPoint& rhs) {
			return lhs.x < rhs.x;
		});
	auto yExtremes = minmax_element(mesh.getVertices().begin(), mesh.getVertices().end(),
		[](const ofPoint& lhs, const ofPoint& rhs) {
			return lhs.y < rhs.y;
		});
	auto zExtremes = minmax_element(mesh.getVertices().begin(), mesh.getVertices().end(),
		[](const ofPoint& lhs, const ofPoint& rhs) {
			return lhs.z < rhs.z;
		});
	return ofVec3f(xExtremes.second->x - xExtremes.first->x,
		yExtremes.second->y - yExtremes.first->y,
		zExtremes.second->z - zExtremes.first->z);
}

std::string wrapString(ofTrueTypeFont* font, std::string text, int width) {

	std::string typeWrapped = "";
	std::string tempString = "";
	std::vector <std::string> words = ofSplitString(text, " ");

	for (int i = 0; i < words.size(); i++) {
		if (words[i] == "") {
			continue;
		}
		if (words[i] == "<br>") {
			typeWrapped += "\n";
			tempString = "";
			continue;
		}

		std::string wrd = words[i];
		//cout << wrd << endl;


		tempString += wrd + " ";
		//cout << tempString << endl;
		int stringwidth = font->stringWidth(tempString);
		//cout << stringwidth << endl;
		//stringwidth = getMeshBoundingBoxDimension(font->getStringMesh(tempString, 0, 0)).x;
		//cout << stringwidth << endl;
		if (stringwidth >= width) {
			tempString = wrd + " ";
			typeWrapped += "\n";
		}

		if (i == words.size() - 1) {
			typeWrapped += wrd;
		}
		else {
			typeWrapped += wrd + " ";
		}
	}

	return typeWrapped;
}

//think copilot may have just stolen this from stackoverflow
//convert to unique pointer?
std::vector<std::string> splitString(std::string text, std::string delimiter) {
	std::vector<std::string> strings;
	std::string::size_type pos = 0;
	std::string::size_type prev = 0;
	while ((pos = text.find(delimiter, prev)) != std::string::npos)
	{
		strings.push_back(text.substr(prev, pos - prev));
		prev = pos + 1;
	}
	// To get the last substring (or only, if delimiter is not found)
	strings.push_back(text.substr(prev));
	return strings;

}

std::string strToLower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
				[](unsigned char c) { return std::tolower(c); });
	return str;

	/*alt
	for (int x = 0; x < strlen(arr); x++)
		putchar(tolower(arr[x]));
		*/
}

std::string strRemoveCharacters(std::string str, char c) {
	str.erase(std::remove(str.begin(), str.end(), c), str.end());
	return str;
}

std::string strToUpper(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
						[](unsigned char c) { return std::toupper(c); });
	return str;
}

//only capitalise first letter
std::string strCapitalizeWord(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
								[](unsigned char c) { return std::tolower(c); });
	str[0] = std::toupper(str[0]);
	return str;
}

std::string strCapitalizeSentence(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
										[](unsigned char c) { return std::tolower(c); });
	str[0] = std::toupper(str[0]);
	for (int i = 0; i < str.length(); i++) {
		if (str[i] == '.' || str[i] == '?' || str[i] == '!') {
			if (i + 2 < str.length()) {
				str[i + 2] = std::toupper(str[i + 2]);
			}
		}
	}
	return str;
}

std::string strCapitalizeEveryWord(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
												[](unsigned char c) { return std::tolower(c); });
	str[0] = std::toupper(str[0]);
	for (int i = 0; i < str.length(); i++) {
		if (str[i] == ' ') {
			if (i + 1 < str.length()) {
				str[i + 1] = std::toupper(str[i + 1]);
			}
		}
	}
	return str;
}

bool saveString(std::string& s, ofstream& fout) {
	/*int size = s.size();
	fout.write((char*)(&size), sizeof(int));
	fout.write(s.c_str(), size);
	return true;*/
	//GPT
	if (fout.is_open()) {
		// Writing the size of the string first
		size_t size = s.size();
		/*if (size == 0) {
			std::cerr << "Error: String size is 0\n";
			return false;
		}
		if (size > 10000) {
			std::cerr << "Error: String size is too big\n";
			return false;
		}*/
		fout.write(reinterpret_cast<const char*>(&size), sizeof(size_t));

		// Writing the string data
		fout.write(s.data(), size);
		return true;
	}
	else {
		std::cerr << "Error: Unable to open file for writing: \n";
		return false;
	}
}

bool loadString(std::string& s, ifstream& fin) {
	/*int size;
	fin.read((char*)(&size), sizeof(int));
	printf("Size of string: %d\n", size);
	char* buffer = new char[size];
	fin.read(buffer, size);
	s = std::string(buffer, size);
	delete[] buffer;
	return true;*/
	//GPT
	if (fin.is_open()) {
		// Reading the size of the string
		size_t size;
		fin.read(reinterpret_cast<char*>(&size), sizeof(size_t));
		printf("string loaded size: %d\n", size);
		// Reading the string data
		if (size == 0) {
			s = "";
			return true;
		}
		std::string str(size, '\0');
		fin.read(&str[0], size);

		s = str;
		return true;
	}
	else {
		std::cerr << "Error: Unable to open file for reading: \s";
		return false;
	}
}


std::vector<std::string> getNextCSVLineSplit(std::istream& str) {
	std::vector<std::string> entries;
	std::string line;
	std::getline(str, line);

	std::stringstream lineStream(line);
	std::string cell;

	while (std::getline(lineStream, cell, ',')) {
		entries.push_back(cell);
	}
	if (!lineStream && cell.empty()) {
		entries.push_back("");
	}
	return entries;
}

ofColor colourConverter(const char* hex) {
	int r, g, b;
	sscanf(hex, "%02x%02x%02x", &r, &g, &b);
	return ofColor(r, g, b);
}


//https://stackoverflow.com/questions/306533/how-do-i-get-a-list-of-files-in-a-directory-in-c
//FORCE CONVERT TO ASCII FOR WINDOWS
/* Returns a list of files in a directory (except the ones that begin with a dot) */

void winfileutil::GetFilesInDirectory(std::vector<string>& out, string directory)
{

	HANDLE dir;
	WIN32_FIND_DATAA file_data;

	if ((dir = FindFirstFileA((directory + "/*").c_str(), &file_data)) == INVALID_HANDLE_VALUE) {
		printf("WANRING NO FILES FOUND IN: %s\n", directory.c_str());
		return; /* No files found */
	}

	do {
		const string file_name = file_data.cFileName;
		const string full_file_name = directory + "/" + file_name;
		const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (file_name[0] == '.')
			continue;

		if (is_directory)
			continue;

		out.push_back(full_file_name);
	} while (FindNextFileA(dir, &file_data));

	FindClose(dir);
	/* linux
			DIR* dir;
			class dirent* ent;
			class stat st;

			dir = opendir(directory);
			while ((ent = readdir(dir)) != NULL) {
				const string file_name = ent->d_name;
				const string full_file_name = directory + "/" + file_name;

				if (file_name[0] == '.')
					continue;

				if (stat(full_file_name.c_str(), &st) == -1)
					continue;

				const bool is_directory = (st.st_mode & S_IFDIR) != 0;

				if (is_directory)
					continue;

				out.push_back(full_file_name);
			}
			closedir(dir);
	*/
} // GetFilesInDirectory


