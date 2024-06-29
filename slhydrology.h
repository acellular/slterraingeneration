#pragma once
#include "utils/slmath.h"
#include <unordered_map>

using namespace SLMath;
typedef std::vector<std::vector<float>> StdVec2Df; // convenience typedefs
typedef std::vector<std::vector<int>> StdVec2Di;


// SLHydrology------------------------------------------------------------------------------------
// Author: Michael Tuck, acellular.itch.io, contact@michaeltuck.com, 2024
// MIT LICENCE
// 
// Methods for generating basic geospatial data from heightmaps (_slope + _aspect)
// Methods for hydrological analysis: D8 flow accumulation, strahler order (channel categorization)
// and the USPED (Unit Stream power Erosion and Deposition) model
// Methods for heightmap modification: Wang and Liu fill sinks, along with basic blur and flatten.
// 
// USAGE: Pass a heightmap to the constructor, then call the methods you need.
// Analysis methods (slope, aspect, flow accumulation, strahler order, USPED) return a new matrix
// and store it in the object for later access.
// Analysis methods can also be called with a matrix to analyze, instead of the original heightmap,
// in this case the results are not stored in the object.
// 
// Modification methods (fill sinks, create lakes, adjust heights via USPED) modify the heightmap.
// Revert to the original heightmap with the revert() method.
// 
// -->all matrices are [y][x] for consistency with i, j notation (i.e i = y and x = j)
// [0][0] is top left for consistency with image formats, like bmp, and opengl (line by line, top down)
class SLHydrology {
public:
	struct ErosionParams {
		float cellSize = 100; //metres
		float C = 0.5;
		float K = 0.05;
		float R = 280;
		float converter = 0.00001;
		bool blurFlow = false;
		int prevailingRill = 2; //0 for no prevailing rill, 1 for prevailing rill, > 1 for alternating
		float weightErosion = 0; //increase erosion versus deposition to prevent continuous rise
		int strahlerThreshold = 1; //the strahler order under which skip considering for channel
	};
	enum AngleUnits { PERCENT, DEGREE, RADIAN };

	SLHydrology() {};
	SLHydrology(StdVec2Df& heightmap, ErosionParams erosionParams = ErosionParams())
		: _z(heightmap), _ero(erosionParams) {};

	// main methods
	void setup();
	void quickProcess(); // processes analysis in order to get to erosion (slope+aspect->direction8->flow->USPED)


	

	// GENERALIZED BASIC GEOSPATIAL ANALYSIS FUNCTIONS---------------------------------------------------
	// basic heightmap analysis
	void calculateDirection8(StdVec2Df& inHeightMap, StdVec2Di& outD8);
	void calculateSlope(StdVec2Df& inHeightMap, StdVec2Df& outSlope, AngleUnits slopeType = DEGREE);
	void calculateAspect(StdVec2Df& inHeightMap, StdVec2Df& outAspect, AngleUnits angleType = DEGREE);
	void calculateAspectAveraged(StdVec2Df& inMatrix, StdVec2Df& outAspect, SLHydrology::AngleUnits angleType);


	// LOCAL ANALYSIS OF PROVIDED HEIGHTMAP--------------------------------------------------------------
	// NOTE-->ORDER of analysis matters!
	// 1. slope + aspect
	// 2. calculateDirection8
	// 3. calculateFlowAccumulation
	// 4. calculateStrahlerOrder
	// 5. indentify channels +/or USPED (erosion and deposition)

	// flow accumulation on stored heightmap
	void calculateFlowAccumulation();
	void blurFlowAccumulation();//WHY WHEN CAN JUST BLUR MANUALLY???

	// basic analysis on stored heightmap
	void calculateSlope(AngleUnits slopeType = DEGREE) {
		calculateSlope(_z, _slope, slopeType);
	}
	void calculateAspect(AngleUnits angleType = DEGREE) {
		calculateAspect(_z, _aspect, angleType);
	}
	void calculateDirection8(bool useFilled = false) {
		auto& heightMap = useFilled ? _zFilled : _z;
		calculateDirection8(heightMap, _flowDirection);
	}

	// fill sinks and create lakes
	void fillSinksWangLiu(float minimumHeightDifferent);//TODO--generalize?

	// create channels (rivers)
	void calculateStrahlerOrder();
	void identifyChannelsByFlow(float threshold); 
	void identifyChannelsByStrahler(int threshold); // stahler order the academic standard

	// erosion--> uses constant values for C, K and R if no matrix provided
	// NOTE: previous steps should be calculated in DEGREE
	void USPED(float multiplier = 1, StdVec2Df* C = nullptr, StdVec2Df* K = nullptr, StdVec2Df* R = nullptr);


	// SIMPLE FAST PREPROCESSORS--------------------------------------------------------------------------
	// simple but fast pinhole fillers and flattener for preprocessing noisy heightmaps
	void basicFillSinksPinholesMin();
	void basicFillSinksPinholesAvg();
	void basicFlattenPeaks();
	//----------------------------------------------------------------------------------------------------


	//setters
	void setErosionParams(ErosionParams ero) { _ero = ero; }

	//getters
	int getCols() { return _z[0].size(); }
	int getRows() { return _z.size(); }
	ErosionParams getErosionParams() { return _ero; }

	StdVec2Df& getHeightMap() { return _z; }
	StdVec2Df& getHeightMapFilled() { return _zFilled; }
	std::vector<std::vector<uint64_t>>& getFlowAccumulation() { return _flowAccumulation; }
	StdVec2Df& getBlurredFlowAccumulation() { return _blurredFlowAccumulation; }
	StdVec2Di& getFlowDirection() { return _flowDirection; }
	StdVec2Di& getFlowDirectionIn() { return _flowDirectionIn; }
	StdVec2Df& getSlope() { return _slope; }
	StdVec2Df& getAspect() { return _aspect; }
	std::vector<std::vector<bool>>& getIsChannel() { return _isChannel; }
	StdVec2Di& getStrahlerOrder() { return _strahlerOrder; }
	StdVec2Df& getErosionDeposition() { return _erosionDeposition; }


private:
	// used by the flow accumulation algorithm, storing all
	// inbound directions in a single int that is decoded with bitwise operations
	void sumFlowDirectionsIn();

	// recursive internals
	int flowAccumRecurv(int i, int j); //recursive flow accumulation
	int strahlerStep(int i, int j, int flowAccumulationThreshold); //recursive strahler order calculation
	

	ErosionParams _ero;
	
	// terrain data matricies
	StdVec2Df _z;
	StdVec2Df _zFilled;

	std::vector<std::vector<uint64_t>> _flowAccumulation; //in case of extra large maps
	StdVec2Df _blurredFlowAccumulation;
	StdVec2Di _flowDirection;
	StdVec2Di _flowDirectionIn;
	StdVec2Df _slope;
	StdVec2Df _aspect;
	StdVec2Di _strahlerOrder;
	StdVec2Df _erosionDeposition;
	std::vector<std::vector<bool>> _isChannel;

	// old-school power of two encoding for D8 (direction 8) flow directions
	// follows Greenlee(1987) https://www.asprs.org/wp-content/uploads/pers/1987journal/oct/1987_oct_1383-1387.pdf
	// and Jenson& Domingue(1988) https ://www.asprs.org/wp-content/uploads/pers/1988journal/nov/1988_nov_1593-1600.pdf
	//  +------------+
	//	| 64  128  1 |
	//	| 32   C   2 |
	//	| 16   8   4 |
	//	+------------+
	// (NOTE: Arc prodducts use a slightly different encoding, clockwise starting from E instead of NE)
	float decodeDirectionToRadian(int d) {
		switch (d) {
		case -4: return 1.570797; //outlet N
		case -3: return 3.141594; //outlet W
		case -2: return 4.712391; //outlet S
		case -1: return 0; //outlet E
		case 0: return -1; //sink
		case 1: return 0.7853985;
		case 2: return 0;
		case 4: return 5.4977895;
		case 8: return 4.712391;
		case 16: return 3.9269925;
		case 32: return 3.141594;
		case 64: return 2.3561955;
		case 128: return 1.570797;
		}
	}
	float decodeDirectionToDegree(int d) {
		switch (d) {
		case -4: return 90; //outlet N
		case -3: return 180; //outlet W
		case -2: return 270; //outlet S
		case -1: return 0; //outlet E
		case 0: return -1;
		case 1: return 45;
		case 2: return 0;
		case 4: return 315;
		case 8: return 270;
		case 16: return 225;
		case 32: return 180;
		case 64: return 135;
		case 128: return 90;
		}
	}
	SLVec2i8 flowDirectionFromEncoded(int encodedDirection) {
		switch (encodedDirection) {
		case -4: return SLVec2i8(0, -1);//outlet N
		case -3: return SLVec2i8(-1, 0);//outlet W
		case -2: return SLVec2i8(0, 1);//outlet S
		case -1: return SLVec2i8(1, 0);//outlet E
		case 0: return SLVec2i8(0, 0);//sink
		case 1: return SLVec2i8(1, -1);
		case 2: return SLVec2i8(1, 0);
		case 4: return SLVec2i8(1, 1);
		case 8: return SLVec2i8(0, 1);
		case 16: return SLVec2i8(-1, 1);
		case 32: return SLVec2i8(-1, 0);
		case 64: return SLVec2i8(-1, -1);
		case 128: return SLVec2i8(0, -1);
		}
	}


	int encodeDirection(int dx, int dy) {
		// convert x and y components to corresponding power of 2 values
		int encodedValue = 0; //Should be for (0,0), but will return for any other improper value

		if (dx == 1) {
			if (dy == -1) encodedValue = 1;
			else if (dy == 0) encodedValue = 2;
			else if (dy == 1) encodedValue = 4;
		}
		else if (dx == 0) {
			if (dy == 1) encodedValue = 8;
			else if (dy == 0) encodedValue = 0; // sink
			else if (dy == -1) encodedValue = 128;
		}
		else if (dx == -1) {
			if (dy == 1) encodedValue = 16;
			else if (dy == 0) encodedValue = 32;
			else if (dy == -1) encodedValue = 64;
		}


		return encodedValue;
	}

	// alt using a map
	int flowDirectionToEncoded(int dx, int dy) {
		return EncodeD8Vec[SLVec2i8(dx, dy)];
	}
	std::unordered_map<SLVec2i8, int, SLVecHash<int8_t>> EncodeD8Vec{
		{SLVec2i8(0, 0), 0}, //sink
		{SLVec2i8(1, -1), 1},
		{SLVec2i8(1, 0), 2},
		{SLVec2i8(1, 1), 4},
		{SLVec2i8(0, 1), 8},
		{SLVec2i8(-1, 1), 16},
		{SLVec2i8(-1, 0), 32},
		{SLVec2i8(-1, -1), 64},
		{SLVec2i8(0, -1), 128}
	};
};

