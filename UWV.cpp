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

#include <vector> // 标准库
#include "UWV.h"

A_long src_width, src_height, mesh_width, mesh_height; // 图片和网格大小信息
PF_EffectWorld *srcL = NULL, *srcM = NULL, *srcR = NULL;
PF_ParamDef leftbox, rightbox, middlebox;


// I -- fundalmental matrix
// Homo -- transform matrix
PF_FloatMatrix *I_L2M = NULL, *I_R2M = NULL, *Homo_L2M = NULL, *Homo_R2M = NULL;

// Processed Image Formats
A_long width_output;
A_long heigh_output;

static float ratioL = UWV_RATIO_DFLT,
             ratioR = UWV_RATIO_DFLT, 
			 focal_length = UWV_ESTIMATION_DFLT;

static bool correct_order_flag = FALSE,
            correcting_order_flag = FALSE,
            homo_flag = false,
            cal_success = false,
			mosaic_flag = false;

static int shiftL = UWV_FRAME_SHIFT_DFLT,
           shiftR = UWV_FRAME_SHIFT_DFLT,
		   proj_method = UWV_METHOD_DFLT;

static int first_click_img;
static int second_click_img;
static std::vector<PF_EffectWorld **> imgs_prt{&srcL, &srcM, &srcR};
static PF_EffectWorld *dest_back_layer;
static bool ordering_done = false;

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
						   |	PF_OutFlag_PIX_INDEPENDENT //pixel independent
						   |	PF_OutFlag_I_EXPAND_BUFFER // expand the output buffer
						   |	PF_OutFlag_SEND_UPDATE_PARAMS_UI // set to receive PF_Cmd_UPDATE_PARAMS_UI
		                   |    PF_OutFlag_CUSTOM_UI; // set to receive PF_Cmd_Event
	
	// honor the collapse ability of each parameters group
	out_data->out_flags2 = PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;

	I_L2M = new PF_FloatMatrix;
	I_L2M->mat[0][0] = I_L2M->mat[1][1] = I_L2M->mat[2][2] = 1;

	I_R2M = new PF_FloatMatrix;
	I_R2M->mat[0][0] = I_R2M->mat[1][1] = I_R2M->mat[2][2] = 1;

	Homo_L2M = new PF_FloatMatrix;
	Homo_L2M->mat[0][0] = Homo_L2M->mat[1][1] = Homo_L2M->mat[2][2] = 1;

	Homo_R2M = new PF_FloatMatrix;
	Homo_R2M->mat[0][0] = Homo_R2M->mat[1][1] = Homo_R2M->mat[2][2] = 1;
	return PF_Err_NONE;
}

static PF_Err
GlobalSetdown(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	delete I_L2M;
	delete I_R2M;
	delete Homo_L2M;
	delete Homo_R2M;

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

	mySD_P->Homo_L= new PF_FloatMatrix;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 8; j++)
			mySD_P->Homo_L->mat[i][j] = 0;

	mySD_P->Homo_L->mat[0][0] = mySD_P->Homo_L->mat[1][1] = mySD_P->Homo_L->mat[2][2] = 1;

	mySD_P->Homo_R = new PF_FloatMatrix;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 8; j++)
			mySD_P->Homo_R->mat[i][j] = 0;
	mySD_P->Homo_R->mat[0][0] = mySD_P->Homo_R->mat[1][1] = mySD_P->Homo_R->mat[2][2] = 1;

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
		delete mySD_P->Homo_L;
		delete mySD_P->Homo_R;
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
					flatSD_P->L2M[0] = mySD_P->Homo_L->mat[0][0];
					flatSD_P->L2M[1] = mySD_P->Homo_L->mat[0][1];
					flatSD_P->L2M[2] = mySD_P->Homo_L->mat[0][2];
					flatSD_P->L2M[3] = mySD_P->Homo_L->mat[1][0];
					flatSD_P->L2M[4] = mySD_P->Homo_L->mat[1][1];
					flatSD_P->L2M[5] = mySD_P->Homo_L->mat[1][2];
					flatSD_P->L2M[6] = mySD_P->Homo_L->mat[2][0];
					flatSD_P->L2M[7] = mySD_P->Homo_L->mat[2][1];
					flatSD_P->L2M[8] = mySD_P->Homo_L->mat[2][2];

					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];
					flatSD_P->R2M[0] = mySD_P->Homo_R->mat[0][0];

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
					mySD_P->Homo_L = new PF_FloatMatrix;

					mySD_P->Homo_R = new PF_FloatMatrix;

					mySD_P->Homo_L->mat[0][0] = flatSD_P->L2M[0];
					mySD_P->Homo_L->mat[0][1] = flatSD_P->L2M[1];
					mySD_P->Homo_L->mat[0][2] = flatSD_P->L2M[2];
					mySD_P->Homo_L->mat[1][0] = flatSD_P->L2M[3];
					mySD_P->Homo_L->mat[1][1] = flatSD_P->L2M[4];
					mySD_P->Homo_L->mat[1][2] = flatSD_P->L2M[5];
					mySD_P->Homo_L->mat[2][0] = flatSD_P->L2M[6];
					mySD_P->Homo_L->mat[2][1] = flatSD_P->L2M[7];
					mySD_P->Homo_L->mat[2][2] = flatSD_P->L2M[8];


					mySD_P->Homo_R->mat[0][0] = flatSD_P->R2M[0];
					mySD_P->Homo_R->mat[0][1] = flatSD_P->R2M[1];
					mySD_P->Homo_R->mat[0][2] = flatSD_P->R2M[2];
					mySD_P->Homo_R->mat[1][0] = flatSD_P->R2M[3];
					mySD_P->Homo_R->mat[1][1] = flatSD_P->R2M[4];
					mySD_P->Homo_R->mat[1][2] = flatSD_P->R2M[5];
					mySD_P->Homo_R->mat[2][0] = flatSD_P->R2M[6];
					mySD_P->Homo_R->mat[2][1] = flatSD_P->R2M[7];
					mySD_P->Homo_R->mat[2][2] = flatSD_P->R2M[8];

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

	//Import
	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_START_COLLAPSED; // Controls the twirl-state of a topic spinner
	PF_ADD_TOPIC(STR(StrID_Import), IMPORT_BEG_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE	// set to receive PF_Cmd_USER_CHANGED_PARAM
		| PF_ParamFlag_CANNOT_TIME_VARY	// do not vary with time, otherwise will be changed after first applied
		| PF_ParamFlag_START_COLLAPSED;
	PF_ADD_LAYER(STR(StrID_Left), -1, LEFT_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE	// set to receive PF_Cmd_USER_CHANGED_PARAM
		| PF_ParamFlag_CANNOT_TIME_VARY	// do not vary with time, otherwise will be changed after first applied
		| PF_ParamFlag_START_COLLAPSED;
	PF_ADD_LAYER(STR(StrID_Middle), -1, MIDDLE_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE	// set to receive PF_Cmd_USER_CHANGED_PARAM
		| PF_ParamFlag_CANNOT_TIME_VARY	// do not vary with time, otherwise will be changed after first applied
		| PF_ParamFlag_START_COLLAPSED;
	PF_ADD_LAYER(STR(StrID_Right), -1, RIGHT_ID);

	// Correct image order manually
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(STR(StrID_CorrectOrder),
		"Adjust",
		FALSE,
		0,
		CORRECTORDER_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(IMPORT_END_ID);

	//Lapped percentage
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_Percentage), PERCENTAGE_BEG_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE	// set to receive PF_Cmd_USER_CHANGED_PARAM
		| PF_ParamFlag_CANNOT_TIME_VARY	// do not vary with time, otherwise will be changed after first applied
		| PF_ParamFlag_START_COLLAPSED;
	PF_ADD_SLIDER(STR(StrID_Ratio_Left),
		UWV_RATIO_MIN,
		UWV_RATIO_MAX,
		UWV_RATIO_MIN,
		UWV_RATIO_MAX,
		UWV_RATIO_DFLT,
		RATIO_L_ID);

	AEFX_CLR_STRUCT(def);
	def.flags = PF_ParamFlag_SUPERVISE
		| PF_ParamFlag_CANNOT_TIME_VARY
		| PF_ParamFlag_START_COLLAPSED;
	PF_ADD_SLIDER(STR(StrID_Ratio_Right),
		UWV_RATIO_MIN,
		UWV_RATIO_MAX,
		UWV_RATIO_MIN,
		UWV_RATIO_MAX,
		UWV_RATIO_DFLT,
		RATIO_R_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(PERCENTAGE_END_ID);

	//Frame shift
	AEFX_CLR_STRUCT(def);
	def.flags |= PF_ParamFlag_START_COLLAPSED;
	PF_ADD_TOPIC(STR(StrID_Frame_Shift), FRAME_SHIFT_BEG_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER(STR(StrID_Frame_Shift_Left),
		UWV_FRAME_SHIFT_MIN,
		UWV_FRAME_SHIFT_MAX,
		UWV_FRAME_SHIFT_MIN,
		UWV_FRAME_SHIFT_MAX,
		UWV_FRAME_SHIFT_DFLT,
		SHIFT_L_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER(STR(StrID_Frame_Shift_Right),
		UWV_FRAME_SHIFT_MIN,
		UWV_FRAME_SHIFT_MAX,
		UWV_FRAME_SHIFT_MIN,
		UWV_FRAME_SHIFT_MAX,
		UWV_FRAME_SHIFT_DFLT,
		SHIFT_R_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(FRAME_SHIFT_END_ID);

	//SETTING
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
		UWV_ESTIMATION_MIN,
		UWV_ESTIMATION_MAX,
		UWV_ESTIMATION_MIN,
		UWV_ESTIMATION_MAX,
		AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE,
		UWV_ESTIMATION_DFLT,
		UWV_ESTIMATION_PREC,
		0, 1,
		FOCAL_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Homography), "Calculate", 0,
		PF_ParamFlag_SUPERVISE, HOMOGRAPHY_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_BUTTON(STR(StrID_Mosaic), "Stitch",
		PF_PUI_DISABLED,
		PF_ParamFlag_SUPERVISE,
		MOSAIC_ID);

	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(STR(StrID_Preview),
		STR(StrID_Preview),
		TRUE,
		0,
		PREVIEW_ID);

	AEFX_CLR_STRUCT(def);
	PF_END_TOPIC(STITCH_END_ID);

	// 添加UI事件捕捉层？？？
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

static PF_Err EwToMat(PF_InData *in_data, PF_EffectWorld *imgE, Mat32f& imgMat) {
	int w = imgE->width,
		h = imgE->height,
		rb = imgE->rowbytes;

	PF_Pixel8 *pixelP = NULL;
	PF_GET_PIXEL_DATA8(imgE, NULL, &pixelP);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			imgMat.at(x, y, 0) = pixelP[x].blue;
			imgMat.at(x, y, 1) = pixelP[x].green;
			imgMat.at(x, y, 2) = pixelP[x].red;
			imgMat.at(x, y, 3) = pixelP[x].alpha;
		}
		pixelP = (PF_Pixel8*)((char*)pixelP + rb); //Move to the next line
	}
	return PF_Err_NONE;
}

static PF_Err MatToEw(PF_InData *in_data, Mat32f &imgMat, PF_EffectWorld *imgE) {
	int w = imgMat.cols(),
		h = imgMat.rows(),
		rb = imgE->rowbytes;

	PF_Pixel8 *pixelP = NULL;
	PF_GET_PIXEL_DATA8(imgE, NULL, &pixelP);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			pixelP[x].blue = imgMat.at(x, y, 0);
			pixelP[x].green = imgMat.at(x, y, 1);
			pixelP[x].red = imgMat.at(x, y, 2);
			pixelP[x].alpha = imgMat.at(x, y, 3);
		}
		pixelP = (PF_Pixel8*)((char*)pixelP + rb);
	}
	return PF_Err_NONE;
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
	copy[IMPORT_BEG_ID]		= *actual[IMPORT_BEG_ID];
	copy[UWV_IMPORT_LEFT]	= *actual[UWV_IMPORT_LEFT];
	copy[UWV_IMPORT_MIDDLE] = *actual[UWV_IMPORT_MIDDLE];
	copy[UWV_IMPORT_RIGHT]	= *actual[UWV_IMPORT_RIGHT];
	copy[UWV_IMPORT_CORRECTORDER]  = *actual[UWV_IMPORT_CORRECTORDER];

	copy[PERCENTAGE_BEG_ID] = *actual[PERCENTAGE_BEG_ID];
	copy[UWV_RATIO_L] = *actual[UWV_RATIO_L];
	copy[UWV_RATIO_R] = *actual[UWV_RATIO_R];

	copy[FRAME_SHIFT_BEG_ID] = *actual[FRAME_SHIFT_BEG_ID];
	copy[UWV_SHIFT_L] = *actual[UWV_SHIFT_L];
	copy[UWV_SHIFT_R] = *actual[UWV_SHIFT_R];

	copy[STITCH_BEG_ID] = *actual[STITCH_BEG_ID];
	copy[UWV_PROJECTION_METHOD] = *actual[UWV_PROJECTION_METHOD];
	copy[UWV_FOCAL] = *actual[UWV_FOCAL];
	copy[UWV_HOMOGRAPHY] = *actual[UWV_HOMOGRAPHY];
	copy[UWV_MOSAIC] = *actual[UWV_MOSAIC];
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

	case UWV_IMPORT_CORRECTORDER:
		correct_order_flag = params[UWV_IMPORT_CORRECTORDER]->u.bd.value;
		if (correct_order_flag == FALSE) {
			correcting_order_flag = FALSE;
			ordering_done = false;
		}
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		break;

	case UWV_RATIO_L:
		ratioL = params[UWV_RATIO_L]->u.sd.value / 100.f;
		break;
	
	case UWV_RATIO_R:
		ratioR = params[UWV_RATIO_R]->u.sd.value / 100.f;
		break;

	case UWV_SHIFT_L:
		shiftL = params[UWV_SHIFT_L]->u.sd.value;
		break;

	case UWV_SHIFT_R:
		shiftR = params[UWV_SHIFT_R]->u.sd.value;
		break;

	case UWV_PROJECTION_METHOD:
		proj_method = params[UWV_PROJECTION_METHOD]->u.pd.value;
		break;

	case UWV_FOCAL:
		focal_length = (A_FpShort)params[UWV_FOCAL]->u.fs_d.value;
		break;
	
	case UWV_HOMOGRAPHY:
		homo_flag = true;
		out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		break;

	case UWV_MOSAIC:
		mosaic_flag = true;
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
		param_copy[RATIO_L_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[RATIO_R_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[SHIFT_L_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		param_copy[SHIFT_R_ID].flags			&= ~PF_ParamFlag_COLLAPSE_TWIRLY;

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
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			RATIO_L_ID,
			&param_copy[RATIO_L_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			RATIO_R_ID,
			&param_copy[RATIO_R_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			SHIFT_L_ID,
			&param_copy[SHIFT_L_ID]));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
			SHIFT_R_ID,
			&param_copy[SHIFT_R_ID]));
	}

	if (!err && params[UWV_PROJECTION_METHOD]->u.pd.value == 2) {
		param_copy[UWV_FOCAL].ui_flags &= ~PF_PUI_DISABLED;
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
	
return		err;
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;

	// 渲染输出时，在此传入当前帧
	PF_EffectWorld	*pure_color_layer = &params[UWV_INPUT]->u.ld; // 流程：把别的素材的当前帧当作参数（处理后）写到output上，

	// 读取当前将要处理的帧
	// 因此，鼠标调节已经不可用，因为每次rerender会重新checkout，覆盖掉指针调整
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	PF_WorldSuite2 *wsP = NULL;
	PF_PixelFormat format;
	ERR(suites.Pica()->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&wsP));
	{
		// create L Mand R layer
		// get the left image
		PF_CHECKOUT_PARAM(in_data,
			UWV_IMPORT_LEFT,
			(in_data->current_time + shiftL * in_data->time_step),//+ff_left*in_data->time_step, 
			in_data->time_step,
			in_data->time_scale,
			&leftbox);
		ERR(wsP->PF_GetPixelFormat((&leftbox.u.ld), &format));
		srcL = new  PF_EffectWorld;
		ERR(wsP->PF_NewWorld(in_data->effect_ref, (&leftbox.u.ld)->width, (&leftbox.u.ld)->height, 1, format, srcL));
		PF_COPY(&leftbox.u.ld,
			srcL,
			NULL,
			NULL);
		// 预览图各子图的大小
		src_width = (&leftbox.u.ld)->width;
		src_height = (&leftbox.u.ld)->height;
		PF_CHECKIN_PARAM(in_data, &leftbox);

		// get the middle image
		PF_CHECKOUT_PARAM(in_data,
			UWV_IMPORT_MIDDLE,
			(in_data->current_time + shiftL * in_data->time_step),//+ff_left*in_data->time_step, 
			in_data->time_step,
			in_data->time_scale,
			&middlebox);
		ERR(wsP->PF_GetPixelFormat((&middlebox.u.ld), &format));
		srcM = new  PF_EffectWorld;
		ERR(wsP->PF_NewWorld(in_data->effect_ref, (&middlebox.u.ld)->width, (&middlebox.u.ld)->height, 1, format, srcM));
		PF_COPY(&middlebox.u.ld,
			srcM,
			NULL,
			NULL);
		src_width = (&middlebox.u.ld)->width;
		src_height = (&middlebox.u.ld)->height;
		PF_CHECKIN_PARAM(in_data, &middlebox);

		// get the right image
		PF_CHECKOUT_PARAM(in_data,
			UWV_IMPORT_RIGHT,
			(in_data->current_time + shiftR * in_data->time_step),//+ff_right*in_data->time_step, 
			in_data->time_step,
			in_data->time_scale,
			&rightbox);
		ERR(wsP->PF_GetPixelFormat((&rightbox.u.ld), &format));
		srcR = new  PF_EffectWorld;
		ERR(wsP->PF_NewWorld(in_data->effect_ref, (&rightbox.u.ld)->width, (&rightbox.u.ld)->height, 1, format, srcR));
		PF_COPY(&rightbox.u.ld,
			srcR,
			NULL,
			NULL);
		// 预览图各子图的大小
		src_width = (&rightbox.u.ld)->width;
		src_height = (&rightbox.u.ld)->height;
		PF_CHECKIN_PARAM(in_data, &rightbox);
	}


	if (ordering_done) {
		// 拼合当前预览/输出图
		out_data->width = (in_data->width) / 4;
		out_data->height = in_data->height / 4;
		mesh_width = (output->width) / 3; // 拼接的view的数目
		mesh_height = ((output->height - (src_height*mesh_width) / (src_width))) >> 1;
		PF_Rect destArea = { 0, mesh_height, mesh_width,  (src_height*mesh_width) / (src_width)+mesh_height };
		PF_COPY(srcL, output, NULL, &destArea);
		PF_Rect destArea1 = { mesh_width, mesh_height, 2 * mesh_width,  (src_height*mesh_width) / (src_width)+mesh_height };
		PF_COPY(srcM, output, NULL, &destArea1);
		PF_Rect destArea2 = { 2 * mesh_width, mesh_height, 3 * mesh_width,  (src_height*mesh_width) / (src_width)+mesh_height };
		PF_COPY(srcR, output, NULL, &destArea2);
		ordering_done = false; // 将调序后的输出渲染完，重置
	}

	/*
	 * TODO
	 * 0、点击交换图片在纯色图层的位置：偶次、异图 点击
	 * 1、参数预设
	 * 2、参数更新
	 * 3、图像拼接
	 */


	if (proj_method == 1) {	// 1 means default project method PLANAR
		// initial the planar stitcher

	} else {	// project method CYLINDER
		// initial the cylinder stitcher
	}

	// do the h calculation
	if (homo_flag) {
		// 计算单应前，在此作数据转换
		Mat32f matOutput(output->width, output->height, 4);
		Mat32f matM(srcM->width, srcM->height, 4);
		Mat32f matL(srcL->width, srcL->height, 4);
		Mat32f matR(srcR->width, srcR->height, 4);

		ERR(EwToMat(in_data, output, matOutput));
		ERR(EwToMat(in_data, srcM, matM));
		ERR(EwToMat(in_data, srcL, matL));
		ERR(EwToMat(in_data, srcR, matR));
		//cal_success = CalHomoInKeyFrame(matL, matM, matR);
		if (cal_success) {
			PF_STRCPY(out_data->return_msg, "H matrix calculation finished!");
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
		} else {
			PF_STRCPY(out_data->return_msg, "H matrix calculation failed!");
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
		}
		homo_flag = false;
	}

	// do the stitch work
	if (mosaic_flag) {
		
	}


	return err;
}

static PF_Err
AdjustOrder(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_P)
{
	PF_Err		err = PF_Err_NONE;

	A_long mesh_width_click = (in_data->width) / 12; // 缩放以后的合成的width，再除以3得到缩放后网格的大小，用于捕捉鼠标事件

	if (correct_order_flag) {
		if ((event_P->e_type == PF_Event_DO_CLICK)){
			PF_Point mouse_downPt = {0, 0}, cornersPtA[4];
			PF_FixedPoint mouse_layerFiPt = {0, 0};
			mouse_downPt = *(reinterpret_cast<PF_Point*>(&event_P->u.do_click.screen_point));
			//event_P->cbs.frame_to_source(event_P->cbs.refcon, event_P->contextH, &mouse_layerFiPt);

			if (correcting_order_flag) {
				//第二次，点击第二张图片
				second_click_img = (mouse_downPt.h) / mesh_width_click;

				//交换两次点击图片的指针
				PF_EffectWorld * tmp = *(imgs_prt[first_click_img]);
				*(imgs_prt[first_click_img]) = *(imgs_prt[second_click_img]);
				*(imgs_prt[second_click_img]) = tmp;
				
				/* //拼合当前预览图，但是不作任何渲染，在Cmd_EVENT中输入（正常认为的当前图层，实际上不是）也没法操作
				PF_Rect destArea = { 0, mesh_height, mesh_width,  (src_height*mesh_width) / (src_width)+mesh_height };
				PF_COPY(srcL, dest_back_layer, NULL, &destArea);
				PF_Rect destArea1 = { mesh_width, mesh_height, 2 * mesh_width,  (src_height*mesh_width) / (src_width)+mesh_height };
				PF_COPY(srcM, dest_back_layer, NULL, &destArea1);
				PF_Rect destArea2 = { 2 * mesh_width, mesh_height, 3 * mesh_width,  (src_height*mesh_width) / (src_width)+mesh_height };
				PF_COPY(srcR, dest_back_layer, NULL, &destArea2);
				*/

				event_P->evt_out_flags = PF_EO_HANDLED_EVENT;
				out_data->out_flags |= PF_OutFlag_FORCE_RERENDER; // 使强制马上渲染，具体触发未知
				ordering_done = true;

				//为下次调整做准备
				first_click_img = 0;
				second_click_img = 0;
				correcting_order_flag = FALSE;
			}
			else
			{
				//第一次，点击第一张图片
				first_click_img = (mouse_downPt.h)/ mesh_width_click;

				//为第二次点击做准备
				correcting_order_flag = TRUE;
			}
		}
	}

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

			case PF_Cmd_EVENT:

				err = AdjustOrder(	in_data,
									out_data,
									params,
									output,
									reinterpret_cast<PF_EventExtra*>(extra));
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}


