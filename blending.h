// file: blending.h
// do blending by Homographies and Images
#pragma once
#include <A.h>

//for ħ��blend()����
#include <vector>
#include <cassert>
#include "lib/mat.h"
#include "projection.h"
#include "homography.h"
#include "imageref.h"
//for ħ��blend()����

#include "homography.h"

using namespace std;
using namespace pano;

Mat32f blendingCPU(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs);

Mat32f blend_YW_ver(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs);

