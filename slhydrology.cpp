#include "slhydrology.h"
#include "utils/slmath.h"
#include <unordered_set>
#include <queue>

// init (unneeded if using processAll)
void SLHydrology::setup() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> hieghtmap size = 0\n");
        return;
    }
    int rows = _z.size();
    int cols = _z[0].size();

    // resets
    _flowAccumulation.resize(0);
    _blurredFlowAccumulation.resize(0);
    _flowDirection.resize(0);
    _flowDirectionIn.resize(0);
    _slope.resize(0);
    _aspect.resize(0);
    _isChannel.resize(0);
    _strahlerOrder.resize(0);
    _erosionDeposition.resize(0);
}

// processes all steps necessary to get flow and erosion
// (note: skips fill sinks so deposition settles at the bottom of 
// what would otherwise be fillled lakes. Most standard analysis
// pipelines would run fill sinks before flow accumulation.)
void SLHydrology::quickProcess() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: SLHydrology::processAll -> Matrix size = 0\n");;
        return;
    }

    //_hydro.basicFillSinksPinholesMin();
    calculateSlope(SLHydrology::DEGREE);// UPSED uses degree
    calculateDirection8();
    calculateAspect(SLHydrology::DEGREE);
    printf("Slope and direction calculated\n");

    calculateFlowAccumulation();
    blurFlowAccumulation();
    printf("Flow Accumulation Calculated\n");

    // run erosion and deposition on basic, unfilled terrain
    USPED(1); // multiplier
    printf("UPSED Calculated\n");
}


// GENERALIZED BASIC GEOSPATIAL ANALYSIS FUNCTIONS------------------------------------------------------------------
// 
// calculate slope in degrees or radians accroding to highest difference in 8 directions
void SLHydrology::calculateSlope(StdVec2Df& inMatrix, StdVec2Df& outSlope, AngleUnits slopeType) {
    if (inMatrix.size() == 0) {
        printf("::::ERROR:::: calculateSlope> inMatrix size = 0\n");
        return;
    }
    int rows = inMatrix.size();
    int cols = inMatrix[0].size();

    outSlope.assign(rows, std::vector<float>(cols, 0));

    for (int i = 0; i < outSlope.size(); i++) {
        for (int j = 0; j < outSlope[0].size(); j++) {
            float maxSlope = 0.0;
            //float minSlope = 0.0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        // for purposes of slope all cells also assumed to be cubes
                        // (i.e. height is assumed to be in the same units as width and height
                        // and can therefore be ignored here.)
                        //float heightDiff = inMatrix[i + dy][j + dx] - inMatrix[i][j]; // alt comparing slope out
                        float heightDiff = inMatrix[i][j] - inMatrix[i + dy][j + dx];
                        float distance = std::sqrt(dx * dx + dy * dy);
                        float currentSlope = heightDiff / distance;
                        maxSlope = std::max(maxSlope, currentSlope);
                    }
                }
            }
            outSlope[i][j] = maxSlope;            

            switch (slopeType) {
            case AngleUnits::DEGREE:
                outSlope[i][j] = std::atan(outSlope[i][j]) * 180 / 3.14159265359;
                break;
            case AngleUnits::RADIAN:
                outSlope[i][j] = std::atan(outSlope[i][j]);
                break;
            }
        }
    }
}

// simple direction8 converted to angleType from encoded number
void SLHydrology::calculateAspect(StdVec2Df& inMatrix, StdVec2Df& outAspect, AngleUnits angleType) {
    if (inMatrix.size() == 0) {
        printf("::::ERROR:::: calculateAspect-> inMatrix size = 0\n");
        return;
    }
    int rows = inMatrix.size();
    int cols = inMatrix[0].size();
    
    outAspect.assign(rows, std::vector<float>(cols, 0));

    StdVec2Di d8tofill(rows, std::vector<int>(cols, -1));

    calculateDirection8(inMatrix, d8tofill);
    for (int i = 0; i < d8tofill.size(); i++) {
        for (int j = 0; j < d8tofill[0].size(); j++) {
            float newAspect = 0;
            if (angleType == AngleUnits::RADIAN) {
                newAspect = decodeDirectionToRadian(d8tofill[i][j]);
                if (newAspect == -1) {//sink/flat
                    newAspect = SLRng::getFloat(0, 2 * 3.14159265359);
                }
            }
            else {
                newAspect = decodeDirectionToDegree(d8tofill[i][j]);
                if (newAspect == -1) {//sink/flat
                    newAspect = SLRng::getFloat(0, 360);
                }
            }
            
            outAspect[i][j] = newAspect;
        }
    }
}


void SLHydrology::calculateAspectAveraged(StdVec2Df& inMatrix, StdVec2Df& outAspect, SLHydrology::AngleUnits angleType) {
    if (inMatrix.size() == 0) {
        printf("::::ERROR:::: calculateAspect-> inMatrix size = 0\n");
        return;
    }
    int rows = inMatrix.size();
    int cols = inMatrix[0].size();

    outAspect.assign(rows, std::vector<float>(cols, 0));

    StdVec2Di d8tofill(rows, std::vector<int>(cols, -1));

    calculateDirection8(inMatrix, d8tofill);

    for (int i = 0; i < d8tofill.size(); i++) {
        for (int j = 0; j < d8tofill[0].size(); j++) {
            // average aspect of two highest slopes
            float maxSlope = 0.0;
            SLPoint maxSlopeDir = SLPoint(-1, -1);
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        // for purposes of slope all cells also assumed to be cubes
                        // (i.e. height is assumed to be in the same units as width and height
                        // and can therefore be ignored here.)
                        //float heightDiff = inMatrix[i + dy][j + dx] - inMatrix[i][j]; // alt comparing slope out
                        float heightDiff = inMatrix[i][j] - inMatrix[i + dy][j + dx];
                        float distance = std::sqrt(dx * dx + dy * dy);
                        float currentSlope = heightDiff / distance;
                        if (currentSlope > maxSlope) {
							maxSlope = currentSlope;
							maxSlopeDir = SLPoint(dx, dy);
						}
                    }
                }
            }
            float maxSlope2 = 0.0;
            SLPoint maxSlope2Dir = SLPoint(-1, -1);
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        //idea--average them
                        float heightDiff = inMatrix[i][j] - inMatrix[i + dy][j + dx]; // alt comparing slope out
                        float distance = std::sqrt(dx * dx + dy * dy);
                        float currentSlope = heightDiff / distance;
                        //maxSlope2 = std::max(maxSlope, currentSlope);

                        if (currentSlope > maxSlope2 && currentSlope < maxSlope) {
                            maxSlope2 = currentSlope;
                            maxSlope2Dir = SLPoint(dx, dy);
                        }
                    }
                }
            }
            float angle1 = decodeDirectionToDegree(encodeDirection(maxSlopeDir.x, maxSlopeDir.y));//oof
            float angle2 = decodeDirectionToDegree(encodeDirection(maxSlope2Dir.x, maxSlope2Dir.y));

            float diff = ((int)(angle1 - angle2 + 180 + 360) % 360) - 180;
            float angleOut = (int)(360 + angle2 + (diff / 2)) % 360;

            switch (angleType) {
            case AngleUnits::DEGREE:
                outAspect[i][j] = angleOut ;
                break;
            case AngleUnits::RADIAN:
                outAspect[i][j] = angleOut / (180 / 3.14159265359);
                break;
            }
        }
    }
}

void SLHydrology::calculateDirection8(StdVec2Df& inMatrix, StdVec2Di& outD8) {
    if (inMatrix.size() == 0) {
        printf("::::ERROR:::: calculateDirection8-> hieghtmap size = 0 n\n");
        return;
    }
    int rows = inMatrix.size();
    int cols = inMatrix[0].size();

    outD8.assign(rows, std::vector<int>(cols, 0));//0 being flats/sinks

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            // outlets
            if (i == 0) outD8[i][j] = -4;//N
            else if (j == 0) outD8[i][j] = -3;//W
            else if (i == rows - 1) outD8[i][j] = -2;//S
            else if (j == cols - 1) outD8[i][j] = -1;//E

            float maxSlope = 0.0;
            int direction = 0;// 0 for sink if no other direction found
            for (int dy = 1; dy >= -1; dy--) {
                for (int dx = 1; dx >= -1; dx--) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        float heightDiff = inMatrix[i][j] - inMatrix[i + dy][j + dx];
                        float distance = std::sqrt(dx * dx + dy * dy);
                        float currentSlope = heightDiff / distance;
                        if (currentSlope > maxSlope) {
                            maxSlope = currentSlope;
                            direction = encodeDirection(dx, dy); // direction encoding for D8 flow
                        }
                    }
                }
            }
            outD8[i][j] = direction;
        }
    }
}



// LOCAL ANALYSIS ON INTERNAL DATA---------------------------------------------------------------------------------------------
// methods should be called in order 
// 1. slope + aspect
// 2. calculateDirection8
// 3. calculateFlowAccumulation
// 4. calculateStrahlerOrder
// 5. indentify channels
// 6. USPED (erosion and deposition)

// recursively follow flow upslope, falling back on itself
// when cells with no inDirections (headwaters) are reached
void SLHydrology::calculateFlowAccumulation() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> hieghtmap size = 0\n");
        return;
    }
    else if (_slope.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> slope size = 0 -> calculate slope first\n");
        return;
    }
    else if (_aspect.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> aspect size = 0 -> calculate aspect first\n");
        return;
    }
    else if (_flowDirection.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> flowDirectionIn size = 0 -> calculate flowDirectionIn first\n");
        return;
    }
    int rows = _z.size();
    int cols = _z[0].size();

    sumFlowDirectionsIn();

    _flowAccumulation.assign(rows, std::vector<uint64_t>(cols, 0));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            flowAccumRecurv(i, j);
        }
    }
}


int SLHydrology::flowAccumRecurv(int i, int j) {
    if (_flowAccumulation[i][j] != 0) {
        return _flowAccumulation[i][j]; //already calculated for this cell
    }

    _flowAccumulation[i][j] = 1; // this cell flows into itself (i.e. all cells are at least a headwater)

    // bitwise decoding of all flow directions into this cell
    // REMEMBER x = j and y = i
    if (_flowDirectionIn[i][j] & 1) { // (1, -1) -> (x, y) -> (i - 1, j + 1)
        _flowAccumulation[i][j] += flowAccumRecurv(i - 1, j + 1);
    }
    if (_flowDirectionIn[i][j] & 2) { // (1, 0)
        _flowAccumulation[i][j] += flowAccumRecurv(i, j + 1);
    }
    if (_flowDirectionIn[i][j] & 4) { // (1, 1)
        _flowAccumulation[i][j] += flowAccumRecurv(i + 1, j + 1);
    }
    if (_flowDirectionIn[i][j] & 8) { // (0, 1)
        _flowAccumulation[i][j] += flowAccumRecurv(i + 1, j);
    }
    if (_flowDirectionIn[i][j] & 16) { // (-1, 1)
        _flowAccumulation[i][j] += flowAccumRecurv(i + 1, j - 1);
    }
    if (_flowDirectionIn[i][j] & 32) { // (-1, 0)
        _flowAccumulation[i][j] += flowAccumRecurv(i, j - 1);
    }
    if (_flowDirectionIn[i][j] & 64) { // (-1, -1)
        _flowAccumulation[i][j] += flowAccumRecurv(i - 1, j - 1);
    }
    if (_flowDirectionIn[i][j] & 128) { // (0, -1)
        _flowAccumulation[i][j] += flowAccumRecurv(i - 1, j);
    }
    return _flowAccumulation[i][j];
}

// optional before calculating erosion to temper the choppiness of the 
// relatively simple D8 flow accumulation model used here
void SLHydrology::blurFlowAccumulation() {
    if (_flowAccumulation.size() == 0) {
        printf("::::ERROR:::: blurFlowAccumulation-> flowAccumulation size = 0 -> calculate flowAccumulation first\n");
        return;
    }
    int rows = _flowAccumulation.size();
    int cols = _flowAccumulation[0].size();

    _blurredFlowAccumulation.assign(rows, std::vector<float>(cols, 0));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            _blurredFlowAccumulation[i][j] = (float)_flowAccumulation[i][j];
        }
    }

    blur(_blurredFlowAccumulation, 2, 1);
}


void SLHydrology::sumFlowDirectionsIn() {
    if (_flowDirection.size() == 0) {
        printf("::::ERROR:::: sumFlowDirectionsIn-> directions size = 0 --> call calculateDirection8() first\n");
        return;
    }
    int rows = _flowDirection.size();
    int cols = _flowDirection[0].size();

    _flowDirectionIn.assign(rows, std::vector<int>(cols, 0)); // initialized to 0 for sink

    // sum flow directions into each cell
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            // for each neighbour, does this neighbour flow into this cell?
            for (int8_t dy = -1; dy <= 1; dy++) {
                for (int8_t dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        if (_flowDirection[i + dy][j + dx] == encodeDirection(-dx, -dy)) {
                            // this will make sinks equal, but since it returns 0 it will not affect flow directions in.
                            _flowDirectionIn[i][j] += encodeDirection(dx, dy);
                        }
                    }
                }
            }
        }
    }
}

// fill sinks following Wang and Liu (2006)
// minimum height difference means that flow accumulation will still work on almost flats
// can then run again and the true flats will be where standing water should be!
// IDEA: could maybe make that a "FIND BASINS" function here-->or always mark flats here, despite minimum
void SLHydrology::fillSinksWangLiu(float minimumHeightDifferent) {
    if (_z.size() == 0) {
        printf("::::ERROR:::: fillSinksWangLiu-> hieghtmap size = 0\n");
        return;
    }
    int rows = _z.size();
    int cols = _z[0].size();

    //priority quque of unprocessed cells
    struct Cell {
        int row, col;
        float elevation;
        Cell(int i, int j, float z) : row(i), col(j), elevation(z) {}
        bool operator>(const Cell& other) const { return elevation > other.elevation; }
    };
    std::priority_queue<Cell, std::vector<Cell>, std::greater<Cell>> openQueue;
    StdVec2Df zSpill = _z;

    enum Processed { UNPROCESSED = -1, OPEN = 0, CLOSED = 1 };
    std::vector<std::vector<Processed>> processed(rows, std::vector<Processed>(cols, UNPROCESSED));

    // insert boundary cells into the priority queue
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (i == 0 || j == 0 || i == rows - 1 || j == cols - 1) {
                int cellIndex = i * cols + j;
                openQueue.push(Cell(i, j, zSpill[i][j]));
            }
        }
    }

    while (!openQueue.empty()) {
        Cell currentCell = openQueue.top();
        openQueue.pop();
        int i = currentCell.row;
        int j = currentCell.col;

        for (int8_t dy = -1; dy <= 1; dy++) {
            for (int8_t dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue; // Skip self
                int idy = dy + i;
                int jdx = dx + j;
                if (idy >= 0 && idy < rows && jdx >= 0 && jdx < cols) {
                    if (processed[idy][jdx] == UNPROCESSED) {

                        // calculate new elevation for the neighbor
                        zSpill[idy][jdx] = std::max(currentCell.elevation, zSpill[idy][jdx]);
                        if (zSpill[idy][jdx] - currentCell.elevation < minimumHeightDifferent) {
                            zSpill[idy][jdx] = currentCell.elevation + minimumHeightDifferent;
                        }

                        openQueue.push(Cell(idy, jdx, zSpill[idy][jdx]));
                        processed[idy][jdx] = OPEN;
                    }
                }
            }
        }
        processed[i][j] = CLOSED;
    }
    _zFilled = zSpill;
}

// simple channel identification using flow-->creates messy-looking channels
// strahler recommended and standard in the literature, but this can be useful
// for creating larger confluences of rivers
void SLHydrology::identifyChannelsByFlow(float flowAccumulationThresh) {
    if (_flowAccumulation.size() == 0) {
        printf("::::ERROR:::: blurFlowAccumulation-> flowAccumulation size = 0 -> calculate flowAccumulation first\n");
        return;
    }
    int rows = _flowAccumulation.size();
    int cols = _flowAccumulation[0].size();

    _isChannel.assign(rows, std::vector<bool>(cols, false));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (_flowAccumulation[i][j] > flowAccumulationThresh) {
                _isChannel[i][j] = true;
            }
        }
    }
}

// channel identification by strahler order threshold
// must have already calculated strahler order
void SLHydrology::identifyChannelsByStrahler(int threshold) {
    if (_flowAccumulation.size() == 0) {
        printf("::::ERROR:::: blurFlowAccumulation-> flowAccumulation size = 0 -> calculate flowAccumulation first\n");
        return;
    }
    int rows = _flowAccumulation.size();
    int cols = _flowAccumulation[0].size();

    _isChannel.assign(rows, std::vector<bool>(cols, false));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (_strahlerOrder[i][j] > threshold){// && _slope[i][j] > 0.00001) {// not flat, sink or outlet
                _isChannel[i][j] = true;
            }
        }
    }
}

// recursively tracks channels from headwaters to calculate strahler order
// where order increases when two channels of the same strahler order meet
void SLHydrology::calculateStrahlerOrder() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: USPED-> hieghtmap size = 0\n");
        return;
    }
    else if (_slope.size() == 0) {
        printf("::::ERROR:::: USPED-> slope size = 0 -> calculate slope first\n");
        return;
    }
    else if (_aspect.size() == 0) {
        printf("::::ERROR:::: USPED-> aspect size = 0 -> calculate aspect first\n");
        return;
    }
    if (_flowDirection.size() == 0) {
        printf("::::ERROR:::: USPED-> flowAccumulation size = 0 -> calculate flowAccumulation first\n");
        return;
    }
    int rows = _flowDirection.size();
    int cols = _flowDirection[0].size();
    _strahlerOrder.assign(rows, std::vector<int>(cols, 0));

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (_flowDirection[i][j] <= 0) { //so a sink or outlet
                //for all headwater cells
                //recursively add to strahler order until reach a confluence
                _strahlerOrder[i][j] = strahlerStep(i, j, _ero.strahlerThreshold);
            }
        }
    }
}


// recursively add to strahler order until reach a confluence
int SLHydrology::strahlerStep(int i, int j, int flowAccumulationThreshold) {
    if (_flowAccumulation[i][j] < flowAccumulationThreshold) {
        //save processing power by skipping cells with low flow accumulation (like in SAGA tools)
        return 0;
    }

    std::vector<int> strahlerIn = {};

    // bitwise decoding of all flow directions into this cell
    // REMEMBER x = j and y = i
    if (_flowDirectionIn[i][j] & 1) { // (1, -1) -> (x, y) -> (i - 1, j + 1)
        strahlerIn.push_back(strahlerStep(i - 1, j + 1, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 2) { // (1, 0)
        strahlerIn.push_back(strahlerStep(i, j + 1, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 4) { // (1, 1)
        strahlerIn.push_back(strahlerStep(i + 1, j + 1, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 8) { // (0, 1)
        strahlerIn.push_back(strahlerStep(i + 1, j, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 16) { // (-1, 1)
        strahlerIn.push_back(strahlerStep(i + 1, j - 1, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 32) { // (-1, 0)
        strahlerIn.push_back(strahlerStep(i, j - 1, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 64) { // (-1, -1)
        strahlerIn.push_back(strahlerStep(i - 1, j - 1, flowAccumulationThreshold));
    }
    if (_flowDirectionIn[i][j] & 128) { // (0, -1)
        strahlerIn.push_back(strahlerStep(i - 1, j, flowAccumulationThreshold));
    }

    //if there are no inputs, then this is a headwater cell
    if (strahlerIn.size() == 0) {
        _strahlerOrder[i][j] = 1;
        return 1;
    }

    //find the max strahler order of all the inputs and if there are multiple
    //inputs with the same strahler order, then increment the strahler order
    //of this cell by 1
    int maxStrahlerIn = 0;
    int maxDuplicate = 0;
    for (int i = 0; i < strahlerIn.size(); i++) {
        if (strahlerIn[i] > maxStrahlerIn) {
            maxStrahlerIn = strahlerIn[i];
        }
        else if (strahlerIn[i] == maxStrahlerIn) {
            maxDuplicate = strahlerIn[i];
        }
    }

    if (maxDuplicate == maxStrahlerIn) {
        _strahlerOrder[i][j] = maxStrahlerIn + 1;
        return maxStrahlerIn + 1;
    }
    else {
        _strahlerOrder[i][j] = maxStrahlerIn;
        return maxStrahlerIn;
    }
}



// USPED (Unit Stream Power based Erosion Deposition) - from Mitasova et al 1996, Mitas and Mitasova 1998
// (flow accumulation, _slope and _aspect must be run first-->assumes _slope and _aspect are in DEGREES)
void SLHydrology::USPED(float multiplier, StdVec2Df* C, StdVec2Df* K, StdVec2Df* R) {
    if (_z.size() == 0) {
        printf("::::ERROR:::: USPED-> hieghtmap size = 0\n");
        return;
    }
    else if (_slope.size() == 0) {
        printf("::::ERROR:::: USPED-> slope size = 0 -> calculate slope first\n");
        return;
    }
    else if (_aspect.size() == 0) {
        printf("::::ERROR:::: USPED-> aspect size = 0 -> calculate aspect first\n");
        return;
    }
    if (_flowAccumulation.size() == 0) {
        printf("::::ERROR:::: USPED-> flowAccumulation size = 0 -> calculate flowAccumulation first\n");
        return;
    }
    int rows = _flowAccumulation.size();
    int cols = _flowAccumulation[0].size();

    bool prevailingRill;
    if (_ero.prevailingRill == 0) {
        prevailingRill = false;
    }
    else if (_ero.prevailingRill == 1) {
        prevailingRill = true;
    }
    else {
        prevailingRill = SLRng::getBool();
    }

    _erosionDeposition.assign(rows, std::vector<float>(cols, 0));

    // a cheat to "even out" erosion since using choppy D8 flow accumulation 
    // (though found in terrain gen tests that it seems to work better to blur USPED output since
    // much of the choppiness comes from the mere eight angles available for _aspect as calculated here
    StdVec2Df flowBlurred(rows, std::vector<float>(cols, 0));
    
    for (int i = 0; i < _flowAccumulation.size(); i++) {
        for (int j = 0; j < _flowAccumulation[0].size(); j++) {
            flowBlurred[i][j] = _flowAccumulation[i][j];
        }
    }
    if (_ero.blurFlow) {
        blur(flowBlurred, 1, .5);
    }

    // STEP 1: following from http://fatra.cnr.ncsu.edu/~hmitaso/gmslab/denix/usped.html
    // looks like the site may be dead-> https://web.archive.org/web/20230119213255/http://fatra.cnr.ncsu.edu/~hmitaso/gmslab/denix/usped.html
    // Slope + _aspect + flow accumulation -> already calculated


    // STEP 2
    // sflowtopo = Pow([flowacc] * resolution, 0.6) * Pow(Sin([_slope] * 0.01745), 1.3))
    StdVec2Df sflowtopo(rows, std::vector<float>(cols, 0));
    if (prevailingRill) {
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                sflowtopo[i][j] = pow(flowBlurred[i][j] * _ero.cellSize, 1.6) * pow(sin(_slope[i][j] * 0.01745), 1.3);
            }
        }
    }
    else { // sheet erosion
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                sflowtopo[i][j] = flowBlurred[i][j] * _ero.cellSize * sin(_slope[i][j] * 0.01745);
            }
        }
    }

    // STEP 3
    // now K, C and R factors
    // initialize with constants from parameters if no custom matricies provided
    StdVec2Df Klocal(rows, std::vector<float>(cols, _ero.K)); //soil factor
    StdVec2Df Clocal(rows, std::vector<float>(cols, _ero.C)); //cover factor
    StdVec2Df Rlocal(rows, std::vector<float>(cols, _ero.R)); //rainfall intensity factor-->to be linked to starting flow accumulation???
    if (K == nullptr) { K = &Klocal; }
    if (C == nullptr) { C = &Clocal; }
    if (R == nullptr) { R = &Rlocal; }

    // qsx = [sflowtopo] * [kfac] * [cfac] * R * Cos((([_aspect] *  (-1)) + 450) * .01745)
    // TODO--what is the point of the -1 multipler and +450-->seems to be exactly the same otherwise
    StdVec2Df qsx(rows, std::vector<float>(cols, 0));
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            qsx[i][j] = sflowtopo[i][j] * (*K)[i][j] * (*C)[i][j] * (*R)[i][j] * cos(((_aspect[i][j] * -1) + 450) * 0.01745);
            //qsx[i][j] = sflowtopo[i][j] * (*K)[i][j] * (*C)[i][j] * (*R)[i][j] * cos((_aspect[i][j]) * 0.01745);
        }
    }

    // qsy = [sflowtopo] * [kfac] * [cfac] * 280 * Sin((([_aspect] *  (-1)) + 450) * .01745)
    StdVec2Df qsy(rows, std::vector<float>(cols, 0));
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            qsy[i][j] = sflowtopo[i][j] * (*K)[i][j] * (*C)[i][j] * (*R)[i][j] * sin(((_aspect[i][j] * -1) + 450) * 0.01745);
            //qsy[i][j] = sflowtopo[i][j] * (*K)[i][j] * (*C)[i][j] * (*R)[i][j] * sin((_aspect[i][j]) * 0.01745);
        }
    }

    // STEP 4
    // slope qsx
    StdVec2Df qsxSlope(rows, std::vector<float>(cols, 0));
    calculateSlope(qsx, qsxSlope, DEGREE);

    // aspect qsx
    StdVec2Df qsxAspect(rows, std::vector<float>(cols, 0));
    calculateAspect(qsx, qsxAspect, DEGREE);

    // STEP 5
    // slope qsy
    StdVec2Df qsySlope(rows, std::vector<float>(cols, 0));
    calculateSlope(qsy, qsySlope, DEGREE);

    // aspect qsy
    StdVec2Df qsyAspect(rows, std::vector<float>(cols, 0));
    calculateAspect(qsy, qsyAspect, DEGREE);


    // STEP 6
    // qsx_dx = Cos((([qsx_aspect] * (-1)) + 450) * .01745) * Tan([qsx_slope] * .01745)
    StdVec2Df qsxDx(rows, std::vector<float>(cols, 0));
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            qsxDx[i][j] = cos(((qsxAspect[i][j] * -1) + 450) * .01745) * tan(qsxSlope[i][j] * .01745);
            //qsxDx[i][j] = cos((qsxAspect[i][j]) * .01745) * tan(qsxSlope[i][j] * .01745);
        }
    }

    // qsy_dy =  Sin((([qsy_aspect] * (-1)) + 450) * .01745) * Tan([qsy_slope] * .01745)
    StdVec2Df qsxDy(rows, std::vector<float>(cols, 0));
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            qsxDy[i][j] = sin(((qsyAspect[i][j] * -1) + 450) * .01745) * tan(qsySlope[i][j] * .01745);
            //qsxDy[i][j] = sin((qsyAspect[i][j]) * .01745) * tan(qsySlope[i][j] * .01745);
        }
    }

    // FINAL!
    // USPED = [qsx_dx] + [qsy_dy]  -> for prevailing rill erosion
    // USPED = ([qsx_dx] + [qsy_dy]) * 10.  -> for prevailing sheet erosion
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            _erosionDeposition[i][j] = (qsxDx[i][j] + qsxDy[i][j]) * multiplier; // multiplier to allow faster erosion/deposition--can lead to more artifacting
            if (!prevailingRill) { _erosionDeposition[i][j] *= 10; }
            _erosionDeposition[i][j] -= (*R)[i][j] * _ero.weightErosion;
        }
    }
}



//---------------------------------------------------------------------------------------------------
// simple basic functions that can be helpful preprocessing with noisy heightmaps
// before commiting to more expensive methods like Wang and Liu fill sinks

void SLHydrology::basicFillSinksPinholesMin() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> hieghtmap size = 0\n");
        return;
    }
    int rows = _z.size();
    int cols = _z[0].size();

    StdVec2Df newMap = _z;

    for (int i = 0; i < rows; i++) {
        for (int j = 1; j < cols; j++) {
            float minNeighborHeight = 99999999999;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        if (_z[i + dy][j + dx] < minNeighborHeight) {
                            minNeighborHeight = _z[i + dy][j + dx];
                        }
                    }
                }
            }
            if (minNeighborHeight > _z[i][j]) {
                newMap[i][j] = minNeighborHeight + .00001;
            }
        }
    }
    _z = newMap;
}

void SLHydrology::basicFillSinksPinholesAvg() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> hieghtmap size = 0\n");
        return;
    }
    int rows = _z.size();
    int cols = _z[0].size();

    StdVec2Df newMap = _z;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float minNeighborHeight = 99999999999;
            float nextNeighborHeight = 99999999999;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue; // skip self
                    if (i + dy >= 0 && i + dy < rows && j + dx >= 0 && j + dx < cols) {
                        if (_z[i + dy][j + dx] < minNeighborHeight) {
                            nextNeighborHeight = minNeighborHeight;
                            minNeighborHeight = _z[i + dy][j + dx];
                        }
                        else if (_z[i + dy][j + dx] < nextNeighborHeight) {
                            nextNeighborHeight = _z[i + dy][j + dx];
                        }
                    }
                }
            }
            if (minNeighborHeight > _z[i][j]) {
                newMap[i][j] = (minNeighborHeight + nextNeighborHeight) / 2;
            }
        }
    }
    _z = newMap;
}


void SLHydrology::basicFlattenPeaks() {
    if (_z.size() == 0) {
        printf("::::ERROR:::: calculateFlowAccumulation-> hieghtmap size = 0\n");
        return;
    }
    int rows = _z.size();
    int cols = _z[0].size();

    StdVec2Df newMap = _z;

    for (int i = 1; i < rows - 1; i++) {
        for (int j = 1; j < cols - 1; j++) {
            if (_flowAccumulation[i][j] > 1) continue;
            if (i < 1 || j < 1 || i > rows - 2 || j > cols - 2) {
                continue;
            }
            float neighbourAvg = (_z[i - 1][j - 1] + _z[i - 1][j] + _z[i - 1][j + 1] +
                _z[i][j - 1] + _z[i][j] + _z[i][j + 1] +
                _z[i + 1][j - 1] + _z[i + 1][j] + _z[i + 1][j + 1]) / 9.0;
            newMap[i][j] = neighbourAvg;
        }
    }
    _z = newMap;
}