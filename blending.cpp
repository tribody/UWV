
//for 魔改blend()函数
#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <cassert>
#include <memory>
#include "stitcher_image.h"
#include "projection.h"
#include "lib/config.h"
#include "lib/timer.h"
#include "multiband.h"
#include "lib/imgproc.h"
#include "blender.h"
//for 魔改blend()函数

#include "blending.h"


void blendingGPU_CUDA(vector<Mat32f> & input_imgs, vector<Homography> & homogs, vector<vector<Coor>>& imgs_ranges_minNmax, vector<Vec2D>& final_proj_range_idnt, Coor& target_size, Vec2D& final_resolution)
{
	;
}


Mat32f blendingCPU(vector<Mat32f> & input_imgs, vector<Homography> & homogs, vector<vector<Coor>>& imgs_ranges_minNmax, vector<Vec2D>& final_proj_range_idnt, Coor& target_size, Vec2D& final_resolution)
{
	// get output/size
	Mat32f target(target_size.y, target_size.x, 3);
	Mat<float> weight(target_size.y, target_size.x, 1);
	memset(weight.ptr(), 0, target_size.y * target_size.x * sizeof(float));
	fill(target, Color::BLACK);
	int k;
	
	// get range

	// blending
#pragma omp parallel for schedule(dynamic)
	for (int k = 0; k < input_imgs.size(); k++) {
		auto& img = input_imgs[k];
		auto& range = imgs_ranges_minNmax[k];
		auto& homogra = homogs[k];
#pragma omp parallel for schedule(dynamic)
		for (int i = range[0].y; i < range[1].y; ++i) {
			float *row = target.ptr(i);
			float *wrow = weight.ptr(i);
#pragma omp parallel for schedule(dynamic)
			for (int j = range[0].x; j < range[1].x; ++j) {
				Vec2D img_coor;
				Vec2D c = Vec2D(j, i) * final_resolution + final_proj_range_idnt[0]; // 
				Vec homogen = Vec(sin(c.x), tan(c.y), cos(c.x)); // proj2homogen, in spherical mode
				Vec ret_1 = homogra.inverse().trans(homogen);
				if (ret_1.z < 0)
					img_coor = Vec2D{ -10, -10 };	// was projected to the other side of the lens, discard
				double denorm = 1.0 / ret_1.z;
				img_coor = Vec2D{ ret_1.x*denorm, ret_1.y*denorm };
				if (img_coor.x < 0 || img_coor.x >= img.width() || img_coor.y < 0 || img_coor.y >= img.height())
					img_coor = Vec2D::NaN();

				if (img_coor.isNaN()) continue;
				float r = img_coor.y, col = img_coor.x;
				auto color = interpolate(img, r, col);
				if (color.x < 0) continue;
				float w = 0.5 - fabs(col / img.width() - 0.5);
				if (!(config::ORDERED_INPUT)) /* blend both direction */
					w *= (0.5 - fabs(r / img.height() - 0.5));
				color *= w;

				row[j * 3] += color.x;
				row[j * 3 + 1] += color.y;
				row[j * 3 + 2] += color.z;
				wrow[j] += w;

			}
		}
	}

//#pragma omp parallel for schedule(dynamic)
	REP(i, target.height()) {
		auto row = target.ptr(i);
		auto wrow = weight.ptr(i);
		REP(j, target.width()) {
			if (wrow[j]) {
				*(row++) /= wrow[j]; *(row++) /= wrow[j]; *(row++) /= wrow[j];
			}
			else {
				*(row++) = -1; *(row++) = -1; *(row++) = -1;
			}
		}
	}


	return target;
}


// seperate from YW's blender
Mat32f blend_YW_ver(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs)
{
	ConnectedImages cp_bundle;
	std::vector<ImageRef> imgs;
	for (auto& n : std::move(input_imgs))
		imgs.emplace_back(n);
	cp_bundle.component.resize(imgs.size());
	REP(i, imgs.size()) {
		cp_bundle.component[i].imgptr = &imgs[i];
		cp_bundle.component[i].imgptr->load();
	}

	cp_bundle.identity_idx = imgs.size() >> 1;
	// produced homo operates on [0,w] coordinate
	REP(i, imgs.size()) {
		cp_bundle.component[i].homo_inv = homogs[i].inverse();
		cp_bundle.component[i].homo = homogs[i];
	}

	if (config::ESTIMATE_CAMERA)
		//bundle.proj_method = ConnectedImages::ProjectionMethod::cylindrical;
		cp_bundle.proj_method = ConnectedImages::ProjectionMethod::spherical;
	else
		cp_bundle.proj_method = ConnectedImages::ProjectionMethod::flat;
	cp_bundle.update_proj_range();

	return cp_bundle.blend();;
}

namespace flat_mode {
	static inline Vec2D homogen2proj(const Vec &coord) {
		return Vec2D(coord.x / coord.z, coord.y / coord.z);
	}

	static inline Vec proj2homogen(const Vec2D &coord) {
		return Vec(coord.x, coord.y, 1);
	}
}

namespace cylindrical_mode {
	static inline Vec2D homogen2proj(const Vec &coord) {
		return Vec2D(atan2(coord.x, coord.z),
			coord.y / (hypot(coord.x, coord.z)));
	}

	static inline Vec proj2homogen(const Vec2D &coord) {
		return Vec(sin(coord.x), coord.y, cos(coord.x));
	}
}


namespace spherical_mode {
	static inline Vec2D homogen2proj(const Vec &coord) {
		return Vec2D(atan2(coord.x, coord.z),
			atan2(coord.y, hypot(coord.x, coord.z)));
	}

	static inline Vec proj2homogen(const Vec2D &coord) {
		return Vec(sin(coord.x), tan(coord.y), cos(coord.x));
	}
}

