
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

Mat32f blendingCPU(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs)
{
	// get output size
	A_long output_height = 0, output_width = 0;
	A_long blend_left_bound = 0, blend_right_bound = 0, blend_top_bound = 0, blend_bottom_bound = 0;
	A_long coor_shift_x, coor_shift_y;
	Vec corner00 = Vec(0, 0, 1);
	Vec corner01 = Vec(input_imgs[0].width(), 0, 1);
	Vec corner10 = Vec(0, input_imgs[0].height(), 1);
	Vec corner11 = Vec(input_imgs[0].width(), input_imgs[0].height(), 1);
	std::vector<Vec> corners{ corner00, corner01, corner10, corner11 };
	for (auto &H : homogs)
	{
		Vec2D x_y = Vec2D(0, 0);
		for (auto & C : corners)
		{
			x_y = H.trans_normalize(C);
			if (x_y.x < blend_left_bound)blend_left_bound = x_y.x;
			if (x_y.x > blend_right_bound)blend_right_bound = x_y.x;
			if (x_y.y < blend_bottom_bound)blend_bottom_bound = x_y.y;
			if (x_y.y > blend_top_bound)blend_top_bound = x_y.y;
		}
	}

	output_height = blend_top_bound - blend_bottom_bound;
	output_width = blend_right_bound - blend_left_bound;

	// blending
	Mat32f *output_img = new Mat32f(output_height, output_width, 3);



	return *output_img;
}


// stretch from YW's blender
Mat32f blend_YW_ver(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs)
{
	ConnectedImages cp_bundle;
	std::vector<ImageRef> imgs;
	for (auto& n : std::move(input_imgs))
		imgs.emplace_back(n);
	cp_bundle.component.resize(imgs.size());
	REP(i, imgs.size())
		cp_bundle.component[i].imgptr = &imgs[i];

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
	return cp_bundle.blend();
}

