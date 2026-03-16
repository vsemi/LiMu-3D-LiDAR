#pragma once

#include <stdint.h>
#include <vector>

#define NUM_COLORS    7500

static const int PIXEL_CODE_OFFSET = 48000;

static const int LOW_AMPLITUDE = 16001 + PIXEL_CODE_OFFSET;
static const int ADC_OVERFLOW  = 16002 + PIXEL_CODE_OFFSET;
static const int SATURATION    = 16003 + PIXEL_CODE_OFFSET;
static const int INTERFERENCE  = 16007 + PIXEL_CODE_OFFSET;
static const int EDGE_DETECTED = 16008 + PIXEL_CODE_OFFSET;

struct Color {
	uint8_t r, g, b;
	Color() : r(0), g(0), b(0) {}
	Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};
class ImageColorizer {
public:
	ImageColorizer();
	Color getColor(int value);
	uint8_t getGrayscale(int value);
	void setRange(int start, int stop);

private:
	std::vector<Color> colorVector;
	int begin;
	int end;
	int numSteps;
	double indexFactorColor;
	double indexFactorBw;

	double interpolate(double x, double x0, double y0, double x1, double y1);
	void createColorMap(int numSteps, int indx, unsigned char &red, unsigned char &green, unsigned char &blue);
};
