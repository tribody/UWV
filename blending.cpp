
# include "blending.h"

namespace pano {

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



		// blending
		Mat32f *output_img = new Mat32f(output_height, output_width, 3);



		return *output_img;
	}

}