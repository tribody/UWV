/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*
	UWV.h
*/

#pragma once

#define _USE_MATH_DEFINES
#define NOMINMAX // for max confliction

#ifndef UWV_H
#define UWV_H

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels; 
								// AE_Effect.h checks for this.

#include "AEConfig.h"

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "AEFX_SuiteHelper.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "UWV_Strings.h"

#include "lib/mat.h"
#include <vector>
#include <cmath>
#include "feature/extrema.h"
#include "feature/matcher.h"
#include "feature/orientation.h"
#include "lib/config.h"
#include "lib/geometry.h"
#include "lib/imgproc.h"
#include "lib/planedrawer.h"
#include "lib/polygon.h"
#include "lib/timer.h"
#include "stitch/cylstitcher.h"
#include "stitch/match_info.h"
#include "stitch/stitcher.h"
#include "stitch/transform_estimate.h"
#include "stitch/warp.h"
#include <ctime>
#include <cassert>

#include "imgproc.h"
#include "stitch/blender.h"
#include "blending.h"
#define PLG_IN

using namespace std;
using namespace pano;
using namespace config;



/* Versioning information */

#define	MAJOR_VERSION				1
#define	MINOR_VERSION				0
#define	BUG_VERSION					0
#define	STAGE_VERSION				PF_Stage_DEVELOP
#define	BUILD_VERSION				1



/* Macros */
//Lapped ratio ajustment
#define UWV_RATIO_MIN			30
#define UWV_RATIO_MAX			80
#define UWV_RATIO_DFLT			50
//Focal length estimation
#define	UWV_ESTIMATION_MIN		5
#define	UWV_ESTIMATION_MAX		9
#define	UWV_ESTIMATION_DFLT	7.033f
#define UWV_ESTIMATION_PREC	3
//Frame shift
#define UWV_FRAME_SHIFT_MIN	-5
#define UWV_FRAME_SHIFT_MAX	5
#define UWV_FRAME_SHIFT_DFLT	0
//Projection type
#define UWV_METHOD_NUM			2	
#define	UWV_METHOD_DFLT		1

/* 这里决定了控件在ParamenterUI上的位置（下标） */
/* 这里的两个枚举是一一对应的，都是某个控件的位置（下标） */
enum {
	UWV_INPUT = 0,

	/* Import settings */
	UWV_IMPORT_BEG,
	// 这里，各个 VIEW 的ID从 2 到 13，
	UWV_IMPORT_VIEW_1,
	UWV_IMPORT_VIEW_2,
	UWV_IMPORT_VIEW_3,
	UWV_IMPORT_VIEW_4,
	UWV_IMPORT_VIEW_5,
	UWV_IMPORT_VIEW_6,
	UWV_IMPORT_VIEW_7,
	UWV_IMPORT_VIEW_8,
	UWV_IMPORT_VIEW_9,
	UWV_IMPORT_VIEW_10,
	UWV_IMPORT_VIEW_11,
	UWV_IMPORT_VIEW_12,
	// UWV_IMPORT_CORRECTORDER, // 是否调整图片顺序的checkbox，不使用
	UWV_IMPORT_END,

	/* Presettings for the users */
	/* the Lapped ratio for the left and right videos */
	UWV_PERCENTAGE_BEG,
	// 这里，各个 PERCENTAGE_RATIO 的ID从 16 到 27，
	UWV_PERCENTAGE_RATIO_1,
	UWV_PERCENTAGE_RATIO_2,
	UWV_PERCENTAGE_RATIO_3,
	UWV_PERCENTAGE_RATIO_4,
	UWV_PERCENTAGE_RATIO_5,
	UWV_PERCENTAGE_RATIO_6,
	UWV_PERCENTAGE_RATIO_7,
	UWV_PERCENTAGE_RATIO_8,
	UWV_PERCENTAGE_RATIO_9,
	UWV_PERCENTAGE_RATIO_10,
	UWV_PERCENTAGE_RATIO_11,
	UWV_PERCENTAGE_RATIO_12,
	UWV_PERCENTAGE_END,

	/* Frame shift */
	UWV_FRAME_SHIFT_BEG,
	// 这里，各个 FRAME_SHIFT 的ID从 30 到 41，
	UWV_FRAME_SHIFT_1,
	UWV_FRAME_SHIFT_2,
	UWV_FRAME_SHIFT_3,
	UWV_FRAME_SHIFT_4,
	UWV_FRAME_SHIFT_5,
	UWV_FRAME_SHIFT_6,
	UWV_FRAME_SHIFT_7,
	UWV_FRAME_SHIFT_8,
	UWV_FRAME_SHIFT_9,
	UWV_FRAME_SHIFT_10,
	UWV_FRAME_SHIFT_11,
	UWV_FRAME_SHIFT_12,
	UWV_FRAME_SHIFT_END,

	/* Stitch settings */
	UWV_SETTINGS_BEG,
	UWV_PROJECTION_METHOD,
	UWV_FOCAL,
	UWV_HOMOGRAPHY,
	UWV_RENDER,
	UWV_SETTINGS_END,

	UWV_NUM_PARAMS
};

enum {
	IMPORT_BEG_ID = 1,

	// 这里，各个 VIEW 的ID从 2 到 13，
	UWV_IMPORT_VIEW_1_ID,
	UWV_IMPORT_VIEW_2_ID,
	UWV_IMPORT_VIEW_3_ID,
	UWV_IMPORT_VIEW_4_ID,
	UWV_IMPORT_VIEW_5_ID,
	UWV_IMPORT_VIEW_6_ID,
	UWV_IMPORT_VIEW_7_ID,
	UWV_IMPORT_VIEW_8_ID,
	UWV_IMPORT_VIEW_9_ID,
	UWV_IMPORT_VIEW_10_ID,
	UWV_IMPORT_VIEW_11_ID,
	UWV_IMPORT_VIEW_12_ID,
	// CORRECTORDER_ID, // 不使用
	IMPORT_END_ID,

	PERCENTAGE_BEG_ID,
	// 这里，各个 PERCENTAGE_RATIO 的ID从 16 到 27，
	UWV_PERCENTAGE_RATIO_1_ID,
	UWV_PERCENTAGE_RATIO_2_ID,
	UWV_PERCENTAGE_RATIO_3_ID,
	UWV_PERCENTAGE_RATIO_4_ID,
	UWV_PERCENTAGE_RATIO_5_ID,
	UWV_PERCENTAGE_RATIO_6_ID,
	UWV_PERCENTAGE_RATIO_7_ID,
	UWV_PERCENTAGE_RATIO_8_ID,
	UWV_PERCENTAGE_RATIO_9_ID,
	UWV_PERCENTAGE_RATIO_10_ID,
	UWV_PERCENTAGE_RATIO_11_ID,
	UWV_PERCENTAGE_RATIO_12_ID,
	PERCENTAGE_END_ID,

	FRAME_SHIFT_BEG_ID,
	// 这里，各个 FRAME_SHIFT 的ID从 30 到 41，
	UWV_FRAME_SHIFT_1_ID,
	UWV_FRAME_SHIFT_2_ID,
	UWV_FRAME_SHIFT_3_ID,
	UWV_FRAME_SHIFT_4_ID,
	UWV_FRAME_SHIFT_5_ID,
	UWV_FRAME_SHIFT_6_ID,
	UWV_FRAME_SHIFT_7_ID,
	UWV_FRAME_SHIFT_8_ID,
	UWV_FRAME_SHIFT_9_ID,
	UWV_FRAME_SHIFT_10_ID,
	UWV_FRAME_SHIFT_11_ID,
	UWV_FRAME_SHIFT_12_ID,
	FRAME_SHIFT_END_ID,

	STITCH_BEG_ID,
	// PROJECTION_ID, // 原PROJECTION_METHOD_ID，即下一个
	METHOD_ID,
	FOCAL_ID,
	HOMOGRAPHY_ID,
	RENDER_ID,
	STITCH_END_ID,
};

/* custom struct */
typedef struct{
	A_long* frame_calc_h_ptr; // selected cunrrent time
	A_Boolean* flag_h_clac_for_render_ptr;
} mySequenceData;

typedef struct {
	A_long flat_frame_calc_h; // selected cunrrent time
	A_Boolean flat_flag_h_clac_for_render;
} flatSequenceData;

#ifdef __cplusplus
	extern "C" {
#endif

// 声明UWV中的函数
static PF_Err
MatToEw(PF_InData *in_data, Mat32f *imgMat, PF_EffectWorld *imgE);

static PF_Err
EwToMat(PF_InData *in_data, PF_EffectWorld *imgE, Mat32f& imgMat);
	
DllExport	PF_Err 
EntryPointFunc(	
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra) ;

#ifdef __cplusplus
}
#endif

#endif // UWV_H
