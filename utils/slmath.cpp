#include "slmath.h"

// static initialization
std::mt19937 SLMath::SLRng::gen;

// calculated distance
float SLMath::distance2D(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return sqrt(dx * dx + dy * dy);
}

// calculated distance
float SLMath::distance2D(SLMath::SLVec2f p1, SLMath::SLVec2f p2) {
    return SLMath::distance2D(p1.x, p1.y, p2.x, p2.y);
}

float SLMath::distance2D(SLMath::SLPoint p1, SLMath::SLPoint p2) {
    return SLMath::distance2D(p1.x, p1.y, p2.x, p2.y);
}

SLMath::SLVec2f SLMath::pointAlongCoordLine(SLMath::SLVec2f p1, SLMath::SLVec2f p2, float r) {
    return (r * (p2 - p1)) + p1;
}

float SLMath::angleBetweenPoints(SLMath::SLVec2f p1, SLMath::SLVec2f p2) {
    return atan2(p2.y - p1.y, p2.x - p1.x);
}

// rotate a 2D vector using a rotation matrix
SLMath::SLVec2f SLMath::rotateVector(const SLMath::SLVec2f& vec, float angle) {
    // calculate the sine and cosine of the angle
    float cosAngle = std::cos(angle);
    float sinAngle = std::sin(angle);

    // create the rotation matrix
    float rotationMatrix[2][2] = {
        {cosAngle, -sinAngle},
        {sinAngle, cosAngle}
    };

    // perform matrix multiplication
    SLMath::SLVec2f rotatedVector;
    rotatedVector.x = rotationMatrix[0][0] * vec.x + rotationMatrix[0][1] * vec.y;
    rotatedVector.y = rotationMatrix[1][0] * vec.x + rotationMatrix[1][1] * vec.y;

    return rotatedVector;
}

// with assumed 0 origin
void SLMath::rotatePoly(const std::vector<SLMath::SLVec2f>& in, std::vector<SLMath::SLVec2f>& out, float angle) {
    if (in.size() != out.size()) {
        printf("WARNING: attemped rotation with in and out vectors of different sizes\n");
        return;
    }

    for (int i = 0; i < in.size(); i++) {
        SLMath::SLVec2f rotV = SLMath::rotateVector(SLMath::SLVec2f(in[i]), angle);
        out[i].x = rotV.x;
        out[i].y = rotV.y;
    }
}

float SLMath::fastlerp(float a, float b, float f) {
    return a + f * (b - a);
}
float SLMath::lerp(float a, float b, float f) {
    return a * (1 - f) + (b * f);
}
float SLMath::rndf() {
    return static_cast<float> (rand()) / static_cast<float> (RAND_MAX);
}


void SLMath::blur(std::vector<std::vector<float>>& matrix, int iterations, float amountPerIter) {
    int _height = matrix.size();
    int _width = matrix[0].size();
    std::vector<std::vector<float>> newMap = matrix;

    for (int i = 0; i < iterations; i++) {
        for (int j = 1; j < _height - 1; j++) {
            for (int k = 1; k < _width - 1; k++) {
                float neighbourAvg = (matrix[j - 1][k - 1] + matrix[j - 1][k] + matrix[j - 1][k + 1] +
                    matrix[j][k - 1] + matrix[j][k] + matrix[j][k + 1] +
                    matrix[j + 1][k - 1] + matrix[j + 1][k] + matrix[j + 1][k + 1]) / 9.0;
                newMap[j][k] += (neighbourAvg - matrix[j][k]) * amountPerIter;
            }
        }
        matrix = newMap;
    }

}

void SLMath::blurAvg(std::vector<std::vector<float>>& matrix, int iterations) {
    int _height = matrix.size();
    int _width = matrix[0].size();
    std::vector<std::vector<float>> newMap = matrix;

    for (int i = 0; i < iterations; i++) {
        for (int j = 1; j < _height - 1; j++) {
            for (int k = 1; k < _width - 1; k++) {
                float neighbourAvg = (matrix[j - 1][k - 1] + matrix[j - 1][k] + matrix[j - 1][k + 1] +
                    matrix[j][k - 1] + matrix[j][k] + matrix[j][k + 1] +
                    matrix[j + 1][k - 1] + matrix[j + 1][k] + matrix[j + 1][k + 1]) / 9.0;
                newMap[j][k] = neighbourAvg; //even simpler blur
            }
        }
        matrix = newMap;
    }

}
