// File: stitcher.cc
// Date: Sun Sep 22 12:54:18 2013 +0800
// Author: Yuxin Wu <ppwwyyxxc@gmail.com>


#include "stitcher.h"

#include <limits>
#include <string>
#include <cmath>
#include <queue>

#include "feature/matcher.h"
#include "lib/imgproc.h"
#include "lib/timer.h"
#include "blender.h"
#include "match_info.h"
#include "transform_estimate.h"
#include "camera_estimator.h"
#include "camera.h"
#include "warp.h"
using namespace std;
using namespace pano;
using namespace config;

namespace pano {

// use in development
const static bool DEBUG_OUT = false; // debug������ƣ�����dump match���ݵ�ʱ����ܻ���ִ���CImg�У�
const static char* MATCHINFO_DUMP = "log/matchinfo.txt";

void Stitcher::change_imgsref(std::vector<Mat32f>& mat_imgs) {
	// bundle.component[0].imgptr // ����/�滻 ��blend��ͼƬ
	for (int k = 0; k < imgs.size(); k++) {
		imgs[k].change_oneimgref(mat_imgs[k]);
	}
}

Mat32f Stitcher::only_render() {
	// TODO automatically determine projection method
	if (ESTIMATE_CAMERA)
		//bundle.proj_method = ConnectedImages::ProjectionMethod::cylindrical;
		bundle.proj_method = ConnectedImages::ProjectionMethod::spherical;
	else
		bundle.proj_method = ConnectedImages::ProjectionMethod::flat;
	print_debug("Using projection method: %d\n", bundle.proj_method);
	bundle.update_proj_range();
	return bundle.blend();
}

void Stitcher::only_build_homog() {
	calc_feature();
	// TODO choose a better starting point by MST use centrality

	pairwise_matches.resize(imgs.size());
	for (auto& k : pairwise_matches) k.resize(imgs.size());
	if (ORDERED_INPUT)
		linear_pairwise_match();
	else
		pairwise_match();
	free_feature();
	//load_matchinfo(MATCHINFO_DUMP);
	if (DEBUG_OUT) {
		draw_matchinfo();
		dump_matchinfo(MATCHINFO_DUMP);
	}
	assign_center();

	if (ESTIMATE_CAMERA)
		estimate_camera();
	else
		build_linear_simple();		// naive mode
	pairwise_matches.clear();
}

void Stitcher::return_homogs(std::vector<Homography> & result_homogs, std::vector<Homography> & result_homogs_invers) {
	for (auto &i : bundle.component) {
		result_homogs.emplace_back(i.homo);
		result_homogs_invers.emplace_back(i.homo_inv);
	}
}

Mat32f Stitcher::build() {
	calc_feature();
	// TODO choose a better starting point by MST use centrality

	pairwise_matches.resize(imgs.size());
	for (auto& k : pairwise_matches) k.resize(imgs.size());
	if (ORDERED_INPUT)
		linear_pairwise_match();
	else
		pairwise_match();
	free_feature();
	//load_matchinfo(MATCHINFO_DUMP);
	if (DEBUG_OUT) {
		draw_matchinfo();         
		dump_matchinfo(MATCHINFO_DUMP);
	}
	assign_center();

	if (ESTIMATE_CAMERA)
		estimate_camera();
	else
		build_linear_simple();		// naive mode
	pairwise_matches.clear();
	// TODO automatically determine projection method
	if (ESTIMATE_CAMERA)
		//bundle.proj_method = ConnectedImages::ProjectionMethod::cylindrical;
		bundle.proj_method = ConnectedImages::ProjectionMethod::spherical;
	else
		bundle.proj_method = ConnectedImages::ProjectionMethod::flat;
	print_debug("Using projection method: %d\n", bundle.proj_method);
	bundle.update_proj_range();
	return bundle.blend();
}

bool Stitcher::match_image(
		const PairWiseMatcher& pwmatcher, int i, int j) {
	auto match = pwmatcher.match(i, j);
	//auto match = FeatureMatcher(feats[i], feats[j]).match();	// slow
	TransformEstimation transf(match, keypoints[i], keypoints[j],
			imgs[i].shape(), imgs[j].shape());	// from j to i
	MatchInfo info;
	bool succ = transf.get_transform(&info);
	if (!succ) {
		if (-(int)info.confidence >= 8)	// reject for geometry reason
			print_debug("Reject bad match with %d inlier from %d to %d\n",
					-(int)info.confidence, i, j);
		return false;
	}
	auto inv = info.homo.inverse();	// TransformEstimation ensures invertible
	inv.mult(1.0 / inv[8]);	// TODO more stable?
	print_debug(
			"Connection between image %d and %d, ninliers=%lu/%d=%lf, conf=%f\n",
			i, j, info.match.size(), match.size(),
			info.match.size() * 1.0 / match.size(),
			info.confidence);

	// fill in pairwise matches
	pairwise_matches[i][j] = info;
	info.homo = inv;
	info.reverse();
	pairwise_matches[j][i] = move(info);
	return true;
}

void Stitcher::pairwise_match() {
	GuardedTimer tm("pairwise_match()");
	size_t n = imgs.size();
	vector<pair<int, int>> tasks;
	REP(i, n) REPL(j, i + 1, n) tasks.emplace_back(i, j);

	PairWiseMatcher pwmatcher(feats);
#pragma omp parallel for schedule(dynamic)
	REP(k, (int)tasks.size()) {
		int i = tasks[k].first, j = tasks[k].second;
		match_image(pwmatcher, i, j);
	}
}

void Stitcher::linear_pairwise_match() {
	GuardedTimer tm("linear_pairwise_match()");
	int n = imgs.size();
	PairWiseMatcher pwmatcher(feats);
#pragma omp parallel for schedule(dynamic)
	REP(i, n) {
		int next = (i + 1) % n;
		if (!match_image(pwmatcher, i, next)) {
			if (i == n - 1)	// head and tail don't have to match
				continue;
			else
				error_exit(ssprintf("Image %d and %d don't match\n", i, next));
		}
		do {
			next = (next + 1) % n;
			if (next == i)
				break;
		} while (match_image(pwmatcher, i, next));
	}
}

void Stitcher::assign_center() {
	bundle.identity_idx = imgs.size() >> 1;
	//bundle.identity_idx = 0;
}

void Stitcher::estimate_camera() {
	vector<Shape2D> shapes;
	for (auto& m: imgs) shapes.emplace_back(m.shape());
	auto cameras = CameraEstimator{pairwise_matches, shapes}.estimate();

	// produced homo operates on [0,w] coordinate
	REP(i, imgs.size()) {
		bundle.component[i].homo_inv = cameras[i].K() * cameras[i].R;
		bundle.component[i].homo = cameras[i].Rinv() * cameras[i].K().inverse();
	}
}

void Stitcher::build_linear_simple() {
	// TODO bfs over pairwise to build bundle
	// assume pano pairwise
	int n = imgs.size(), mid = bundle.identity_idx;
	bundle.component[mid].homo = Homography::I();

	auto& comp = bundle.component;

	// accumulate the transformations
	if (mid + 1 < n) {
		comp[mid+1].homo = pairwise_matches[mid][mid+1].homo;
		REPL(k, mid + 2, n)
			comp[k].homo = comp[k - 1].homo * pairwise_matches[k-1][k].homo;
	}
	if (mid - 1 >= 0) {
		comp[mid-1].homo = pairwise_matches[mid][mid-1].homo;
		REPD(k, mid - 2, 0)
			comp[k].homo = comp[k + 1].homo * pairwise_matches[k+1][k].homo;
	}
	// now, comp[k]: from k to identity

	bundle.shift_all_homo();
	bundle.calc_inverse_homo();
}

}	// namepsace pano

