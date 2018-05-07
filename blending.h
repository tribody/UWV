// file: blending.h
// do blending by Homographies and Images
#include <A.h>

#include <vector>
//#include <mat.h>
#include "homography.h"

namespace pano {


Mat32f blendingCPU(std::vector<Mat32f> & input_imgs, std::vector<Homography> & homogs)
{
	// get output size
	A_long output_height, output_width;
	A_long blend_left_bound, blend_right_bound, blend_top_bound, blend_bottom_bound;
	Coor corner00(0, 0), corner01(input_imgs[0].width, 0), corner10(0, -(input_imgs[0].height)), corner11(input_imgs[0].width, -(input_imgs[0].height));
	std::vector<Coor> corners{ corner00, corner01, corner10, corner11 };
	for (auto &H : homogs)
	{
		for (auto & C : corners)
		{
			;
		}
	}

	// blending
	Mat32f output_img; // (output_height, output_width, 3);



	return output_img;
}




}