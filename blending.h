// file: blending.h
// do blending by Homographies and Images
#pragma once
#include <A.h>

#include <vector>
#include "homography.h"

namespace pano {

	Mat32f blendingCPU(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs);

}
