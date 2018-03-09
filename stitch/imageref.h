//File: imageref.h
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include <string>
#include <memory>
#include "lib/mat.h"
#include "lib/imgproc.h"
#include "match_info.h"

namespace pano {
		// A transparent reference to a image in file
		struct ImageRef {
			Mat32f fname_2_mat32f; /* 此处修改stitcher */
			Mat32f* img = nullptr;
			int _width, _height;

			void load() {
				if (img) return;
				/* 此处修改stitcher */
				img = new Mat32f{ fname_2_mat32f };
				_width = img->width();
				_height = img->height();
			}
			void change_imgref(const Mat32f& mat_insert) {
				img = new Mat32f{ mat_insert };
				_width = img->width();
				_height = img->height();
			}

			void release() { if (img) delete img; img = nullptr; }

			int width() const { return _width; }
			int height() const { return _height; }
			Shape2D shape() const { return {_width, _height}; }

			ImageRef(const Mat32f& fname_2_mat32f): fname_2_mat32f(fname_2_mat32f) {}

			~ImageRef() { if (img) delete img; }
		};

}
