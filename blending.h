// file: blending.h
// do blending by Homographies and Images
#pragma once
#include <A.h>

//for 魔改blend()函数
#include <vector>
#include <cassert>
#include "lib/mat.h"
#include "projection.h"
#include "homography.h"
#include "imageref.h"
//for 魔改blend()函数

#include "homography.h"

using namespace std;
using namespace pano;

void blendingGPU_CUDA(vector<Mat32f> & input_imgs, vector<Homography> & homogs, vector<vector<Coor>>& imgs_ranges_minNmax, vector<Vec2D>& final_proj_range_idnt, Coor& target_size, Vec2D& final_resolution);

Mat32f blendingCPU(vector<Mat32f> & input_imgs, vector<Homography> & homogs, vector<vector<Coor>>& imgs_ranges_minNmax, vector<Vec2D>& final_proj_range, Coor& target_size, Vec2D& final_resolution);

Mat32f blend_YW_ver(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs);

