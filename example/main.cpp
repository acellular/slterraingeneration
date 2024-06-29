#include <stdio.h>
#include <iostream>
#include "../slterrain.h"
#include "mapoutput.h"
#include "configloader.h"

void saveTerrainToBitmap(SLTerrain& terrain, SLTerrain::TerrainParams& terrainParams) {
    BmpOutput mp; // save terrain to bitmaps

    int seed = terrain.getFBMParams().seed;
    std::string fileSuffix = "-seed" + std::to_string(seed) + ".bmp";

    // terrain types map
    ColorCategories<SLTerrain::TerrainType> terrainColorCategories;
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::GRASSLAND, SLColor("6aa961"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::FOREST, SLColor("264d42"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::VALLEY, SLColor("337b55"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::MOUNTAIN, SLColor("909294"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::GLACIER, SLColor("eef5ff"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::PLATEAU, SLColor("6f5d53"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::STANDING_WATER, SLColor("3b6dca"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::RIVER, SLColor("2f5eb5"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::IRON, SLColor("b3744e"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::COAL, SLColor("151564"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::STONE, SLColor("989aa5"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::BOG_IRON, SLColor("547d83"));
    terrainColorCategories.addColorRangeCenter(SLTerrain::TerrainType::URANIUM, SLColor("51ff4f"));
    mp.saveTerrainAsBitMap(terrain.getTerrainTypes(), terrainColorCategories, "terrainTypes" + fileSuffix, CGT_EXACT);

    // height map
    ColorCategories<float> heightColorCategories;
    heightColorCategories.addColorRangeCenter(0, SLColor(0));
    heightColorCategories.addColorRangeCenter(100, SLColor(255));
    mp.saveTerrainAsBitMap(terrain.getHydro().getHeightMap(), heightColorCategories, "heightmap" + fileSuffix, CGT_GRADATED);

    // height map filled
    ColorCategories<float> heightFilledColorCategories;
    heightFilledColorCategories.addColorRangeCenter(0, SLColor(0));
    heightFilledColorCategories.addColorRangeCenter(100, SLColor(255));
    mp.saveTerrainAsBitMap(terrain.getHydro().getHeightMapFilled(), heightFilledColorCategories, "heightmapFilled" + fileSuffix, CGT_GRADATED);

    // flow accumulation map
    ColorCategories<uint64_t> flowColorCategories;
    flowColorCategories.addColorRangeCenter(128, SLColor("222034"));
    flowColorCategories.addColorRangeCenter(32, SLColor("3365ff"));
    flowColorCategories.addColorRangeCenter(8, SLColor("ffcb55"));
    flowColorCategories.addColorRangeCenter(4, SLColor("fb802d"));
    flowColorCategories.addColorRangeCenter(1, SLColor("f9160e"));
    mp.saveTerrainAsBitMap(terrain.getHydro().getFlowAccumulation(), flowColorCategories, "flow" + fileSuffix, CGT_GRADATED);

    // USPED (erosion and deposition) map
    ColorCategories<float> uspedColorCategories;
    uspedColorCategories.addColorRangeCenter(-10000, SLColor(0));
    uspedColorCategories.addColorRangeCenter(-2000, SLColor("f9160e"));
    uspedColorCategories.addColorRangeCenter(0, SLColor("ffcb55"));
    uspedColorCategories.addColorRangeCenter(2000, SLColor("3365ff"));
    uspedColorCategories.addColorRangeCenter(10000, SLColor(255));
    mp.saveTerrainAsBitMap(terrain.getHydro().getErosionDeposition(), uspedColorCategories, "USPED" + fileSuffix, CGT_GRADATED);

    // aspect overlay map --> used here to create shadows and highlights
    std::vector<std::vector<float>>& slope = terrain.getHydro().getSlope();
    std::vector<std::vector<float>>& aspect = terrain.getHydro().getAspect();
    std::vector<std::vector<float>> opacity(slope.size(), std::vector<float>(slope[0].size(), 0));
    for (int i = 0; i < slope.size(); i++) {
        for (int j = 0; j < slope[i].size(); j++) {
            opacity[i][j] = (round(slope[i][j] / 15)) / 2.0;
            if (opacity[i][j] > 1) {
                opacity[i][j] = 1;
            }

            if (aspect[i][j] <= -1) { opacity[i][j] = 0; }
            else if (aspect[i][j] <= 0 || aspect[i][j] > 315) { opacity[i][j] *= -0.9; }
            else if (aspect[i][j] <= 45) { opacity[i][j] *= 0.8; }
            else if (aspect[i][j] <= 90) { opacity[i][j] *= 0.9; }
            else if (aspect[i][j] <= 135) { opacity[i][j] *= 1; }
            else if (aspect[i][j] <= 180) { opacity[i][j] *= 0.9; }
            else if (aspect[i][j] <= 225) { opacity[i][j] *= -0.9; }
            else if (aspect[i][j] <= 270) { opacity[i][j] *= -1; }
            else if (aspect[i][j] <= 315) { opacity[i][j] *= -1; }
            else { opacity[i][j] = 0; }
        }
    }
    ColorCategories<float> aspectCategories;
    aspectCategories.addColorRangeCenter(1, SLColor("fff0be", 100));
    aspectCategories.addColorRangeCenter(0, SLColor("aca39d", 0));
    aspectCategories.addColorRangeCenter(-0.3, SLColor("3c3939", 90));
    aspectCategories.addColorRangeCenter(-1, SLColor("141928", 180));

    // add aspect to terraintypes using alpha to get a better visualisation
    auto& terrainTypes = terrain.getTerrainTypes();
    std::vector<std::vector<SLColor>> fullColorMatrix(terrainParams.height, std::vector<SLColor>(terrainParams.width, SLColor(0)));
    for (int i = 0; i < terrainParams.height; i++) {
        for (int j = 0; j < terrainParams.width; j++) {
            fullColorMatrix[i][j] = terrainColorCategories.getColorExact(terrainTypes[i][j]);
            fullColorMatrix[i][j] = fullColorMatrix[i][j].blendUsingAlpha(aspectCategories.getColorGradated(opacity[i][j]));
        }
    }
    mp.saveColorMatrix(fullColorMatrix, "terrain" + fileSuffix);
}

int main() {
    SLRng::init();

    // load config
    ConfigLoader config;
    config.load();

    SLTerrain::FBMParams fbmParams;
    fbmParams.seed = config["seed"];
    fbmParams.octaves = config["octaves"];
    fbmParams.offsetX = config["offsetX"];
    fbmParams.offsetY = config["offsetY"];
    fbmParams.scale = config["scale"];
    fbmParams.lacunarity = config["lacunarity"];
    fbmParams.H = config["H"];
    fbmParams.frequency = config["frequency"];
    fbmParams.amplitude = config["amplitude"];
    fbmParams.baseHeight = config["baseHeight"];
    fbmParams.heightMultiplier = config["heightMultiplier"];
    fbmParams.heightModifier = config["heightModifier"];
    fbmParams.heightExponent = config["heightExponent"];
    fbmParams.normalize = (int)config["normalizeBool"];

    SLTerrain::TerrainParams terrainParams;
    terrainParams.width = config["width"];
    terrainParams.height = config["height"];
    terrainParams.age = config["age"];
    terrainParams.useChannelErosion = (int)config["channelErosionBool"];
    terrainParams.addResources = (int)config["addResourcesBool"];
    terrainParams.USPEDminBlur = config["USPEDminBlur"];
    terrainParams.USPEDmaxBlur = config["USPEDmaxBlur"];
   

    SLHydrology::ErosionParams erosionParams;
    erosionParams.cellSize = config["cellSize"];
    erosionParams.prevailingRill = (int)config["prevailingRill"];
    erosionParams.C = config["C"];
    erosionParams.K = config["K"];
    erosionParams.R = config["R"];
    erosionParams.weightErosion = config["weightErosion"];
    erosionParams.converter = config["converter"];
    erosionParams.blurFlow = config["preBlurFlowAccumulaionBool"];
    erosionParams.strahlerThreshold = config["strahlerThreshold"];


    //generate terrain-----------------------------------------------------
    SLTerrain terrain;
    terrain.newMap(fbmParams, terrainParams, erosionParams);

    // let the user continue generation if desired
    while (true) {
        saveTerrainToBitmap(terrain, terrainParams);

        char yn;
        printf("-->Generation COMPLETE. Bitmaps SAVED.\nContinue generation for another 100 'years' (iterations)? (Y / N) : ");
        do {
            yn = getchar();
        } while (yn == '\n');

        if (yn != 'Y' && yn != 'y') {
            printf("Exiting program.");
            return 0;
        }

        // continue iterating terrain
        for (int i = 0; i < 20; i++) {
            terrain.processYearFast();
            terrain.processRiversAndLakes();
            terrain.calculateTerrainTypes();
        }
        for (int i = 0; i < 5; i++) {
            terrain.processYear(i);
            terrain.calculateTerrainTypes();
        }
    }
};