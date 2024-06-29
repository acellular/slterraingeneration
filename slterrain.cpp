#include "slTerrain.h"
#include <unordered_set>
#include <queue>

// for initialization (not required if using the newMap function)
void SLTerrain::setup() {
    int rows = getRows();
    int cols = getCols();

    _terrainType.assign(rows, std::vector<TerrainType>(cols, GRASSLAND));
    _burned.assign(rows, std::vector<int>(cols, false));
    _Cfactor.assign(rows, std::vector<float>(cols, _hydro.getErosionParams().C));
}

// for map generation using FBM and USPED erosion model
void SLTerrain::newMap(FBMParams fbmParams, TerrainParams terrainParams, SLHydrology::ErosionParams erosionParams) {
    _fbm = fbmParams;
    _ter = terrainParams;

    int rows = terrainParams.height;
    int cols = terrainParams.width;

    if (fbmParams.seed == 0) {
        _fbm.seed = SLRng::getInt(0, 100000);
        printf("Generation seed: %d\n", _fbm.seed);
	}
    SLRng::init(_fbm.seed);


    auto fbmZ = FBMGenerator(_fbm, rows, cols);
    _hydro = SLHydrology(fbmZ, erosionParams);
    
    setup();

    // extra large gradations across the map to enouragge flow and prevent massive lakes
    auto& z = _hydro.getHeightMap();
    PerlinNoise2D pNoise;
    int offsetX = _fbm.offsetX;
    int offsetY = _fbm.offsetY;
    if (offsetX == 0) {
        offsetX = SLRng::getInt(0, 100000);
    }
    if (offsetY == 0)
        offsetY = SLRng::getInt(0, 100000); {
    }
    float scale = 1000;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float sampleX = (j + offsetX) / scale;
            float sampleY = (i + offsetY) / scale;
            z[i][j] = z[i][j] + pNoise.noise(sampleX, sampleY) * 20;
        }
    }

    // initial fast generation, skipping channels and terraintype categorization
    if (_ter.age < 10) { _ter.age = 10; }
    for (int i = 0; i < _ter.age / 5; i++) {
        _hydro.calculateSlope(SLHydrology::DEGREE); //UPSED uses DEGREE
        _hydro.calculateDirection8();
        _hydro.calculateAspect(SLHydrology::DEGREE);
        printf("Slope and direction calculated\n");

        _hydro.calculateFlowAccumulation();
        printf("Flow accumulation calculated\n");

        _hydro.USPED(5); //multiply erosion/deposition by 5 for faster initial generation at cost of more noise artifacts
        blurAndOffsetUSPEDErosion();
        adjustHeightsViaErosionDeposition();
        printf("USPED erosion and deposition calculated\n");
    }
    _hydro.calculateStrahlerOrder();
    _hydro.identifyChannelsByStrahler(3);

    for (int i = 0; i < 20; i++) {
        processYear(i);
        calculateTerrainTypes();
	}

    // uses terrain types to add resources
    if (_ter.addResources) {
		addRndResouceDeposits();
	}
}

// terrain generation iteration, a "year" (or say, a turn in a game)
void SLTerrain::processYear(int year) {
    int rows = getRows();
    int cols = getCols();

    _burned.assign(rows, std::vector<int>(cols, 0));
    for (int i = 0; i < 4; i++) {//TODO param for num rnd wildfires a year-->or maybe actually a variable on this function?
        rndWildfire(6);
	}
    //_hydro.basicFillSinksPinholesMin();
    _hydro.calculateSlope(SLHydrology::DEGREE);// UPSED uses degree
    _hydro.calculateDirection8();
    _hydro.calculateAspect(SLHydrology::DEGREE);
    printf("Slope and direction calculated\n");

    _hydro.calculateFlowAccumulation();
    _hydro.blurFlowAccumulation();
    printf("Flow Accumulation Calculated\n");
    
    // run erosion and deposition on basic, unfilled terrain
    calcCfactorFromTerrainTypes();
    _hydro.USPED(1, &_Cfactor);
    blurAndOffsetUSPEDErosion();
    additionalErosionDeposition();
    adjustHeightsViaErosionDeposition();
    printf("USPED erosion and deposition calculated\n");

    // fill sinks and recalculate flow accumulation to create info for channels
    _hydro.fillSinksWangLiu(0.00001);
    _hydro.calculateDirection8(true);
    _hydro.calculateFlowAccumulation();
    printf("Filled Flow Accumulation Calculated\n");
    _hydro.fillSinksWangLiu(0);
    _hydro.calculateStrahlerOrder();
    _hydro.calculateDirection8(true); // needs to be run one final time to get flats for categorizing lakes
    printf("Strahler Order Calculated\n");

    _hydro.identifyChannelsByStrahler(3);
}

// terrain generation iteration, a "year" (or say, a turn in a game)
// simplified version of processYear, skips wildfires, lake and channel identification
// and has 5x faster erosion/deposition
void SLTerrain::processYearFast() {
    int rows = getRows();
    int cols = getCols();

    _hydro.calculateSlope(SLHydrology::DEGREE); //UPSED uses DEGREE
    _hydro.calculateDirection8();
    _hydro.calculateAspect(SLHydrology::DEGREE);
    printf("Slope and direction calculated\n");

    _hydro.calculateFlowAccumulation();
    printf("Flow accumulation calculated\n");

    calcCfactorFromTerrainTypes();
    _hydro.USPED(5, &_Cfactor);
    blurAndOffsetUSPEDErosion();
    adjustHeightsViaErosionDeposition();
    printf("USPED erosion and deposition calculated\n");
}

// identify rivers and create flats for lakes using fill sinks
void SLTerrain::processRiversAndLakes() {
    int rows = getRows();
    int cols = getCols();

    // fill sinks and recalculate flow accumulation to create info for channels
    _hydro.fillSinksWangLiu(0.00001);
    _hydro.calculateDirection8(true);
    _hydro.calculateFlowAccumulation();
    printf("Filled Flow Accumulation Calculated\n");
    _hydro.fillSinksWangLiu(0);
    _hydro.calculateStrahlerOrder();
    _hydro.calculateDirection8(true); // needs to be run one final time to get flats for categorizing lakes
    printf("Strahler Order Calculated\n");
    // identify channels by strahler order
    _hydro.identifyChannelsByStrahler(3);
}




void SLTerrain::save(std::ofstream& fout) {
    saveMatrix(_hydro.getHeightMap(), fout);
    saveMatrix(_hydro.getHeightMapFilled(), fout);
    saveMatrix(_hydro.getFlowAccumulation(), fout);
    saveMatrix(_hydro.getFlowDirection(), fout);
    saveMatrix(_hydro.getFlowDirectionIn(), fout);
    saveMatrix(_hydro.getSlope(), fout);
    saveMatrix(_hydro.getAspect(), fout);
    saveMatrix(_hydro.getStrahlerOrder(), fout);
    saveMatrix(_terrainType, fout);
    saveMatrix(_hydro.getErosionDeposition(), fout);
    saveMatrix(_burned, fout);

    saveVector(_ironDeposits, fout);
    saveVector(_coalDeposits, fout);
    saveVector(_bogIronDeposits, fout);
    saveVector(_stoneDeposits, fout);
    saveVector(_uraniumDeposits, fout);

    fout.close();
}

void SLTerrain::load(std::ifstream& fin) {
    loadMatrix(_hydro.getHeightMap(), fin);
    loadMatrix(_hydro.getHeightMapFilled(), fin);
    loadMatrix(_hydro.getFlowAccumulation(), fin);
    loadMatrix(_hydro.getFlowDirection(), fin);
    loadMatrix(_hydro.getFlowDirectionIn(), fin);
    loadMatrix(_hydro.getSlope(), fin);
    loadMatrix(_hydro.getAspect(), fin);
    loadMatrix(_hydro.getStrahlerOrder(), fin);
    loadMatrix(_terrainType, fin);
    loadMatrix(_hydro.getErosionDeposition(), fin);
    loadMatrix(_burned, fin);

    loadVector(_ironDeposits, fin);
    loadVector(_coalDeposits, fin);
    loadVector(_bogIronDeposits, fin);
    loadVector(_stoneDeposits, fin);
    loadVector(_uraniumDeposits, fin);

    int rows = getRows();
    int cols = getCols();//TODO error checking

    //regens
    _hydro.blurFlowAccumulation();
    _hydro.identifyChannelsByStrahler(3);
}

// simple random placement according to terrain type
void SLTerrain::addRndResouceDeposits() {
    int rows = getRows();
    int cols = getCols();
    auto& slope = _hydro.getSlope();

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (_terrainType[i][j] == MOUNTAIN || _terrainType[i][j] == PLATEAU || slope[i][j] > 35) {
                if (SLRng::getFloat(0, 1) < 0.01) {
                    _ironDeposits.push_back({ j, i });
                }
                else if (SLRng::getFloat(0, 1) < 0.01) {
                    _coalDeposits.push_back({ j, i });
                }
                else if (SLRng::getFloat(0, 1) < 0.0007) {
                    _uraniumDeposits.push_back({ j, i });
                }
			}
            if (_terrainType[i][j] == VALLEY) {
                if (SLRng::getFloat(0, 1) < 0.005) {
                    _bogIronDeposits.push_back({ j, i });
                }
            }
            else if (_terrainType[i][j] != MOUNTAIN && SLRng::getFloat(0, 1) < 0.01) {
                _stoneDeposits.push_back({ j, i });
            }
        }
    }
}

// based on flow, slope and elevation
void SLTerrain::calculateTerrainTypes() {
    int rows = getRows();
    int cols = getCols();
    auto& z = _hydro.getHeightMap();
    auto& slope = _hydro.getSlope();
    auto& isChannel = _hydro.getIsChannel();
    auto& flowDirection = _hydro.getFlowDirection();
    auto& flowAccumulation = _hydro.getFlowAccumulation();
    auto& blurredFlowAccumulation = _hydro.getBlurredFlowAccumulation();

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (flowDirection[i][j] == 0) { //(_z[i][j] < 30) { //original just used min elevation
                _terrainType[i][j] = STANDING_WATER;
                continue;
            }
            if (isChannel[i][j]) {
                _terrainType[i][j] = RIVER;
                continue;
            }

            // only remnove forests if water
            if (_terrainType[i][j] == FOREST && isChannel[i][j] == false && blurredFlowAccumulation[i][j] > 2) {
				continue;
			}

            // instead of relying on flow direction could maybe mark areas as standing water in fill sinks??
            if (z[i][j] > 72 && flowDirection[i][j] == 0) {
                _terrainType[i][j] = GLACIER;
            }
            else if (z[i][j] + 10/flowAccumulation[i][j] > 85) {
				_terrainType[i][j] = GLACIER;
			}
            else if (z[i][j] + 10/flowAccumulation[i][j] > 72) {
				_terrainType[i][j] = MOUNTAIN;
			}
            else if (z[i][j] > 45 && slope[i][j] < 15) {
				_terrainType[i][j] = PLATEAU;
			}
            else if (z[i][j] < 45 && slope[i][j] < 10 && blurredFlowAccumulation[i][j] > 10) {
                _terrainType[i][j] = VALLEY;
            }
            else if (flowAccumulation[i][j] > 5 && _burned[i][j] < 1 && SLRng::getFloat(0,1) < 0.25) {
				_terrainType[i][j] = FOREST;
			}
            else {
                _terrainType[i][j] = GRASSLAND;
            }
        }
    }

    //loop resource desposits and make sure to add back in 
    for (auto& deposit : _ironDeposits) {
        if (_terrainType[deposit.y][deposit.x] != STANDING_WATER) {
            _terrainType[deposit.y][deposit.x] = IRON;
        }
	}
    for (auto& deposit : _coalDeposits) {
        if (_terrainType[deposit.y][deposit.x] != STANDING_WATER) {
            _terrainType[deposit.y][deposit.x] = COAL;
        }
    }
    for (auto& deposit : _bogIronDeposits) {
        if (_terrainType[deposit.y][deposit.x] != STANDING_WATER) {
            _terrainType[deposit.y][deposit.x] = BOG_IRON;
        }
	}
    for (auto& deposit : _stoneDeposits) {
        if (_terrainType[deposit.y][deposit.x] != STANDING_WATER) {
            _terrainType[deposit.y][deposit.x] = STONE;
        }
	}
    for (auto& deposit : _uraniumDeposits) {
        if (_terrainType[deposit.y][deposit.x] != STANDING_WATER) {
            _terrainType[deposit.y][deposit.x] = URANIUM;
        }
    }
}

// Fractional Brownian Motion (FBM) generator
// NOTE-->will overwrite the exisiting heightmap
StdVec2Df SLTerrain::FBMGenerator(FBMParams fbmParams, int rows, int cols) {
    if (rows == 0 || cols == 0) {
        printf("::::ERROR:::: FBMGenerator> rows or cols = 0\n");
        return StdVec2Df(0);
    }

    int offsetX = fbmParams.offsetX;
    int offsetY = fbmParams.offsetY;
    if (offsetX == 0) {
        offsetX = SLRng::getInt(0, 100000);
    }
    if (offsetY == 0)
        offsetY = SLRng::getInt(0, 100000); {
    }

    PerlinNoise2D pNoise = PerlinNoise2D();

    StdVec2Df z(rows, std::vector<float>(cols, 0));

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            // calculate sample indices based on the coordinates, the scale and the offset
            float sampleX = (x + offsetX) / fbmParams.scale;
            float sampleY = (y + offsetY) / fbmParams.scale;

            //create gain from the Hurst exponent!
            //->lower the hurst the more volatile it becomes, when H = 1, G = .5, when H = 1/2, G = .7
            float gain = pow(2, -fbmParams.H);

            //calculate noise for this value
            float normalization = 0;
            float lacunarity = fbmParams.lacunarity;
            float amplitude = fbmParams.amplitude;
            float frequency = fbmParams.frequency;
            float noise = 0;

            for (int i = 0; i < fbmParams.octaves; i++)
            {
                // generate noise value using PerlinNoise for a given wave
                noise += amplitude * pNoise.noise(sampleX * frequency, sampleY * frequency);
                if (fbmParams.normalize) { normalization += amplitude; }
                frequency *= lacunarity;
                amplitude *= gain;
            }

            //normalize the noise value within 0 and 1
            if (fbmParams.normalize) { noise /= normalization; }
            z[y][x] = pow(noise * fbmParams.heightMultiplier, fbmParams.heightExponent) + fbmParams.heightModifier;
            if (fbmParams.baseHeight > 0 && fbmParams.baseHeight > z[y][x]) {
                z[y][x] = fbmParams.baseHeight;
            }
        }
    }
    return z;
}

// reduce deposition and blur errosion to adjust for artifacts from using simple D8 flow
void SLTerrain::blurAndOffsetUSPEDErosion() {
    int rows = getRows();
    int cols = getCols();
    auto& erosionDeposition = _hydro.getErosionDeposition();

    //gradually reduce borders
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (i < 1 || j < 1 || i > rows - 2 || j > cols - 2) {
                erosionDeposition[i][j] -= _hydro.getErosionParams().R * 0.1;
            }
        }
    }

    blur(erosionDeposition, SLRng::getFloat(_ter.USPEDminBlur, _ter.USPEDmaxBlur), 0.36);// make params?
}


// extra erosion and deposition, not based on hydrology.
// can help avoid some artifacts and make channels more consistent, but
// eventually breaks down in other ways, becoming ugly
void SLTerrain::additionalErosionDeposition() {
    // skip the rest if we're not using channel erosion
    if (!_ter.useChannelErosion) return;

    int rows = getRows();
    int cols = getCols();
    auto& slope = _hydro.getSlope();
    auto& z = _hydro.getHeightMap();
    auto& flowDirection = _hydro.getFlowDirection();
    auto& flowAccumulation = _hydro.getFlowAccumulation();
    auto& isChannel = _hydro.getIsChannel();
    auto& erosionDeposition = _hydro.getErosionDeposition();

    // increase deposition at edges of valleys
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (i < 1 || j < 1 || i > rows - 2 || j > cols - 2) {
                continue;
            }

            if (erosionDeposition[i][j] < 0) { continue; }

            float neighbourAvg = (slope[i - 1][j - 1] + slope[i - 1][j] + slope[i - 1][j + 1] +
                slope[i][j - 1] + slope[i][j] + slope[i][j + 1] +
                slope[i + 1][j - 1] + slope[i + 1][j] + slope[i + 1][j + 1]) / 9.0;

            float neighbourAvgZ = (z[i - 1][j - 1] + z[i - 1][j] + z[i - 1][j + 1] +
                z[i][j - 1] + z[i][j] + z[i][j + 1] +
                z[i + 1][j - 1] + z[i + 1][j] + z[i + 1][j + 1]) / 9.0;

            if (neighbourAvgZ > z[i][j]) {
                erosionDeposition[i][j] += erosionDeposition[i][j] * ((neighbourAvg + 1) / (slope[i][j] + 1)) * 0.2;
            }
        }
    }

    // alt idea using channels and direction of flow to build up banks
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (flowDirection[i][j] == 0) { //sink
                erosionDeposition[i][j] += (erosionDeposition[i][j] + 10) * .03 * 2;
			}
            //just skip edges
            if (i < 4 || j < 4 || i > rows - 4 || j > cols - 4) {
				continue;
			}
            if (erosionDeposition[i][j] < -10) {
				continue;
			}
            if (flowAccumulation[i][j] < 10) {
                continue;
            }
            

            float extraErosion = (erosionDeposition[i][j]+10)*.03;
            if (!isChannel[i][j]) {
                extraErosion * 4;
            }

            if (true) {
                switch (flowDirection[i][j]) {
                case 1: //NE
                    erosionDeposition[i - 1][j + 1] -= extraErosion * 2;
                    break;
                case 2: //E
                    erosionDeposition[i][j + 1] += extraErosion * 2;//centre
                    break;
                case 4: //SE
                    erosionDeposition[i + 1][j + 1] -= extraErosion * 2;//centre
                    break;
                case 8: //S
                    erosionDeposition[i + 1][j] -= extraErosion * 2;//centre
                    break;
                case 16: //SW
                    erosionDeposition[i + 1][j - 1] -= extraErosion * 2;//centre
                    break;
                case 32: //W
                    erosionDeposition[i][j - 1] -= extraErosion * 2;//centre
                    break;
                case 64: //NW
                    erosionDeposition[i - 1][j - 1] -= extraErosion * 2;//centre
                    break;
                case 128: //N
                    erosionDeposition[i - 1][j] -= extraErosion * 2;//centre
                    break;
                }
            }

            // trailing seprated for now so can turn off
            if (false) continue;
            switch (flowDirection[i][j]) {
            case 1: //NE
                erosionDeposition[i - 1][j - 2] += extraErosion;//left
                erosionDeposition[i + 2][j + 1] += extraErosion;//right
                break;
            case 2: //E
                erosionDeposition[i - 2][j-1] += extraErosion;//left
                erosionDeposition[i + 2][j-1] += extraErosion;//right
                break;
            case 4: //SE
                erosionDeposition[i - 2][j + 1] += extraErosion;//left
                erosionDeposition[i + 1][j - 2] += extraErosion;//right
                break;
            case 8: //S
                erosionDeposition[i - 1][j + 2] += extraErosion;//left
                erosionDeposition[i - 1][j - 2] += extraErosion;//right
                break;
            case 16: //SW
                erosionDeposition[i + 1][j + 2] += extraErosion;//left
                erosionDeposition[i - 2][j - 1] += extraErosion;//right
                break;
            case 32: //W
                erosionDeposition[i + 2][j + 1] += extraErosion;//left
                erosionDeposition[i - 2][j + 1] += extraErosion;//right
                break;
            case 64: //NW
                erosionDeposition[i + 2][j - 1] += extraErosion;//left
                erosionDeposition[i - 1][j + 2] += extraErosion;//right
                break;
            case 128: //N
                erosionDeposition[i + 1][j - 2] += extraErosion;//left
                erosionDeposition[i + 1][j + 2] += extraErosion;//right
                break;
            }
            if (false) continue;
            switch (flowDirection[i][j]) {
            case 1: //NE
                erosionDeposition[i - 1][j - 3] += extraErosion * 0.5;//left
                erosionDeposition[i + 3][j + 1] += extraErosion * 0.5;//right
                break;
            case 2: //E
                erosionDeposition[i - 3][j - 1] += extraErosion * 0.5;//left
                erosionDeposition[i + 3][j - 1] += extraErosion * 0.5;//right
                break;
            case 4: //SE
                erosionDeposition[i - 3][j + 1] += extraErosion * 0.5;//left
                erosionDeposition[i + 1][j - 3] += extraErosion * 0.5;//right
                break;
            case 8: //S
                erosionDeposition[i - 1][j + 3] += extraErosion * 0.5;//left
                erosionDeposition[i - 1][j - 3] += extraErosion * 0.5;//right
                break;
            case 16: //SW
                erosionDeposition[i + 1][j + 3] += extraErosion * 0.5;//left
                erosionDeposition[i - 3][j - 1] += extraErosion * 0.5;//right
                break;
            case 32: //W
                erosionDeposition[i + 3][j + 1] += extraErosion * 0.5;//left
                erosionDeposition[i - 3][j + 1] += extraErosion * 0.5;//right
                break;
            case 64: //NW
                erosionDeposition[i + 3][j - 1] += extraErosion * 0.5;//left
                erosionDeposition[i - 1][j + 3] += extraErosion * 0.5;//right
                break;
            case 128: //N
                erosionDeposition[i + 1][j - 3] += extraErosion * 0.5;//left
                erosionDeposition[i + 1][j + 3] += extraErosion * 0.5;//right
                break;
            }

            //add erosion behind
            if (false) continue;
            switch (flowDirection[i][j]) {
            case 1: //NE
                erosionDeposition[i + 1][j - 1] += extraErosion * 0.5;
                erosionDeposition[i + 2][j - 2] += extraErosion;
                break;
            case 2: //E
                erosionDeposition[i][j - 1] += extraErosion * 0.5;
                erosionDeposition[i][j - 2] += extraErosion;
                break;
            case 4: //SE
                erosionDeposition[i - 1][j - 1] += extraErosion * 0.5;
                erosionDeposition[i - 2][j - 2] += extraErosion;
                break;
            case 8: //S
                erosionDeposition[i - 1][j] += extraErosion * 0.5;
                erosionDeposition[i - 2][j] += extraErosion;
                break;
            case 16: //SW
                erosionDeposition[i - 1][j + 1] += extraErosion * 0.5;
                erosionDeposition[i - 2][j + 2] += extraErosion;
                break;
            case 32: //W
                erosionDeposition[i][j + 1] += extraErosion * 0.5;
                erosionDeposition[i][j + 2] += extraErosion;
                break;
            case 64: //NW
                erosionDeposition[i + 1][j + 1] += extraErosion * 0.5;
                erosionDeposition[i + 2][j + 2] += extraErosion;
                break;
            case 128: //N
                erosionDeposition[i + 1][j] += extraErosion * 0.5;
                erosionDeposition[i + 2][j] += extraErosion;
                break;
            }

        }
    }

    blur(erosionDeposition, 2, .3);
}

// add erosion/deposition to the height map based on an arbitrary multiplier "converter"
void SLTerrain::adjustHeightsViaErosionDeposition() {
    int rows = getRows();
    int cols = getCols();
    auto& z = _hydro.getHeightMap();
    auto& erosionDeposition = _hydro.getErosionDeposition();

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float toAdd = erosionDeposition[i][j] * _hydro.getErosionParams().converter * SLRng::getFloat(.75, 1);
            z[i][j] += toAdd * (0.5 + z[i][j] * 0.005);
        }
    }
}

// wildfires (a simple cellular automata)------------------------------------------------------------------------
void SLTerrain::rndWildfire(int iterations) {
    int rows = getRows();
    int cols = getCols();
    auto& flowAccumulation = _hydro.getFlowAccumulation();

    int x = SLRng::getFloat(iterations, cols- iterations); //so can skip bounds checks
    int y = SLRng::getFloat(iterations, rows- iterations);
    if (_terrainType[y][x] == MOUNTAIN || _terrainType[y][x] == STANDING_WATER || flowAccumulation[y][x] > 10) { 
        return; 
    }
    if (_burned[y][x] >= 1) { return; }

    wildfire(x, y, iterations);
}

void SLTerrain::wildfire(int x, int y, int iterations) {
    int rows = getRows();
    int cols = getCols();

    std::vector<std::vector<int>> newBurned(rows, std::vector<int>(cols, 0));

    // avoid edges
    if (x <= iterations) { x = iterations + 1; }
    else if (x > cols - iterations - 1) { x = cols - iterations - 2; }
    if (y <= iterations) { y = iterations + 1; }
    else if (y > rows - iterations - 1) { y = rows - iterations - 2; }

    newBurned[y][x] = 1;

    for (int i = 0; i < iterations; i++) {//should really be spreading step by step instead of cylcing whole matrix
        wildfireIteration(newBurned, i + 1);
    }

    //combined with current burned
    for (int i = 1; i < rows - 1; i++) {
        for (int j = 1; j < cols - 1; j++) {
            _burned[i][j] += newBurned[i][j];
        }
    }
}

// recursive wildfire spread (should really be spreading step by step instead of cylcing whole matrix)
void SLTerrain::wildfireIteration(StdVec2Di& newBurned, int iteration) {
    int rows = getRows();
    int cols = getCols();

    for (int i = 1; i < rows - 1; i++) {
        for (int j = 1; j < cols - 1; j++) {
            if (newBurned[i][j] < 1) {
                continue;
            }
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (newBurned[i + dy][j + dx] > 0) { continue; }
                    //already know in bounds becuase of iteration check
                    if (_terrainType[i + dy][j + dx] != GRASSLAND
                        && _terrainType[i + dy][j + dx] != FOREST
                        && _terrainType[i + dy][j + dx] != VALLEY) {
                        //|| flowAccumulation[j + dx][i + dy] > 20) {//rivers should be covered by this
                        continue;
                    }
                    if (SLRng::getFloat(0, 1) < 0.1 * (0.2 + (_terrainType[i][j] == GRASSLAND))) {
                        newBurned[i + dy][j + dx] += iteration;
                        if (_terrainType[i + dy][j + dx] == FOREST) {
                            _terrainType[i + dy][j + dx] = GRASSLAND;
                        }
                    }
                }
            }
        }
    }
}