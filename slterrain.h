#pragma once

#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <string>
#include "utils/slMath.h"
#include "slhydrology.h"
using namespace SLMath;

// SLTerrain------------------------------------------------------------------------------------
// Author: Michael Tuck, acellular.itch.io, contact@michaeltuck.com, 2024
// MIT LICENCE
// 
// Terrain generator class for creating and managing terrain.
// Uses a fractal brownian motion generator to create terrain.
// Uses a SLHydrology to simulate water flow and erosion.
// 
// Unlike SLHydrology, which is based on standard geospatial and GIS
// analysis techniques (flow accumulation and USPED),
// this class uses those techniques along with standard game dev
// techniques like fractional brownian motion (layered Perlin noise),
// with basic terrain types, resources and cellular automata for wildfires.
class SLTerrain {
public:
	struct FBMParams {
		int seed = 0; //0 for random
		int octaves = 5;
		int offsetX = 0; //0 for random
		int offsetY = 0; //0 for random
		float scale = 150;
		float lacunarity = 2;
		float H = 1;
		float frequency = 0.5;
		float amplitude = 0.5;
		float baseHeight = 0;
		float heightMultiplier = 5;
		float heightModifier = 0;
		float heightExponent = 4;
		bool normalize = true;
	};
	struct TerrainParams {
		int width = 500;
		int height = 500;
		int age = 20;
		bool useChannelErosion = false;
		bool addResources = false;
		int USPEDminBlur = 8; //minimum rnd blur radius for USPED erosion(used to avoid artifacts)
		int USPEDmaxBlur = 30; //maximum rnd blur radius for USPED erosion
	};
	
	// designed with a tile-based city-building or 4x game in mind
	enum TerrainType {
		GRASSLAND,
		FOREST,
		VALLEY,
		MOUNTAIN,
		GLACIER,
		PLATEAU,
		STANDING_WATER,
		RIVER,
		IRON, // simple resources as a terrain type
		COAL,
		STONE,
		BOG_IRON,
		URANIUM
	};

	// main methods
	void setup(); // only necessary if not using newMap (e.g. using load or manually passing in heightmap)

	// for map generation using FBM and USPED erosion model
	void newMap() { newMap(_fbm, _ter, _hydro.getErosionParams()); }
	void newMap(FBMParams fbmParams, TerrainParams terrainParams, SLHydrology::ErosionParams erosionParams);
	void processYear(int year);
	void processYearFast();
	void processRiversAndLakes();
	void save(std::ofstream& fout);
	void load(std::ifstream& fin);

	// terrain generation
	StdVec2Df FBMGenerator(FBMParams fbmParams, int rows, int cols); //TODO--generalize to SLMath??
	void blurAndOffsetUSPEDErosion();
	void calculateTerrainTypes();
	void additionalErosionDeposition();
	void adjustHeightsViaErosionDeposition();
	void addRndResouceDeposits();

	// allows objects using terrain to cause forest fires
	void wildfire(int x, int y, int iterations);
	void rndWildfire(int iterations);

	// setters
	void setFBMParams(FBMParams params) { _fbm = params; }
	void setTerrainParams(TerrainParams ter) { _ter = ter; }
	SLHydrology::ErosionParams getErosionParams() { return _hydro.getErosionParams(); }
	
	// getters
	int getCols() { return _hydro.getCols(); }
	int getRows() { return _hydro.getRows(); }
	int getBurned(int x, int y) { return _burned[y][x]; }
	StdVec2Di& getBurned() { return _burned; }
	std::vector<std::vector<TerrainType>>& getTerrainTypes() { return _terrainType; }
	FBMParams getFBMParams() { return _fbm; }
	SLHydrology& getHydro() { return _hydro; }

	// params
	TerrainParams getTerrainParams() { return _ter; }
	void setErosionParams(SLHydrology::ErosionParams ero) { _hydro.setErosionParams(ero); }

	std::string getTerrainTypeName(TerrainType type) {
		switch (type) {
		case GRASSLAND: return "Grassland";
		case FOREST: return "Forest";
		case VALLEY: return "Valley";
		case MOUNTAIN: return "Mountain";
		case PLATEAU: return "Plateau";
		case STANDING_WATER: return "Standing Water";
		case RIVER: return "River";
		case GLACIER: return "Glacier";
		case IRON: return "Iron";
		case COAL: return "Coal";
		case STONE: return "Stone";
		case BOG_IRON: return "Bog Iron";
		case URANIUM: return "Uranium";
		default: return "Unknown";
		}
	}

private:
	// parameters
	FBMParams _fbm;
	TerrainParams _ter;
	SLHydrology _hydro; // for erosion processing using USPED model

	// all matrices are [y][x] for consistency with i, j notation (i.e i = y and x = j)
	StdVec2Di _burned; // number represents burn iteration
	std::vector<std::vector<TerrainType>> _terrainType;

	// resources
	std::vector<SLPoint> _ironDeposits;
	std::vector<SLPoint> _coalDeposits;
	std::vector<SLPoint> _bogIronDeposits;
	std::vector<SLPoint> _stoneDeposits;
	std::vector<SLPoint> _uraniumDeposits;

	// custom Cfactor (cover factor) for each terrain type
	// used in SLHydrology for calculating erosion/deposition
	StdVec2Df _Cfactor;
	void calcCfactorFromTerrainTypes() {
		int rows = getRows();
		int cols = getCols();
		std::vector<std::vector<float>> C(rows, std::vector<float>(cols, 0.5));
		for (int i = 0; i < rows; i++) {
			for (int j = 0; j < cols; j++) {
				switch (_terrainType[i][j]) {
				case GRASSLAND:
					C[i][j] = 0.6;
					break;
				case FOREST:
					C[i][j] = 0.3;
					break;
				case VALLEY:
					C[i][j] = 0.6;
					break;
				case MOUNTAIN:
					C[i][j] = 0.5;
					break;
				case GLACIER:
					C[i][j] = 0.5;
					break;
				case PLATEAU:
					C[i][j] = 0.7;
					break;
				case STANDING_WATER:
					C[i][j] = 0.4;
					break;
				case RIVER:
					C[i][j] = 0.5;
				}
			}
		}
		_Cfactor = C;
	}
	
	void wildfireIteration(StdVec2Di& newBurned, int iteration);
};

