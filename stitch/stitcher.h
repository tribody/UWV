// File: stitcher.h
// Date: Sat May 04 22:36:30 2013 +0800
// Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include <memory>
#include <vector>
#include "lib/mat.h"
#include "lib/utils.h"
#include "stitcher_image.h"
#include "stitcherbase.h"

namespace pano {

// forward declaration
class Homography;
class MatchData;
struct MatchInfo;
class PairWiseMatcher;

class Stitcher : public StitcherBase {
	private:
		// transformation and metadata of each image
		ConnectedImages bundle;

		// 2d array of all matches
		// pairwise_matches[i][j].homo transform j to i
		std::vector<std::vector<MatchInfo>> pairwise_matches;

		// match two images
		bool match_image(const PairWiseMatcher&, int i, int j);

		// pairwise matching of all images
		void pairwise_match();
		// equivalent to pairwise_match when dealing with linear images
		void linear_pairwise_match();

		// assign a center to be identity
		void assign_center();

		// build by estimating camera parameters
		void estimate_camera();

		// naively build panorama assuming linear imgs
		void build_linear_simple();

		// for debug
		void draw_matchinfo();
		void dump_matchinfo(const char*) const;
		void load_matchinfo(const char*);
	public:

		template<typename U, typename X =
			disable_if_same_or_derived<Stitcher, U>>
			Stitcher(U&& i) : StitcherBase(std::forward<U>(i)) {
				bundle.component.resize(imgs.size());
				REP(i, imgs.size())
					bundle.component[i].imgptr = &imgs[i];
			}

		virtual Mat32f build();
		virtual void only_clac_homogras();
		Mat32f only_render();
		void change_imgsref(std::vector<Mat32f>& mat_imgs);
		void return_homogras(std::vector<Homography> & result_homogras);
		void return_render_params(std::vector<std::vector<Coor>> & imgs_ranges_minNmax,
									std::vector<Vec2D>& final_proj_range_idnt,
			                        Coor& target_size,
									Vec2D& final_resolution);
};

}
