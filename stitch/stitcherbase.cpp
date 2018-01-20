//File: stitcherbase.cc
//Author: Yuxin Wu <ppwwyyxx@gmail.com>

#include "stitcherbase.h"
#include "lib/timer.h"
#define DEBUG_MODE
namespace pano {

void StitcherBase::calc_feature() {
	GuardedTimer tm("calc_feature()");
	feats.resize(imgs.size());
	keypoints.resize(imgs.size());
	// detect feature
#ifndef DEBUG_MODE
#pragma omp parallel for schedule(dynamic)
#endif // !DEBUG_MODE
	for (int k = 0; k < imgs.size(); k++) {
		imgs[k].load();
		feats[k] = feature_det->detect_feature(*imgs[k].img);
		if (config::LAZY_READ)
			imgs[k].release();
		if (feats[k].size() == 0)
			error_exit(ssprintf("Cannot find feature in image %lu!\n", k));
		print_debug("Image %lu has %lu features\n", k, feats[k].size());
		keypoints[k].resize(feats[k].size());
		for (int i=0; i < feats[k].size(); i++)
			keypoints[k][i] = feats[k][i].coor;
	}
}

void StitcherBase::free_feature() {
	feats.clear(); feats.shrink_to_fit();	// free memory for feature
	keypoints.clear(); keypoints.shrink_to_fit();	// free memory for feature
}

}
