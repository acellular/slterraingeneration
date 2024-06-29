#pragma once
#pragma warning(disable: 4146) //so we can use custom abs with unsigned types
#include "../slTerrain.h"
using namespace SLMath;

// can't be templated so global with prefixes
enum ColorGradientType {
	CGT_EXACT,
	CGT_CLOSEST,
	CGT_GRADATED
};

// because std library abs doesn't handle unsigned types
// sure there's a bitwise way that would be better
// but templates... only using it for ColorCategories
template<typename T>
T SLAbs(T number) {
	if (number < 0) {
		return -number;
	}
	return number;
}
// Template class for mapping values to colours.
// Works with number types (including enums).
// Can return exact colour for value, closest colour to value,
// or gradated colour between two closest values.
template<typename T>
class ColorCategories {

public:
	void addColorRangeCenter(T value, SLColor colour) {
		_colourMap[value] = colour;
		_isSorted = false;
	}
	void addColorRangeCenter(T value, float r, float g, float b, float a = 255) {
		_colourMap[value] = SLColor(r, g, b, a);
		_isSorted = false;
	}

	SLColor getColorExact(T value) {
		if (_colourMap.find(value) == _colourMap.end()) { return SLColor(0, 0, 0, 0); } //no value for key
		return _colourMap[value];
	}

	SLColor getColorClosest(T value) {
		T minValue = (T)0;
		for (auto& pair : _colourMap) {
			if (SLAbs(pair.first - value) < SLAbs(minValue - value)) {
				minValue = pair.first;
			}
		}
		return _colourMap[minValue];
	}

	SLColor getColorGradated(T value) {
		if (!_isSorted) {
			_values.clear();
			for (auto& pair : _colourMap) {
				_values.push_back(pair.first);
			}
			if (_values.size() < 2) {
				return _colourMap[(T)_values.back()];
			}
			std::sort(_values.begin(), _values.end());
			_isSorted = true;
		}

		// find closest values
		int closestValueIdx = 0;
		for (int i = 1; i < _values.size(); i++) {
			if (SLAbs(_values[i] - value) < SLAbs(_values[closestValueIdx] - value)) {
				closestValueIdx = i;
			}
		}

		// is value at front or back of range?
		bool isFront = closestValueIdx == _values.size() - 1 && value >= _values[closestValueIdx];
		bool isBack = closestValueIdx == 0 && value <= _values[closestValueIdx];
		if (isFront || isBack) {
			return _colourMap[(T)_values[closestValueIdx]];
		}

		// find distance to two closest values
		float closestValue = _values[closestValueIdx];
		float nextClosestValue = 0;
		if (closestValue < value) {
			nextClosestValue = _values[closestValueIdx + 1];
		}
		else {
			nextClosestValue = _values[closestValueIdx - 1];
		}
		float closestDist = SLAbs(closestValue - value);
		float nextClosestDist = SLAbs(nextClosestValue - value);

		// merge colours of two closest values
		SLColor closest = _colourMap[(T)closestValue];
		SLColor  nextClosest = _colourMap[(T)nextClosestValue];
		float sum = closestDist + nextClosestDist;
		if (sum == 0) { closestDist = 1; }
		else {
			closestDist = 1 - (closestDist / sum);
			nextClosestDist = 1 - (nextClosestDist / sum);
		}
		return SLColor(closest * closestDist + nextClosest * nextClosestDist);
	};

private:
	bool _isSorted = false;
	std::unordered_map<T, SLColor> _colourMap;

	std::vector<float> _values; // for sorted values in gradated
};


// Takes values and colour categories to output an bmp with
// colours that are an CGT_EXACT, CLOSESTS or CGT_GRADATED match with values.
class BmpOutput {
public:
	// takes matrix of number values and outputs them as a bitmap according to colour categories
	template<typename T>
	void saveTerrainAsBitMap(std::vector<std::vector<T>>& _values, ColorCategories<T>& colorCategories, std::string file, ColorGradientType type) {
		int h = _values.size();
		if (h == 0) { return; }
		int w = _values[0].size();
		if (w == 0) { return; }

		std::vector<std::vector<SLColor>> colorMatrix(_values.size(), std::vector<SLColor>(_values[0].size(), SLColor()));

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				SLColor c;
				switch (type) {
				case CGT_EXACT:
					c = colorCategories.getColorExact(_values[i][j]);
					break;
				case CGT_CLOSEST:
					c = colorCategories.getColorClosest(_values[i][j]);
					break;
				case CGT_GRADATED:
					c = colorCategories.getColorGradated(_values[i][j]);
					break;
				}
				colorMatrix[i][j] = c;
			}
		}
		saveColorMatrix(colorMatrix, file);
	}

	// save a matrix of RGB colours to a bitmap file (alpha ignored)
	void saveColorMatrix(std::vector<std::vector<SLColor>>& colorMatrix, std::string file) {
		// modified from https://stackoverflow.com/questions/2654480/writing-bmp-image-in-pure-c-c-without-other-libraries
		int h = colorMatrix.size();
		if (h == 0) { return; }
		int w = colorMatrix[0].size();
		if (w == 0) { return; }

		FILE* f;
		unsigned char* img = NULL;
		int filesize = 54 + 3 * w * h;

		img = (unsigned char*)malloc(3 * w * h);
		memset(img, 0, 3 * w * h);

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				int y = i;
				int x = j;

				SLColor c = colorMatrix[i][j];
				int r = c.r;
				int g = c.g;
				int b = c.b;
				
				img[(x + y * w) * 3 + 2] = (unsigned char)(r);
				img[(x + y * w) * 3 + 1] = (unsigned char)(g);
				img[(x + y * w) * 3 + 0] = (unsigned char)(b);
			}
		}

		unsigned char bmpfileheader[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0 };
		unsigned char bmpinfoheader[40] = { 40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0 };
		unsigned char bmppad[3] = { 0,0,0 };

		bmpfileheader[2] = (unsigned char)(filesize);
		bmpfileheader[3] = (unsigned char)(filesize >> 8);
		bmpfileheader[4] = (unsigned char)(filesize >> 16);
		bmpfileheader[5] = (unsigned char)(filesize >> 24);

		bmpinfoheader[4] = (unsigned char)(w);
		bmpinfoheader[5] = (unsigned char)(w >> 8);
		bmpinfoheader[6] = (unsigned char)(w >> 16);
		bmpinfoheader[7] = (unsigned char)(w >> 24);
		bmpinfoheader[8] = (unsigned char)(h);
		bmpinfoheader[9] = (unsigned char)(h >> 8);
		bmpinfoheader[10] = (unsigned char)(h >> 16);
		bmpinfoheader[11] = (unsigned char)(h >> 24);

		fopen_s(&f, file.c_str(), "wb");
		fwrite(bmpfileheader, 1, 14, f);
		fwrite(bmpinfoheader, 1, 40, f);
		for (int i = 0; i < h; i++) {
			fwrite(img + (w * (h - i - 1) * 3), 3, w, f);
			fwrite(bmppad, 1, (4 - (w * 3) % 4) % 4, f);
		}

		free(img);
		fclose(f);
	}
};

