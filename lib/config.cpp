// File: config.cc
// Date: Sat May 04 22:22:20 2013 +0800
// Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#include "config.h"
#include "debugutils.h"
#include "utils.h"
#include <iostream>
using namespace std;

namespace config {

// TODO allow different types for a value. using template
ConfigParser::ConfigParser(const char* fname) {
	if (!exists_file)
		error_exit("Cannot find config file!");
	const static size_t BUFSIZE = 4096;		// TODO overflow
	ifstream fin(fname);
	string s; s.resize(BUFSIZE);
	float val;
	while (fin >> s) {
		if (s[0] == '#') {
			fin.getline(&s[0], BUFSIZE, '\n');
			continue;
		}
		fin >> val;
		data[s] = val;
		fin.getline(&s[0], BUFSIZE, '\n');
	}
}

float ConfigParser::get(const std::string& s) {
	if (data.count(s) == 0)
		error_exit(ssprintf("Option %s not found in config file!\n", s.c_str()));
	return data[s];
}

// here confirm the config PARAS
bool CYLINDER = false;
bool TRANS = false;
bool CROP = true;
float FOCAL_LENGTH = 37;
bool ESTIMATE_CAMERA = true;
bool STRAIGHTEN = true;
int MAX_OUTPUT_SIZE = 8000;
bool ORDERED_INPUT = false;
bool LAZY_READ = true;

int SIFT_WORKING_SIZE = 800;
int NUM_OCTAVE = 3;
int NUM_SCALE = 7;
float SCALE_FACTOR = 1.4142135623;

float GAUSS_SIGMA = 1.4142135623;
int GAUSS_WINDOW_FACTOR = 4;

float JUDGE_EXTREMA_DIFF_THRES = 2e-3;
float CONTRAST_THRES = 3e-2;
float PRE_COLOR_THRES = 5e-2;
float EDGE_RATIO = 10;

int CALC_OFFSET_DEPTH = 4;
float OFFSET_THRES = 0.5;

float ORI_RADIUS = 4.5;
int ORI_HIST_SMOOTH_COUNT = 2;

int DESC_HIST_SCALE_FACTOR = 3;
int DESC_INT_FACTOR = 512;

float MATCH_REJECT_NEXT_RATIO = 0.8;

int RANSAC_ITERATIONS = 1500;
double RANSAC_INLIER_THRES = 3.5;
float INLIER_IN_MATCH_RATIO = 0.1;
float INLIER_IN_POINTS_RATIO = 0.04;

float SLOPE_PLAIN = 8e-3;

int MULTIPASS_BA = 1;
float LM_LAMBDA = 5;

int MULTIBAND = 0;

}
