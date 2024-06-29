#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <fstream>
#include <random>

namespace SLMath {

    // STUCTS AND CLASSES ----------------------------------------------------------------------------------------
    
    // RNG using modern C++11
    class SLRng {
    public:
        static void init() {
            std::random_device seed;
            gen = std::mt19937(seed());
        }
        static void init(int seed) {
            gen = std::mt19937(seed);
        }
        static float getFloat(float min, float max) {
            std::uniform_real_distribution<float> dis(min, max);
            return dis(gen);
        }
        static float getFloat(float max) {
            std::uniform_real_distribution<float> dis(0, max);
            return dis(gen);
        }
        static int getInt(int min, int max) {
            std::uniform_int_distribution<int> dis(min, max);
            return dis(gen);
        }
        static int getInt(int max) {
            std::uniform_int_distribution<int> dis(0, max);
            return dis(gen);
        }
        static bool getBool() {
            std::uniform_int_distribution<int> dis(0, 1);
            return round(dis(gen));
        }
    private:
        static std::mt19937 gen;
    };
    

    // for basic point storage wihtout operators
    struct SLPoint {
        SLPoint() : x(0), y(0) {}
        SLPoint(int x, int y) : x(x), y(y) {}
        bool operator==(const SLPoint& p) {
            return x == p.x && y == p.y;
        }
        bool operator!=(const SLPoint& p) {
            return x != p.x || y != p.y;
        }

        int x;
        int y;
    };

    // a bad hash function for SLPoint
    struct SLPointhash {
        std::size_t operator () (const SLPoint& p) const {
            auto h1 = std::hash<int>{}(p.x);
            auto h2 = std::hash<int>{}(p.y);
            return h1 ^ h2;
        }
    };


    // Simple colour struct with rgba values including basic operators.
    // Use blendUsingAlpha to blend colours using the alpha of the added colour.
    struct SLColor {
        float r;
        float g;
        float b;
        float a;

        SLColor() : r(0), g(0), b(0), a(255) {}
        SLColor(float r, float g, float b, float a = 255) : r(r), g(g), b(b), a(a) {}
        SLColor(float v, float a = 255) : r(v), g(v), b(v), a(a) {}
        SLColor(const char* hex, float a) {
            int ir, ig, ib;
            sscanf_s(hex, "%02x%02x%02x", &ir, &ig, &ib);
            r = ir;
            g = ig;
            b = ib;
            this->a = a;
        }
        SLColor(std::string hex) : SLColor(hex.c_str(), 255) {}

        SLColor operator+(const SLColor& c) const {
            return SLColor(r + c.r, g + c.g, b + c.b, a + c.a);
        }
        SLColor operator-(const SLColor& c) const {
            return SLColor(r - c.r, g - c.g, b - c.b, a - c.a);
        }
        SLColor operator*(const SLColor& c) const {
            return SLColor(r * c.r, g * c.g, b * c.b, a * c.a);
        }
        SLColor operator/(const SLColor& c) const {
            return SLColor(r / c.r, g / c.g, b / c.b, a / c.a);
        }
        SLColor operator+(float f) const {
            return SLColor(r + f, g + f, b + f, a + f);
        }
        SLColor operator-(float f) const {
            return SLColor(r - f, g - f, b - f, a - f);
        }
        SLColor operator*(float f) const {
            return SLColor(r * f, g * f, b * f, a * f);
        }
        SLColor operator/(float f) const {
            return SLColor(r / f, g / f, b / f, a / f);
        }

        //add colours using alpha of added colour
        SLColor blendUsingAlpha(const SLColor& c) const {
            float alpha = c.a / 255.0;
            return SLColor(r * (1 - alpha) + c.r * alpha, g * (1 - alpha) + c.g * alpha, b * (1 - alpha) + c.b * alpha, 255);
        }
    };


    // Structure to represent a 2D vector with operators and basic functions.
    template <typename T>
    struct SLVec2 {
        T x;
        T y;

        SLVec2<T>() : x(0), y(0) {};
        SLVec2<T>(T _x, T _y) : x(_x), y(_y) {};

        SLVec2<T> operator+(SLVec2<T> vec2) const {
            return { x + vec2.x, y + vec2.y };
        }
        SLVec2<T> operator-(const SLVec2<T>& vec2) const {
            return { x - vec2.x, y - vec2.y };
        }
        SLVec2<T> operator*(const SLVec2<T>& vec2) const {
            return { x * vec2.x, y * vec2.y };
        }
        SLVec2<T> operator/(SLVec2<T> vec2) const {
            return { x / vec2.x, y / vec2.y };
        }
        SLVec2<T> operator+(T scalar) const {
            return SLVec2<T>{ x + scalar, y + scalar };
        }
        SLVec2<T> operator-(T scalar) const {
            return { x - scalar, y - scalar };
        }
        SLVec2<T> operator*(T scalar) const {
            return SLVec2<T>{ x* scalar, y* scalar };
        }
        SLVec2<T> operator/(T scalar) const {
            return { x / scalar, y / scalar };
        }
        bool operator==(const SLVec2<T>& vec2) const {
            return x == vec2.x && y == vec2.y;
        }
        bool operator!=(const SLVec2<T>& vec2) const {
            return !(*this == vec2);
        }
        // compares lengths from an assumed 0,0 origin
        bool operator<(const SLVec2<T>& vec2) const {
            return length() < vec2.length();
        }
        // compares lengths from an assumed 0,0 origin
        bool operator>(const SLVec2<T>& vec2) const {
            return length() > vec2.length();
        }
        // compares lengths from an assumed 0,0 origin
        bool operator<=(const SLVec2<T>& vec2) const {
            return length() <= vec2.length();
        }
        // compares lengths from an assumed 0,0 origin
        bool operator>=(const SLVec2<T>& vec2) const {
            return length() >= vec2.length();
        }

        // assumed origin at 0,0
        float length() const {
            return std::sqrt(x * x + y * y);
        }
        // assumed origin at 0,0
        float angleRadians() const {
            return atan2(y, x);
        }
        // assumed origin at 0,0
        float angleDegrees() const {
            return atan2(y, x) * 57.2957914;
        }


        // non-member operators for scalars, defined inline so that
        // they do not need to be defined for each typedef
        friend SLVec2<T> operator+(T f, const SLVec2<T>& vec) {
            return { f + vec.x, f + vec.y };
        }
        friend SLVec2<T> operator-(T f, const SLVec2<T>& vec) {
            return { f - vec.x, f - vec.y };
        }
        friend SLVec2<T> operator*(T f, const SLVec2<T>& vec) {
            return { f * vec.x, f * vec.y };
        }
        friend SLVec2<T> operator/(T f, const SLVec2<T>& vec) {
            return { f / vec.x, f / vec.y };
        }

    };

    // convenience types
    typedef SLVec2<uint64_t> SLVec2u64;
    typedef SLVec2<int64_t> SLVec2i64;
    typedef SLVec2<int8_t> SLVec2i8;
    typedef SLVec2<int> SLVec2i;
    typedef SLVec2<float> SLVec2f;

    // a bad hash function for slVec
    template <typename T>
    struct SLVecHash {
        std::size_t operator () (const SLVec2<T>& p) const {
            auto h1 = std::hash<T>{}(p.x);
            auto h2 = std::hash<T>{}(p.y);
            return h1 ^ h2;
        }
    };

    // END STUCTS AND CLASSES ----------------------------------------------------------------------------------------






    // FUNCTIONALITY -------------------------------------------------------------------------------------------------
    // 
    // Basics-------------------------------------------------------------------------------------
    // TODO--template versions
    float distance2D(float x1, float y1, float x2, float y2);
    float distance2D(SLVec2f p1, SLVec2f p2);
    float distance2D(SLPoint p1, SLPoint p2);

    SLVec2f pointAlongCoordLine(SLVec2f p1, SLVec2f p2, float r);
    float angleBetweenPoints(SLVec2f p1, SLVec2f p2);

    SLVec2f rotateVector(const SLVec2f& vec, float angle);
    void rotatePoly(const std::vector<SLVec2f>& in, std::vector<SLVec2f>& out, float angle);

    float fastlerp(float a, float b, float f);
    float lerp(float a, float b, float f);
    float rndf(); // rand() based float 0-1
    template <typename T>
    T slClamp(T val, T min, T max) {
        if (val < min) val = min;
        else if (val > max) val = max;
        return val;
    }

    // vector and matrix functions--------------------------------------------------------------------------------
    void blur(std::vector<std::vector<float>>& matrix, int iterations, float amountPerIter);
    void blurAvg(std::vector<std::vector<float>>& matrix, int iterations);

    template <typename T>
    bool saveVector(std::vector<T>& vector, std::ofstream& fout) {
        //TODO-->checks
        printf(":SAVE MATRIX:\n");
        int size = (int)(vector.size());
        fout.write((char*)(&size), sizeof(int));

        printf("SSize of T: %d\n", sizeof(T));
        printf("SSize of vector: %d\n", size);
        fout.write((char*)(&vector[0]), size * sizeof(T));
        return true;
    }


    template <typename T>
    bool loadVector(std::vector<T>& vector, std::ifstream& fin) {
        //TODO-->checks
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
    bool saveMatrix(std::vector<std::vector<T>>& matrix, std::ofstream& fout) {
        //TODO-->checks
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

    template <typename T>
    bool loadMatrix(std::vector<std::vector<T>>& matrix, std::ifstream& fin) {
        //TODO-->checks
        printf(":LOAD MATRIX:\n");
        int y, x;
        fin.read((char*)(&y), sizeof(int));
        fin.read((char*)(&x), sizeof(int));

        printf("Size of T: %d\n", sizeof(T));
        printf("Size of matrix: %d, %d\n", y, x);

        matrix = std::vector <std::vector<T>>(y, std::vector<T>(x));

        for (int i = 0; i < y; i++) {
            fin.read((char*)(&matrix[i][0]), x * sizeof(T));
        }
        return true;
    }
    // END FUNCTIONALITY -----------------------------------------------------------------------------------------------


    class PerlinNoise2D {
    public:
        // where did I get this from?
        PerlinNoise2D() {
            // initialize permutation table
            for (int i = 0; i < 256; ++i) {
                p[i] = i;
            }
            std::random_shuffle(p, p + 256);
            for (int i = 0; i < 256; ++i) {
                p[256 + i] = p[i];
            }
        }

        double noise(double x, double y) const {
            int X = static_cast<int>(floor(x)) & 255;
            int Y = static_cast<int>(floor(y)) & 255;
            x -= floor(x);
            y -= floor(y);
            double u = fade(x);
            double v = fade(y);
            int A = p[X] + Y, B = p[X + 1] + Y;

            return (lerp(v, lerp(u, grad(p[A], x, y), grad(p[B], x - 1, y)),
                lerp(u, grad(p[A + 1], x, y - 1), grad(p[B + 1], x - 1, y - 1))) + 1) * .5;
        }

    private:
        double fade(double t) const {
            return t * t * t * (t * (t * 6 - 15) + 10);
        }

        double lerp(double t, double a, double b) const {
            return a + t * (b - a);
        }

        double grad(int hash, double x, double y) const {
            int h = hash & 15;
            double u = h < 8 ? x : y;
            double v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
            return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
        }

        int p[512]; // permutation table
    };
}; // namespace SLMath
