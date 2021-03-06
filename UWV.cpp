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

/*	UWV.cpp	

	This is a compiling husk of a project. Fill it in with interesting
	pixel processing code.
	
	Revision history: 

	1.0 (seemed like a good idea at the time)	bbb		6/1/2002

	1.0 Okay, I'm leaving the version at 1.0,	bbb		2/15/2006
		for obvious reasons; you're going to 
		copy these files directly! This is the
		first XCode version, though.

	1.0	Let's simplify this barebones sample	zal		11/11/2010

*/

#include "UWV.h"
//CUDA RunTime API
//#include <cuda_runtime.h>



A_long mesh_width_1, mesh_height_1, src_width=0, src_height=0; // 图片和网格大小信息

// I -- fundalmental matrix
// Homo -- transform matrix

// Processed Image Formats
A_long width_output;
A_long heigh_output;

// add by chromiumikx
PF_ParamDef checkout;
vector<PF_EffectWorld *>src_imgs( 12, NULL );

//float focal_length = UWV_FOCAL_LEN_DFLT;

bool flag_calc_homog = false;
bool flag_check_render = false;
bool flag_single_Stitcher = true;

//int UWV_proj_method = UWV_METHOD_DFLT;

// add by chromiumikx
int num_selected_imgs;
vector<float>lapped_ratios(12, UWV_RATIO_DFLT);
vector<int>frame_shifts(12, UWV_FRAME_SHIFT_DFLT);


Mat32f* mat_result_img_ptr;

// data to save
A_long key_frame_calc_h;
vector<Homography>* result_homogras; // (12, Homography::I()); 不必写定12个，按需要添加

// params for render
vector<vector<Coor>>* imgs_ranges_minNmax;
vector<Vec2D>* final_proj_range_idnt;
Coor* target_size;
Vec2D* final_resolution;


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											STR(StrID_Name), 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	// Here bind PiPL.r tightly ...
	out_data->out_flags =  PF_OutFlag_DEEP_COLOR_AWARE // just 16bpc, not 32bpc
						   |	PF_OutFlag_PIX_INDEPENDENT // pixel independent
						   |	PF_OutFlag_I_EXPAND_BUFFER // expand the output buffer
						   |	PF_OutFlag_SEND_UPDATE_PARAMS_UI // set to receive PF_Cmd_UPDATE_PARAMS_UI
		                   |    PF_OutFlag_CUSTOM_UI; // set to receive PF_Cmd_Event，不使用
	
	// honor the collapse ability of each parameters group
	out_data->out_flags2 = PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;

	return PF_Err_NONE;
}

static PF_Err
GlobalSetdown(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	return PF_Err_NONE;
}

static PF_Err
SequenceSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	mySequenceData* mySD_P;

	if (out_data->sequence_data) {
		PF_DISPOSE_HANDLE(out_data->sequence_data);
	}
	out_data->sequence_data = PF_NEW_HANDLE(sizeof(mySequenceData));
	if (!out_data->sequence_data) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	mySD_P = *(mySequenceData**)out_data->sequence_data;

	mySD_P->frame_calc_h_ptr = new A_long;
	mySD_P->flag_h_clac_for_render_ptr = new A_Boolean;
	mySD_P->homogs_ptr = new vector<Homography>;
	mySD_P->num_H = new A_long;
	mySD_P->imgs_ranges_minNmax_ptr = new vector<vector<Coor>>;
	mySD_P->final_proj_range_idnt_ptr = new vector<Vec2D>;
	mySD_P->target_size_ptr = new Coor;
	mySD_P->final_resolution_ptr = new Vec2D;
	
	*(mySD_P->frame_calc_h_ptr) = 0;
	*(mySD_P->flag_h_clac_for_render_ptr) = false;
	*(mySD_P->num_H) = 0;

	mySD_P->target_size_ptr->x = 0;
	mySD_P->target_size_ptr->y = 0;
	mySD_P->final_resolution_ptr->x = 0;
	mySD_P->final_resolution_ptr->y = 0;

	return PF_Err_NONE;
}

static PF_Err
SequenceSetdown(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	if (in_data->sequence_data) {
		mySequenceData *mySD_P = *(mySequenceData**)out_data->sequence_data;
		delete mySD_P->frame_calc_h_ptr;
		delete mySD_P->flag_h_clac_for_render_ptr;
		delete mySD_P->homogs_ptr;
		delete mySD_P->num_H;
		delete mySD_P->imgs_ranges_minNmax_ptr;
		delete mySD_P->final_proj_range_idnt_ptr;
		delete mySD_P->target_size_ptr;
		delete mySD_P->final_resolution_ptr;

		PF_DISPOSE_HANDLE(in_data->sequence_data);
		out_data->sequence_data = NULL;
	}
	return PF_Err_NONE;
}

static PF_Err
SequenceFlatten(
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Make a flat copy of whatever is in the unflat seq data handed to us. 
	if (in_data->sequence_data) {
		mySequenceData* mySD_P = reinterpret_cast<mySequenceData*>(DH(in_data->sequence_data));

		if (mySD_P) {
			PF_Handle flat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(flatSequenceData));

			if (flat_seq_dataH) {
				flatSequenceData*	flatSD_P = reinterpret_cast<flatSequenceData*>(suites.HandleSuite1()->host_lock_handle(flat_seq_dataH));

				if (flatSD_P) {
					AEFX_CLR_STRUCT(*flatSD_P);
					flatSD_P->flat_frame_calc_h = *(mySD_P->frame_calc_h_ptr);
					flatSD_P->flat_flag_h_clac_for_render = *(mySD_P->flag_h_clac_for_render_ptr);
					flatSD_P->flat_num_H = *(mySD_P->num_H);
					flatSD_P->flat_target_size.data[0] = mySD_P->target_size_ptr->x;
					flatSD_P->flat_target_size.data[1] = mySD_P->target_size_ptr->y;
					flatSD_P->flat_final_resolution.data[0] = mySD_P->final_resolution_ptr->x;
					flatSD_P->flat_final_resolution.data[1] = mySD_P->final_resolution_ptr->y;
					
					int cntr_vec = 0;
					for (auto &i_homo : *(mySD_P->homogs_ptr))
					{
						data_H data_H;
						for (int i=0; i < 9; i++) 
						{
							data_H.data[i] = i_homo.data[i];
						}
						flatSD_P->flat_homogs[cntr_vec] = data_H;
						cntr_vec = cntr_vec + 1;
					}

					cntr_vec = 0;
					for(auto& i_img_ranges: *(mySD_P->imgs_ranges_minNmax_ptr))
					{
						data_Vec2D data_Vec2D_min;
						data_Vec2D data_Vec2D_max;
						data_Vec2D_min.data[0] = i_img_ranges[0].x;
						data_Vec2D_min.data[1] = i_img_ranges[0].y;
						data_Vec2D_max.data[0] = i_img_ranges[1].x;
						data_Vec2D_max.data[1] = i_img_ranges[1].y;
						flatSD_P->flat_imgs_ranges_minNmax[cntr_vec][0] = data_Vec2D_min;
						flatSD_P->flat_imgs_ranges_minNmax[cntr_vec][1] = data_Vec2D_max;
						cntr_vec = cntr_vec + 1;
					}					
					
				    cntr_vec = 0;
					for(auto& final_range: *(mySD_P->final_proj_range_idnt_ptr))
					{
						data_Vec2D data_Vec2D;
						data_Vec2D.data[0] = final_range.x;
						data_Vec2D.data[1] = final_range.y;
						flatSD_P->flat_final_proj_range_idnt[cntr_vec] = data_Vec2D;
						cntr_vec = cntr_vec + 1;
					}

					suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
					out_data->sequence_data = flat_seq_dataH;
					suites.HandleSuite1()->host_unlock_handle(flat_seq_dataH);
				}
			}
			else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	}
	else {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

// get back the sequenced datas
static PF_Err
SequenceResetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// We got here because we're either opening a project w/saved (flat) sequence data,
	// or we've just been asked to flatten our sequence data (for a save) and now 
	// we're blowing it back up.

	if (in_data->sequence_data) {
		// get the flatten sequence data from the disk?
		flatSequenceData *flatSD_P = reinterpret_cast<flatSequenceData*>(DH(in_data->sequence_data));

		if (flatSD_P) {
			PF_Handle unflat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(mySequenceData));

			if (unflat_seq_dataH) {
				// produce an unflatten sequence pointer
				mySequenceData* mySD_P = reinterpret_cast<mySequenceData*>(suites.HandleSuite1()->host_lock_handle(unflat_seq_dataH));

				if (mySD_P) {
					AEFX_CLR_STRUCT(*mySD_P);
					mySD_P->frame_calc_h_ptr = new A_long;
					mySD_P->flag_h_clac_for_render_ptr = new A_Boolean;
					mySD_P->homogs_ptr = new vector<Homography>;
					mySD_P->num_H = new A_long;
					mySD_P->imgs_ranges_minNmax_ptr = new vector<vector<Coor>>;
					mySD_P->final_proj_range_idnt_ptr = new vector<Vec2D>;
					mySD_P->target_size_ptr = new Coor;
					mySD_P->final_resolution_ptr = new Vec2D;
		
					*(mySD_P->frame_calc_h_ptr) = flatSD_P->flat_frame_calc_h;
					*(mySD_P->flag_h_clac_for_render_ptr) = flatSD_P->flat_flag_h_clac_for_render;
					*(mySD_P->num_H) = flatSD_P->flat_num_H;
					mySD_P->target_size_ptr->x = int(flatSD_P->flat_target_size.data[0]);
					mySD_P->target_size_ptr->y = int(flatSD_P->flat_target_size.data[1]);
					mySD_P->final_resolution_ptr->x = flatSD_P->flat_final_resolution.data[0];
					mySD_P->final_resolution_ptr->y = flatSD_P->flat_final_resolution.data[1];
					
					for (int j = 0; j < *(mySD_P->num_H); j++)
					{
						Homography* tmp_homog = new Homography(flatSD_P->flat_homogs[j].data);
						mySD_P->homogs_ptr->emplace_back(*tmp_homog);
						delete tmp_homog;

						Coor img_range_min{ int((flatSD_P->flat_imgs_ranges_minNmax)[j][0].data[0]) ,int((flatSD_P->flat_imgs_ranges_minNmax)[j][0].data[1]) },
							img_range_max{ int((flatSD_P->flat_imgs_ranges_minNmax)[j][1].data[0]) ,int((flatSD_P->flat_imgs_ranges_minNmax)[j][1].data[1]) };
						vector<Coor>* minNmax = new vector<Coor>;
						minNmax->emplace_back(img_range_min);
						minNmax->emplace_back(img_range_max);
						mySD_P->imgs_ranges_minNmax_ptr->emplace_back(*minNmax);
						delete minNmax; minNmax = NULL;

						Vec2D final_range_min{ flatSD_P->flat_final_proj_range_idnt[0].data[0], flatSD_P->flat_final_proj_range_idnt[0].data[1] };
						Vec2D final_range_max{ flatSD_P->flat_final_proj_range_idnt[1].data[0], flatSD_P->flat_final_proj_range_idnt[1].data[1] };
						mySD_P->final_proj_range_idnt_ptr->emplace_back(final_range_min);
						mySD_P->final_proj_range_idnt_ptr->emplace_back(final_range_max);

					}

					out_data->sequence_data = unflat_seq_dataH;
					suites.HandleSuite1()->host_unlock_handle(unflat_seq_dataH);
				}
			}
			else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	}
	return err;
}


static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	/* Import */
	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_START_COLLAPSED; // Controls the twirl-state of a topic spinner
	PF_ADD_TOPIC(STR(StrID_Import), IMPORT_BEG_ID);
	// ID和StrID位置相差2: ID + 2 = StrID
	int i;
	for (i = 0; i < 12; i++) {
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE	// set to receive PF_Cmd_USER_CHANGED_PARAM
			| PF_ParamFlag_CANNOT_TIME_VARY; // do not vary with time, otherwise will be changed after first applied
		PF_ADD_LAYER(STR(i+UWV_IMPORT_VIEW_1_ID+2), -1, i+ UWV_IMPORT_VIEW_1_ID);
	}
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(IMPORT_END_ID);

	/* Lapped percentage */
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_Percentage), PERCENTAGE_BEG_ID);
	for (i = 0; i < 12; i++) {
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE
			      | PF_ParamFlag_CANNOT_TIME_VARY
			      | PF_ParamFlag_START_COLLAPSED;
		PF_ADD_SLIDER(STR(i + UWV_PERCENTAGE_RATIO_1_ID + 2),
			UWV_RATIO_MIN,
			UWV_RATIO_MAX,
			UWV_RATIO_MIN,
			UWV_RATIO_MAX,
			UWV_RATIO_DFLT,
			i + UWV_PERCENTAGE_RATIO_1_ID);
	}
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(PERCENTAGE_END_ID);

	/* Frame shift */
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_Frame_Shift), FRAME_SHIFT_BEG_ID);
	for (i = 0; i < 12; i++) {
		AEFX_CLR_STRUCT(def);
		def.flags = PF_ParamFlag_SUPERVISE 
			      | PF_ParamFlag_CANNOT_TIME_VARY;
		PF_ADD_SLIDER(STR(i + UWV_FRAME_SHIFT_1_ID + 2),
			UWV_FRAME_SHIFT_MIN,
			UWV_FRAME_SHIFT_MAX,
			UWV_FRAME_SHIFT_MIN,
			UWV_FRAME_SHIFT_MAX,
			UWV_FRAME_SHIFT_DFLT, // DFLT: 默认值
			i + UWV_FRAME_SHIFT_1_ID);
	}
	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(FRAME_SHIFT_END_ID);

	/* SETTING stitching */
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_UWV), STITCH_BEG_ID);

	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_SUPERVISE;
	PF_ADD_POPUP(STR(StrID_Projection),
		UWV_METHOD_NUM,
		UWV_METHOD_DFLT,
		STR(StrID_Projection_Type),
		METHOD_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDER(STR(StrID_Focal_Lenghth),
		UWV_FOCLA_LEN_MIN,
		UWV_FOCAL_LEN_MAX,
		UWV_FOCLA_LEN_MIN,
		UWV_FOCAL_LEN_MAX,
		AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE,
		UWV_FOCAL_LEN_DFLT,
		UWV_FOCAL_LEN_PREC,
		0,
		1,
		FOCAL_ID);

	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_SUPERVISE;
	PF_ADD_POPUP(STR(StrID_Multi_Band_Blend),
		5,
		1,
		STR(StrID_Multi_Band_Blend_Num),
		UWV_MULTI_BAND_NUM_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Homography),
		"Calculate",
		0,
		PF_ParamFlag_SUPERVISE,
		HOMOGRAPHY_ID);

	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_SUPERVISE;
	PF_ADD_CHECKBOX(STR(StrID_Render),
		STR(StrID_Render),
		FALSE,
		0,
		RENDER_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(STITCH_END_ID);

	/* 添加UI事件捕捉层，不使用 */
	if (!err)
	{
		PF_CustomUIInfo			ci;

		AEFX_CLR_STRUCT(ci);

		ci.events = PF_CustomEFlag_LAYER |
			PF_CustomEFlag_COMP;
		ci.comp_ui_width =
			ci.comp_ui_height = 0;

		ci.comp_ui_alignment = PF_UIAlignment_NONE;

		ci.layer_ui_width =
			ci.layer_ui_height = 0;

		ci.layer_ui_alignment = PF_UIAlignment_NONE;

		ci.preview_ui_width =
			ci.preview_ui_height = 0;

		ci.layer_ui_alignment = PF_UIAlignment_NONE;

		err = (*(in_data->inter.register_ui))(in_data->effect_ref, &ci);
	}


	out_data->num_params = UWV_NUM_PARAMS;

	return err;
}

static
PF_Err
FrameSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err				err = PF_Err_NONE;

	PF_FpLong border = params[UWV_INPUT]->u.fs_d.value;
	return err;
}

static PF_Err
MakeParamCopy(
	PF_ParamDef *actual[],	/* >> */
	PF_ParamDef copy[])		/* << */
{
	for (A_short iS = 0; iS < UWV_NUM_PARAMS; ++iS) {
		AEFX_CLR_STRUCT(copy[iS]);	// clean params are important!
	}
	copy[UWV_INPUT]			= *actual[UWV_INPUT];
	copy[UWV_IMPORT_BEG]	= *actual[IMPORT_BEG_ID];
	int i;
	for (i = 0; i < 12; i++) {
		copy[i+ UWV_IMPORT_VIEW_1] = *actual[i+ UWV_IMPORT_VIEW_1];
	}
	// copy[UWV_IMPORT_CORRECTORDER]  = *actual[UWV_IMPORT_CORRECTORDER];

	copy[PERCENTAGE_BEG_ID] = *actual[PERCENTAGE_BEG_ID];
	for (i = 0; i < 12; i++) {
		copy[i + UWV_PERCENTAGE_RATIO_1] = *actual[i + UWV_PERCENTAGE_RATIO_1];
	}

	copy[FRAME_SHIFT_BEG_ID] = *actual[FRAME_SHIFT_BEG_ID];
	for (i = 0; i < 12; i++) {
		copy[i + UWV_FRAME_SHIFT_1] = *actual[i + UWV_FRAME_SHIFT_1];
	}

	copy[STITCH_BEG_ID] = *actual[STITCH_BEG_ID];
	copy[UWV_PROJECTION_METHOD] = *actual[UWV_PROJECTION_METHOD];
	copy[UWV_FOCAL] = *actual[UWV_FOCAL];
	copy[UWV_MULTI_BAND_NUM] = *actual[UWV_MULTI_BAND_NUM];
	copy[UWV_HOMOGRAPHY] = *actual[UWV_HOMOGRAPHY];
	copy[UWV_RENDER] = *actual[UWV_RENDER];
	return PF_Err_NONE;
}

static PF_Err
UserChangedParam(
	PF_InData						*in_data,
	PF_OutData						*out_data,
	PF_ParamDef						*params[],
	PF_LayerDef						*outputP,
	const PF_UserChangedParamExtra	*which_hitP)
{
	PF_Err err = PF_Err_NONE;

	switch (which_hitP->param_index) {

	case UWV_IMPORT_VIEW_1 + 0:
	case UWV_IMPORT_VIEW_1 + 1:
	case UWV_IMPORT_VIEW_1 + 2:
	case UWV_IMPORT_VIEW_1 + 3:
	case UWV_IMPORT_VIEW_1 + 4:
	case UWV_IMPORT_VIEW_1 + 5:
	case UWV_IMPORT_VIEW_1 + 6:
	case UWV_IMPORT_VIEW_1 + 7:
	case UWV_IMPORT_VIEW_1 + 8:
	case UWV_IMPORT_VIEW_1 + 9:
	case UWV_IMPORT_VIEW_1 + 10:
	case UWV_IMPORT_VIEW_1 + 11:
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER; // 每次导入图片都重新渲染
		break;

	case UWV_PERCENTAGE_RATIO_1 + 0:
	case UWV_PERCENTAGE_RATIO_1 + 1:
	case UWV_PERCENTAGE_RATIO_1 + 2:
	case UWV_PERCENTAGE_RATIO_1 + 3:
	case UWV_PERCENTAGE_RATIO_1 + 4:
	case UWV_PERCENTAGE_RATIO_1 + 5:
	case UWV_PERCENTAGE_RATIO_1 + 6:
	case UWV_PERCENTAGE_RATIO_1 + 7:
	case UWV_PERCENTAGE_RATIO_1 + 8:
	case UWV_PERCENTAGE_RATIO_1 + 9:
	case UWV_PERCENTAGE_RATIO_1 + 10:
	case UWV_PERCENTAGE_RATIO_1 + 11:
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER; // 重新渲染
		break;

	case UWV_FRAME_SHIFT_1 + 0:
	case UWV_FRAME_SHIFT_1 + 1:
	case UWV_FRAME_SHIFT_1 + 2:
	case UWV_FRAME_SHIFT_1 + 3:
	case UWV_FRAME_SHIFT_1 + 4:
	case UWV_FRAME_SHIFT_1 + 5:
	case UWV_FRAME_SHIFT_1 + 6:
	case UWV_FRAME_SHIFT_1 + 7:
	case UWV_FRAME_SHIFT_1 + 8:
	case UWV_FRAME_SHIFT_1 + 9:
	case UWV_FRAME_SHIFT_1 + 10:
	case UWV_FRAME_SHIFT_1 + 11:
		// get parameters in render() function
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER; // 重新渲染
		break;

	case UWV_PROJECTION_METHOD:
		//UWV_proj_method = params[UWV_PROJECTION_METHOD]->u.pd.value;
		break;

	case UWV_FOCAL:
		//focal_length = (A_FpShort)params[UWV_FOCAL]->u.fs_d.value;
		break;
	
	case UWV_HOMOGRAPHY:
		flag_calc_homog = true;
		flag_single_Stitcher = true;
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		break;

	case UWV_RENDER:
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		break;

	}
	return err;
}

static PF_Err
UpdateParameterUI(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*outputP)
{
	PF_Err				err = PF_Err_NONE;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_ParamDef param_copy[UWV_NUM_PARAMS];
	ERR(MakeParamCopy(params, param_copy));

	// expand the collapsed items
	if ((param_copy[IMPORT_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)
		&& (param_copy[PERCENTAGE_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)
		&& (param_copy[FRAME_SHIFT_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)
		&& (param_copy[STITCH_BEG_ID].flags & PF_ParamFlag_COLLAPSE_TWIRLY)) {

		param_copy[IMPORT_BEG_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[PERCENTAGE_BEG_ID].flags		&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[FRAME_SHIFT_BEG_ID].flags	&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[STITCH_BEG_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			IMPORT_BEG_ID,
			&param_copy[IMPORT_BEG_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			PERCENTAGE_BEG_ID,
			&param_copy[PERCENTAGE_BEG_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			FRAME_SHIFT_BEG_ID,
			&param_copy[FRAME_SHIFT_BEG_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			STITCH_BEG_ID,
			&param_copy[STITCH_BEG_ID]));
		
		// auto expand RATIO_1 and FRAME_SHIFT_1
		// param_copy[UWV_PERCENTAGE_RATIO_1].flags &= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		// ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
		// 	UWV_PERCENTAGE_RATIO_1_ID,
		// 	&param_copy[UWV_PERCENTAGE_RATIO_1_ID]));
		param_copy[UWV_FRAME_SHIFT_1].flags &= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_FRAME_SHIFT_1_ID,
			&param_copy[UWV_FRAME_SHIFT_1_ID]));
	}

	ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
		UWV_MULTI_BAND_NUM_ID,
		&param_copy[UWV_MULTI_BAND_NUM_ID]));

	if (!err && params[UWV_PROJECTION_METHOD]->u.pd.value == 2) {
		param_copy[UWV_FOCAL].ui_flags &= ~PF_PUI_DISABLED; // 可点击区域变灰 效果
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_FOCAL,
			&param_copy[UWV_FOCAL]));
	}
	else {
		param_copy[UWV_FOCAL].ui_flags |= PF_PUI_DISABLED;
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			UWV_FOCAL,
			&param_copy[UWV_FOCAL]));
	}

	// 按钮变灰 效果
	
	return err;
}

static PF_Err
EwToMat(PF_InData *in_data, PF_EffectWorld *imgEw, Mat32f& imgMat) {
	PF_Err err = PF_Err_NONE;

	int w = imgEw->width,
		h = imgEw->height,
		rb = imgEw->rowbytes; // 每次步进，下一像素

	PF_Pixel8 *pixelP = NULL;
	PF_GET_PIXEL_DATA8(imgEw, NULL, &pixelP);

	for (int y = 0; y < imgMat.rows(); y++) {
		for (int x = 0; x < imgMat.cols(); x++) {
			imgMat.at(y, x, 0) = (float)(pixelP[x].blue)/255.0;
			imgMat.at(y, x, 1) = (float)(pixelP[x].green)/255.0;
			imgMat.at(y, x, 2) = (float)(pixelP[x].red)/255.0;
			//imgMat.at(y, x, 3) = pixelP[x].alpha;
		}
		pixelP = (PF_Pixel8*)((char*)pixelP + rb); //Move to the next line
	}
	return PF_Err_NONE;
}

static PF_Err
MatToEw(PF_InData *in_data, Mat32f *imgMat, PF_EffectWorld *imgE) {
	PF_Err err = PF_Err_NONE;

	int w = imgMat->cols(),
		h = imgMat->rows(),
		rb = imgE->rowbytes;

	PF_Pixel8 *pixelP = NULL;
	PF_GET_PIXEL_DATA8(imgE, NULL, &pixelP);

	for (int y = 0; y < h; y++) { // 注意对应mat32f的格式
		for (int x = 0; x < w; x++) {
			pixelP[x].blue = 255 * imgMat->at(y, x, 0);
			pixelP[x].green = 255 * imgMat->at(y, x, 1);
			pixelP[x].red = 255 * imgMat->at(y, x, 2);
			pixelP[x].alpha = 255;//imgMat.at(y, x, 3);
		}
		pixelP = (PF_Pixel8*)((char*)pixelP + rb);
	}
	return PF_Err_NONE;
}

static PF_Err
GetSrcImgs(PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	vector<PF_EffectWorld *> &src_imgs,//测试vector的传参方式
	int &num_selected_imgs,
	PF_PixelFormat &format,
	A_long selected_current_time)
{
	PF_Err err = PF_Err_NONE;

	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_WorldSuite2 *wsP = NULL;
	ERR(suites.Pica()->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&wsP));

	int i;
	A_long checkout_frame;
	// loop every selecting action, little bit low efficient
	for (i = 0; i < 12; i++)
	{
		// Get Parameters
		frame_shifts[i] = params[UWV_FRAME_SHIFT_1 + i]->u.sd.value;
		lapped_ratios[i] = params[UWV_FRAME_SHIFT_1 + i]->u.sd.value / 100.f;

		// get images
		checkout_frame = (selected_current_time + frame_shifts[i] * in_data->time_step);
		if (checkout_frame < 0)checkout_frame = 0; // 避免越界
		PF_CHECKOUT_PARAM(in_data,
			(i + 2),
			checkout_frame, //+ff_left*in_data->time_step, 
			in_data->time_step,
			in_data->time_scale,
			&checkout);
		if (((&checkout.u.ld)->width != 0) && ((&checkout.u.ld)->width != output->width)) { // 判断当前IMPORT的不是空图片也不是纯色图层（宽度为输出的宽度）
			ERR(wsP->PF_GetPixelFormat((&checkout.u.ld), &format));
			if (src_imgs[i]) { delete src_imgs[i]; src_imgs[i] = NULL; }
			src_imgs[i] = new  PF_EffectWorld;
			ERR(wsP->PF_NewWorld(in_data->effect_ref, (&checkout.u.ld)->width, (&checkout.u.ld)->height, 1, format, (src_imgs[i])));
			PF_COPY(&checkout.u.ld,
				src_imgs[i],
				NULL,
				NULL);
			num_selected_imgs = num_selected_imgs + 1;
			// 预览图各子图的大小
			src_width = src_imgs[i]->width;
			src_height = src_imgs[i]->height;
		}
		PF_CHECKIN_PARAM(in_data, &checkout);
	}
	return PF_Err_NONE;
}

Mat32f CropImg(Mat32f & input_img)
{
	return crop(input_img);
}

void OutputDebugPrintf(const char * strOutputString, ...)
{
	char strBuffer[4096] = { 0 };
	va_list vlArgs;
	va_start(vlArgs, strOutputString);
	_vsnprintf(strBuffer, sizeof(strBuffer) - 1, strOutputString, vlArgs);
	//vsprintf(strBuffer,strOutputString,vlArgs);
	va_end(vlArgs);
	OutputDebugString(strBuffer);
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	/*
	Here include 3 parts:
	<1>. set and get parameters in AE
	<2>. preview result instantly in AE
	<3>. use parameters to stitch frames in AE or AME
	*/
	PF_Err err = PF_Err_NONE;

	// 当前应用（使用）这个插件的layer
	// PF_EffectWorld	*pure_color_layer = &params[UWV_INPUT]->u.ld; // 流程：把别的素材的当前帧当作参数（处理后）写到output上，

	// 公用同一个ew_result_img来显示结果：imported，calc_H，can_render
	PF_EffectWorld* ew_result_img = new PF_EffectWorld;

	// 获取AE工具套件
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_WorldSuite2 *wsP = NULL;
	ERR(suites.Pica()->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&wsP));
	PF_PixelFormat format;


	/* 
	<1>. Get Parameters:
	1. current frame datas
	2. get set parameters
	3. homography or the stitcher class
	*/
	flag_check_render = params[UWV_RENDER]->u.bd.value;

	config::FOCAL_LENGTH = ((A_FpShort)params[UWV_FOCAL]->u.fs_d.value)*7.38;  // 14(wild), 21(medium), 28(narrow) just for gopro 5 black. 

	//config::MULTIBAND = params[UWV_MULTI_BAND_NUM]->u.pd.value - 1; // 多通带融合

	// 1. Get current frame datas
	// 2. Get parameters
	int i;
	int num_selected_imgs = 0;
	// loop every selecting action, little bit low efficient
	GetSrcImgs(in_data, out_data, params, output, src_imgs, num_selected_imgs, format, in_data->current_time);
	// 图片格式转换
	vector<Mat32f> mat_imgs;
	for (i = 0; i < num_selected_imgs; i++) {
		mat_imgs.emplace_back(src_imgs[0]->height, src_imgs[0]->width, 3);
		ERR(EwToMat(in_data, (src_imgs[i]), (mat_imgs[i])));
	}

	// 注意：render会对mat_result_img_ptr赋值，很可能会改变其size，所以在此重置
	if (mat_result_img_ptr) delete mat_result_img_ptr;
	mat_result_img_ptr = new Mat32f(output->height, output->width, 3);
	PF_Rect destArea;

	// 2. Get set parameters
	// from sequence data
	mySequenceData* mySD_P = NULL;
	mySD_P = *(mySequenceData**)out_data->sequence_data; // then use the '*(mySD_P->frame_calc_h_ptr)' to visit the key_frame_calc_h


	/*
	<2>. Preview result instantly in AE:
	1. current imported views
	2. do the H calculation(in fact: just selected key-frame, by sequencing the selected 'current_time') !!!
	3. do the pre-stitch work
	*/
	
	// 1. current imported views
	if (num_selected_imgs && !flag_check_render) {
		out_data->width = (in_data->width) / 4; // ??
		out_data->height = (in_data->height) / 4; // ??
		mesh_width_1 = (output->width) / num_selected_imgs; // 拼接的view的数目
		mesh_height_1 = ((output->height - (src_height*mesh_width_1) / (src_width))) >> 1;
		ERR(wsP->PF_GetPixelFormat((src_imgs[0]), &format));
		PF_EffectWorld* ew_selected_view = new  PF_EffectWorld;
		ERR(wsP->PF_NewWorld(in_data->effect_ref, output->width, output->height, 1, format, (ew_selected_view)));
		PF_COPY(output, ew_selected_view, NULL, NULL);
		for (i = 0; i < num_selected_imgs; i++) {
			if (src_imgs[i] != NULL) {
				destArea = { i*mesh_width_1, mesh_height_1, (i + 1)*mesh_width_1, output->height - mesh_height_1 };
				PF_COPY(src_imgs[i], ew_selected_view, NULL, &destArea);
			}
		}
		// show current imported views
		PF_COPY(ew_selected_view, output, NULL, NULL);
		ERR(wsP->PF_DisposeWorld(in_data->effect_ref, ew_selected_view));
		if (ew_selected_view) { delete ew_selected_view; ew_selected_view = NULL; }
	}



	// 宏函数，生成Stitcher，渲染
#define CALC_HOMOGS \
		if (flag_single_Stitcher) \
		{ \
			flag_single_Stitcher = false; \
			num_selected_imgs = 0; \
			vector<PF_EffectWorld *>key_src_imgs(12, NULL); \
			GetSrcImgs(in_data, out_data, params, output, key_src_imgs, num_selected_imgs, format, *(mySD_P->frame_calc_h_ptr)); \
            *(mySD_P->num_H) = num_selected_imgs; \
			vector<Mat32f> key_mat_imgs; \
			for (i = 0; i < num_selected_imgs; i++) { \
				key_mat_imgs.emplace_back(key_src_imgs[0]->height, key_src_imgs[0]->width, 3); \
				ERR(EwToMat(in_data, (key_src_imgs[i]), (key_mat_imgs[i]))); \
			} \
			Stitcher stitcher_ptr(move(key_mat_imgs)); \
			stitcher_ptr.only_clac_homogras(); \
			if (result_homogras) delete result_homogras; \
			result_homogras = NULL; \
			result_homogras = new vector<Homography>; \
			stitcher_ptr.return_homogras(*result_homogras); \
			if (mySD_P->homogs_ptr) delete mySD_P->homogs_ptr; \
			mySD_P->homogs_ptr = NULL; \
			mySD_P->homogs_ptr = new vector<Homography>; \
			for (auto &i_homo : *result_homogras) \
			{ \
				mySD_P->homogs_ptr->emplace_back(i_homo); \
			} \
		} 

#define BLEND_RESULT *mat_result_img_ptr = blend_YW_ver(mat_imgs, *(mySD_P->homogs_ptr))

#define SHOW_RESULT \
		ERR(wsP->PF_GetPixelFormat((src_imgs[0]), &format)); \
		ERR(wsP->PF_NewWorld(in_data->effect_ref, (*mat_result_img_ptr).width(), (*mat_result_img_ptr).height(), 1, format, (ew_result_img))); \
		ERR(MatToEw(in_data, mat_result_img_ptr, ew_result_img)); \
		if ((ew_result_img->width) / (ew_result_img->height) < (output->width) / (output->height)) \
		{ \
			A_long top_edge = 0; \
			A_long bottom_edge = output->height; \
			A_long left_edge = ((output->width - (output->height*ew_result_img->width) / ew_result_img->height) >> 1); \
			A_long right_edge = ((output->width + (output->height*ew_result_img->width) / ew_result_img->height) >> 1); \
            destArea = { left_edge, top_edge, right_edge, bottom_edge }; \
		} \
		else { \
			A_long left_edge = 0; \
			A_long right_edge = output->width; \
			A_long top_edge = ((output->height - (output->width*ew_result_img->height) / ew_result_img->width) >> 1); \
			A_long bottom_edge = ((output->height + (output->width*ew_result_img->height) / ew_result_img->width) >> 1); \
            destArea = { left_edge, top_edge, right_edge, bottom_edge }; \
		} \
		PF_EffectWorld* zeros_output = new PF_EffectWorld; \
		ERR(wsP->PF_NewWorld(in_data->effect_ref, 3, 3, 1, format, zeros_output)); \
		PF_Pixel8 *pixelP = NULL; \
		PF_GET_PIXEL_DATA8(zeros_output, NULL, &pixelP); \
		for (int y = 0; y < 3; y++) { \
			for (int x = 0; x < 3; x++) { \
				pixelP[x].blue = 0; \
				pixelP[x].green = 0; \
				pixelP[x].red = 0; \
				pixelP[x].alpha = 255; \
			} \
			pixelP = (PF_Pixel8*)((char*)pixelP + zeros_output->rowbytes); \
		} \
		PF_COPY(zeros_output, output, NULL, NULL); \
		ERR(wsP->PF_DisposeWorld(in_data->effect_ref, zeros_output)); \
        if (zeros_output) { delete zeros_output; zeros_output = NULL; } \
		PF_COPY(ew_result_img, output, NULL, &destArea); \
		ERR(wsP->PF_DisposeWorld(in_data->effect_ref, ew_result_img))



	/*
	<2>. Calc H once
	*/
	if (flag_calc_homog && (num_selected_imgs > 1) && (!flag_check_render))
	{
		// 准备序列化：preview render之后，若对当前的H（由key_frame_calc_h计算出）满意，保存项目时会序列化
		*(mySD_P->frame_calc_h_ptr) = (in_data->current_time); // 记录（之后会序列化）选来计算的帧


		// 图片格式转换
		// 初始化CylinderStitcher
		// 取出计算结果Homography，在后续融合中使用
		// 使用CylinderStitcher进行 图像渲染
		// 图像融合:by ikx
		// 缩放、填充区域计算
		// 输出output前，用全黑3*3矩阵覆盖output
		// 读写mat32f时，注意对应mat32f的格式
		
		// calc and get render params
		if (flag_single_Stitcher) 
		{ 
			flag_single_Stitcher = false; 
			num_selected_imgs = 0; 
			vector<PF_EffectWorld *>key_src_imgs(12, NULL); 
			GetSrcImgs(in_data, out_data, params, output, key_src_imgs, num_selected_imgs, format, *(mySD_P->frame_calc_h_ptr)); 
			*(mySD_P->num_H) = num_selected_imgs; 
			vector<Mat32f> key_mat_imgs; 
			for (i = 0; i < num_selected_imgs; i++) {
					key_mat_imgs.emplace_back(key_src_imgs[0]->height, key_src_imgs[0]->width, 3); 
					ERR(EwToMat(in_data, (key_src_imgs[i]), (key_mat_imgs[i]))); 
			} 
			Stitcher stitcher_ptr(move(key_mat_imgs)); 
			stitcher_ptr.only_clac_homogras(); 
			if (result_homogras) delete result_homogras; 
			result_homogras = NULL; 
			result_homogras = new vector<Homography>; 
			stitcher_ptr.return_homogras(*result_homogras); 
			if (mySD_P->homogs_ptr) delete mySD_P->homogs_ptr; 
			mySD_P->homogs_ptr = NULL; 
			mySD_P->homogs_ptr = new vector<Homography>; 
			for (auto &i_homo : *result_homogras) 
			{ 
				mySD_P->homogs_ptr->emplace_back(i_homo); 
			} 

			// return render params
			if (imgs_ranges_minNmax) { delete imgs_ranges_minNmax; imgs_ranges_minNmax = NULL; }
			imgs_ranges_minNmax = new vector<vector<Coor>>;
			if (final_proj_range_idnt) { delete final_proj_range_idnt; final_proj_range_idnt = NULL; }
			final_proj_range_idnt = new vector<Vec2D>;
			if (target_size) { delete target_size; target_size = NULL; }
			target_size = new Coor;
			if (final_resolution) { delete final_resolution; final_resolution = NULL; }
			final_resolution = new Vec2D;
			stitcher_ptr.return_render_params(*imgs_ranges_minNmax, *final_proj_range_idnt, *target_size, *final_resolution);
			// get and sequence params
			if (mySD_P->imgs_ranges_minNmax_ptr) delete mySD_P->imgs_ranges_minNmax_ptr;
			if (mySD_P->final_proj_range_idnt_ptr) delete mySD_P->final_proj_range_idnt_ptr;
			if (mySD_P->target_size_ptr) delete mySD_P->target_size_ptr;
			if (mySD_P->final_resolution_ptr) delete mySD_P->final_resolution_ptr;
			mySD_P->imgs_ranges_minNmax_ptr = NULL; mySD_P->final_proj_range_idnt_ptr = NULL; mySD_P->target_size_ptr = NULL; mySD_P->final_resolution_ptr = NULL;
			mySD_P->imgs_ranges_minNmax_ptr = new vector<vector<Coor>>;
			mySD_P->final_proj_range_idnt_ptr = new vector<Vec2D>;
			mySD_P->target_size_ptr = new Coor;
			mySD_P->final_resolution_ptr = new Vec2D;
			*(mySD_P->target_size_ptr) = *target_size;
			*(mySD_P->final_resolution_ptr) = *final_resolution;
			auto scale_coor_to_img_coor = [&](Vec2D v) {
				v = (v - (*final_proj_range_idnt)[0]) / *final_resolution;
				return Coor(v.x, v.y);
			};
			for (auto &i_imgs_ranges : *imgs_ranges_minNmax)
			{
				vector<Coor> coor_range_{ i_imgs_ranges[0], i_imgs_ranges[1] };
				(*mySD_P->imgs_ranges_minNmax_ptr).emplace_back(coor_range_);
			}
			(*mySD_P->final_proj_range_idnt_ptr).emplace_back((*final_proj_range_idnt)[0]);
			(*mySD_P->final_proj_range_idnt_ptr).emplace_back((*final_proj_range_idnt)[1]);

		}

		// blend result
		//*mat_result_img_ptr = blend_YW_ver(mat_imgs, *(mySD_P->homogs_ptr));

		// TEST blending on CPU seperate from stitcher
		*mat_result_img_ptr = blendingCPU(mat_imgs, *(mySD_P->homogs_ptr), 
			*mySD_P->imgs_ranges_minNmax_ptr, 
			*mySD_P->final_proj_range_idnt_ptr, 
			*(mySD_P->target_size_ptr), 
			*(mySD_P->final_resolution_ptr));

		SHOW_RESULT;
	}


	/*
	<3>. Blend the final frames in AE or AME using parameters
	*/
	if (flag_check_render && (num_selected_imgs > 1))
	{
		if (mySD_P->homogs_ptr->empty()) {
			CALC_HOMOGS;
		}
		if (!(mySD_P->homogs_ptr->empty())) 
		{ 
			*mat_result_img_ptr = blend_YW_ver(mat_imgs, *(mySD_P->homogs_ptr));
			SHOW_RESULT;
		}
		
	}

	// 为了表示此次点击是使能flag_calc_homog，最后再置零
	if (flag_calc_homog) flag_calc_homog = false;

	if (ew_result_img) { delete ew_result_img; ew_result_img = NULL; }

	return err;
}



PF_Err DllExport
EntryPointFunc (
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:

				err = About(in_data,
							out_data,
							params,
							output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:

				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_GLOBAL_SETDOWN:

				err = GlobalSetdown(in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_PARAMS_SETUP:

				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_USER_CHANGED_PARAM:

				err = UserChangedParam(in_data,
					out_data,
					params,
					output,
					reinterpret_cast<const PF_UserChangedParamExtra *>(extra));
				break;
				
			case PF_Cmd_RENDER: // 注：取帧必须要在这里做，否则渲染的时候没法一直是最新帧

				err = Render(	in_data,
								out_data,
								params,
								output);
				break;

			case PF_Cmd_UPDATE_PARAMS_UI:

				err = UpdateParameterUI(in_data,
										out_data,
										params,
										output);
				break;

			case PF_Cmd_SEQUENCE_SETUP:
				err = SequenceSetup(in_data, out_data, params, output);
				break;
			case PF_Cmd_SEQUENCE_SETDOWN:
				err = SequenceSetdown(in_data, out_data, params, output);
				break;
			case PF_Cmd_SEQUENCE_FLATTEN:
				err = SequenceFlatten(in_data, out_data);
				break;
			case PF_Cmd_SEQUENCE_RESETUP:
				err = SequenceResetup(in_data, out_data, params, output);
				break;

		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
};
