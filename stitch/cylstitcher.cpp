//File: cylstitcher.cc
//Author: Yuxin Wu <ppwwyyxx@gmail.com>

#include "cylstitcher.h"

#include "lib/timer.h"
#include "lib/config.h"
#include "lib/imgproc.h"
#include "feature/matcher.h"
#include "transform_estimate.h"
#include "blender.h"
#include "match_info.h"
#include "warp.h"
#define PLG_IN

using namespace config;
using namespace std;

namespace pano {

void CylinderStitcher::change_imgsref(std::vector<Mat32f>& mat_imgs) {
	// bundle.component[0].imgptr // 操作/替换 被blend的图片
	for (int k = 0; k < imgs.size(); k++) {
		imgs[k].change_oneimgref(mat_imgs[k]);
	}
}

Mat32f CylinderStitcher::only_render() {
	auto ret = bundle.blend();
	return perspective_correction(ret);
}

void CylinderStitcher::only_clac_homogras() {
	calc_feature();
	bundle.identity_idx = imgs.size() >> 1;
	build_warp();
	free_feature();
	bundle.proj_method = ConnectedImages::ProjectionMethod::flat;
	bundle.update_proj_range();
	//auto ret = bundle.blend();
	//return perspective_correction(ret);
}

void CylinderStitcher::return_render_params(std::vector<Homography> & result_homogras, std::vector<Homography> & result_homogs_invers) {
	for (auto &i: bundle.component) {
		result_homogras.emplace_back(i.homo);
		result_homogs_invers.emplace_back(i.homo_inv);
	}
}

void CylinderStitcher::build_warp() {
	GuardedTimer tm("build_warp()");
	int n = imgs.size(), mid = bundle.identity_idx;
	REP(i, n) bundle.component[i].homo = Homography::I();

	Timer timer;
	vector<MatchData> matches;		// matches[k]: k,k+1
	PairWiseMatcher pwmatcher(feats);
	matches.resize(n - 1);
#pragma omp parallel for schedule(dynamic)
	REP(k, n - 1)
		matches[k] = pwmatcher.match(k, (k + 1) % n);
#ifndef PLG_IN
	print_debug("match time: %lf secs\n", timer.duration());
#endif // !PLG_IN


	vector<Homography> bestmat;

	float minslope = numeric_limits<float>::max();
	float bestfactor = 1;
	if (n - mid > 1) {
		float newfactor = 1;
		// XXX: ugly
		float slope = update_h_factor(newfactor, minslope, bestfactor, bestmat, matches);
		if (bestmat.empty())
			error_exit("Failed to find hfactor");
		float centerx1 = 0, centerx2 = bestmat[0].trans2d(0, 0).x;
		float order = (centerx2 > centerx1 ? 1 : -1);
		REP(k, 3) {
			if (fabs(slope) < SLOPE_PLAIN) break;
			newfactor += (slope < 0 ? order : -order) / (5 * pow(2, k));
			slope = update_h_factor(newfactor, minslope, bestfactor, bestmat, matches);
		}
	}
	print_debug("Best hfactor: %lf\n", bestfactor);
	CylinderWarper warper(bestfactor);
	REP(k, n) imgs[k].load();
#pragma omp parallel for schedule(dynamic)
	REP(k, n) warper.warp(*imgs[k].img, keypoints[k]);

	// accumulate
	REPL(k, mid + 1, n) bundle.component[k].homo = move(bestmat[k - mid - 1]);
#pragma omp parallel for schedule(dynamic)
	REPD(i, mid - 1, 0) {
		matches[i].reverse();
		MatchInfo info;
		bool succ = TransformEstimation(
			matches[i], keypoints[i + 1], keypoints[i],
			imgs[i + 1].shape(), imgs[i].shape()).get_transform(&info);
		// Can match before, but not here. This would be a bug.
		if (!succ)
			error_exit(ssprintf("Failed to match between image %d and %d.", i, i + 1));
		// homo: operate on half-shifted coor
		bundle.component[i].homo = info.homo;
	}
	REPD(i, mid - 2, 0)
		bundle.component[i].homo = bundle.component[i + 1].homo * bundle.component[i].homo;

	bundle.shift_all_homo();
	bundle.calc_inverse_homo();
}

float CylinderStitcher::update_h_factor(float nowfactor,
		float & minslope, // 所有图片中心到全局坐标系原点的连线的最小斜率，
		float & bestfactor,
		vector<Homography>& mat, // 到参考图的单应
		const vector<MatchData>& matches) {
	const int n = imgs.size(), mid = bundle.identity_idx;
	const int start = mid, end = n, len = end - start;

	vector<Shape2D> nowimgs;
	vector<vector<Vec2D>> nowkpts;
	REPL(k, start, end) {
		nowimgs.emplace_back(imgs[k].shape());
		nowkpts.push_back(keypoints[k]);
	}			// nowfeats[0] == feats[mid]

	CylinderWarper warper(nowfactor);
#pragma omp parallel for schedule(dynamic)
	REP(k, len)
		warper.warp(nowimgs[k], nowkpts[k]);

	vector<Homography> nowmat;		// size = len - 1
	nowmat.resize(len - 1);
	bool failed = false;
#pragma omp parallel for schedule(dynamic)
	REPL(k, 1, len) {
		MatchInfo info;
		bool succ = TransformEstimation(
				matches[k - 1 + mid], nowkpts[k - 1], nowkpts[k],
				nowimgs[k-1], nowimgs[k]).get_transform(&info);
		if (! succ)
			failed = true;
		//error_exit("The two image doesn't match. Failed");
		nowmat[k-1] = info.homo;
	}
	if (failed) return 0;

	REPL(k, 1, len - 1)
		nowmat[k] = nowmat[k - 1] * nowmat[k];	// transform to nowimgs[0] == imgs[mid]

	// check the slope of the result image
	Vec2D center2 = nowmat.back().trans2d(0, 0);
	const float slope = center2.y/ center2.x;
	print_debug("slope: %lf\n", slope);
	if (update_min(minslope, fabs(slope))) {
		bestfactor = nowfactor;
		mat = move(nowmat);
	}
	return slope;
}

Mat32f CylinderStitcher::perspective_correction(const Mat32f& img) {
	int w = img.width(), h = img.height();
	int refw = imgs[bundle.identity_idx].width(),
			refh = imgs[bundle.identity_idx].height();
	auto homogen2proj = bundle.get_homogen2proj();
	Vec2D proj_min = bundle.proj_range.min;

	vector<Vec2D> corners;
	auto cur = &(bundle.component.front());
	auto to_ref_coor = [&](Vec2D v) {
		v.x *= cur->imgptr->width(), v.y *= cur->imgptr->height();
		Vec homo = cur->homo.trans(v);
		homo.x /= refw, homo.y /= refh;
		homo.x += 0.5 * homo.z, homo.y += 0.5 * homo.z;
		Vec2D t_corner = homogen2proj(homo);
		t_corner.x *= refw, t_corner.y *= refh;
		t_corner = t_corner - proj_min;
		corners.push_back(t_corner);
	};
	to_ref_coor(Vec2D(-0.5, -0.5));
	to_ref_coor(Vec2D(-0.5, 0.5));
	cur = &(bundle.component.back());
	to_ref_coor(Vec2D(0.5, -0.5));
	to_ref_coor(Vec2D(0.5, 0.5));

	// stretch the four corner to rectangle
	vector<Vec2D> corners_std = {
		Vec2D(0, 0), Vec2D(0, h),
		Vec2D(w, 0), Vec2D(w, h)};
	Matrix m = getPerspectiveTransform(corners, corners_std);
	Homography inv(m);

	LinearBlender blender;
	Mat32f tmp_mat{};
	ImageRef tmp(tmp_mat);
	tmp.img = new Mat32f(img);
	tmp._width = img.width(), tmp._height = img.height();
	blender.add_image(
			Coor(0,0), Coor(w,h), tmp,
			[=](Coor c) -> Vec2D {
		return inv.trans2d(Vec2D(c.x, c.y));
	});
	return blender.run();
}

}
